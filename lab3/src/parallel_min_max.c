#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

int main(int argc, char **argv) {
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  bool with_files = false;

  while (true) {
    int current_optind = optind ? optind : 1;

    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"by_files", no_argument, 0, 'f'},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "f", options, &option_index);

    if (c == -1) break;

    switch (c) {
      case 0:
        switch (option_index) {
          case 0:
            seed = atoi(optarg);
            if (seed <= 0) {
                printf("seed must be a positive number\n");
                return 1;
            }
            break;
          case 1:
            array_size = atoi(optarg);
            if (array_size <= 0) {
                printf("array_size must be a positive number\n");
                return 1;
            }
            break;
          case 2:
            pnum = atoi(optarg);
            if (pnum <= 0) {
                printf("pnum must be a positive number\n");
                return 1;
            }
            break;
          case 3:
            with_files = true;
            break;

          default:
            printf("Index %d is out of options\n", option_index);
        }
        break;
      case 'f':
        with_files = true;
        break;

      case '?':
        break;

      default:
        printf("getopt returned character code 0%o?\n", c);
    }
  }

  if (optind < argc) {
    printf("Has at least one no option argument\n");
    return 1;
  }

  if (seed == -1 || array_size == -1 || pnum == -1) {
    printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" \n",
           argv[0]);
    return 1;
  }

  int *array = malloc(sizeof(int) * array_size);
  GenerateArray(array, array_size, seed);
  // Массив для pipe
  int pipes[2 * pnum]; 
  char **filenames = NULL;
  
  if (!with_files) {
    // Создание pipe'ов
    for (int i = 0; i < pnum; i++) {
      if (pipe(pipes + 2 * i) < 0) {
        printf("Failed to create pipe for process %d\n", i);
        return 1;
      }
    }
  } else {
    filenames = malloc(pnum * sizeof(char*));
    // Создание уникального имени файла для каждого процесса
    for (int i = 0; i < pnum; i++) {
      filenames[i] = malloc(20 * sizeof(char));
      sprintf(filenames[i], "min_max_%d.txt", i);
    }
  }

  int active_child_processes = 0;

  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  int chunk_size = array_size / pnum;
  int remainder = array_size % pnum;

  for (int i = 0; i < pnum; i++) {
    pid_t child_pid = fork();
    if (child_pid >= 0) {
      active_child_processes += 1;
      if (child_pid == 0) {
        // Вычисление границ
        int start = i * chunk_size;
        int end = (i == pnum - 1) ? array_size : start + chunk_size;
        
        struct MinMax local_min_max = GetMinMax(array, start, end);

        if (with_files) {
          FILE *file = fopen(filenames[i], "w");
          if (file == NULL) {
            printf("Failed to open file %s\n", filenames[i]);
            exit(1);
          }
          fprintf(file, "%d %d", local_min_max.min, local_min_max.max);
          fclose(file);
        } else {
          close(pipes[2 * i]); 
          write(pipes[2 * i + 1], &local_min_max.min, sizeof(int));
          write(pipes[2 * i + 1], &local_min_max.max, sizeof(int));
          close(pipes[2 * i + 1]); 
        }
        /// Освобождение ресурсов
        free(array);
        if (with_files) {
          for (int j = 0; j < pnum; j++) {
            free(filenames[j]);
          }
          free(filenames);
        }
        exit(0);
      }

    } else {
      printf("Fork failed!\n");
      return 1;
    }
  }

  while (active_child_processes > 0) {
    wait(NULL);
    active_child_processes -= 1;
  }

  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;
  // чтение всех резульатов
  for (int i = 0; i < pnum; i++) {
    int min = INT_MAX;
    int max = INT_MIN;

    if (with_files) {
      FILE *file = fopen(filenames[i], "r");
      if (file == NULL) {
        printf("Failed to open file %s\n", filenames[i]);
        continue;
      }
      fscanf(file, "%d %d", &min, &max);
      fclose(file);
      
      remove(filenames[i]);
    } else {
      close(pipes[2 * i + 1]); 
      read(pipes[2 * i], &min, sizeof(int));
      read(pipes[2 * i], &max, sizeof(int));
      close(pipes[2 * i]); 
    }

    if (min < min_max.min) min_max.min = min;
    if (max > min_max.max) min_max.max = max;
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);
  if (with_files && filenames != NULL) {
    for (int i = 0; i < pnum; i++) {
      free(filenames[i]);
    }
    free(filenames);
  }

  printf("Min: %d\n", min_max.min);
  printf("Max: %d\n", min_max.max);
  printf("Elapsed time: %fms\n", elapsed_time);
  printf("Array size: %d\n", array_size);
  printf("Processes number: %d\n", pnum);
  printf("Seed: %d\n", seed);
  fflush(NULL);
  return 0;
}