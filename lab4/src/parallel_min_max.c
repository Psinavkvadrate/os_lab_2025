#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

static pid_t *child_pids = NULL;
static int num_children = 0;
static volatile sig_atomic_t timeout_triggered = 0;
//---------------NEW-----------------
void timeout_handler(int sig) {
    if (sig == SIGALRM) {
        timeout_triggered = 1;
        printf("Timeout reached. Killing child processes...\n");
        for (int i = 0; i < num_children; i++) {
            if (child_pids[i] > 0) {
                kill(child_pids[i], SIGKILL);
            }
        }
    }
}

int main(int argc, char **argv) {
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  int timeout = 0;
  bool with_files = false;

  while (true) {
    int current_optind = optind ? optind : 1;

    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"by_files", no_argument, 0, 'f'},
                                      {"timeout", required_argument, 0, 0},
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
          case 4:
            timeout = atoi(optarg);
            if (timeout <= 0) {
                printf("timeout must be a positive number\n");
                return 1;
            }
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
    printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" [--timeout \"seconds\"]\n",
           argv[0]);
    return 1;
  }

  child_pids = malloc(pnum * sizeof(pid_t));
  if (child_pids == NULL) {
    printf("Failed to allocate memory for child_pids\n");
    return 1;
  }
  num_children = pnum;
  for (int i = 0; i < pnum; i++) {
      child_pids[i] = 0;
  }

  //---------------NEW-----------------
  if (timeout > 0) {
      struct sigaction sa;
      sa.sa_handler = timeout_handler;
      sigemptyset(&sa.sa_mask);
      sa.sa_flags = 0;
      if (sigaction(SIGALRM, &sa, NULL) == -1) {
          perror("sigaction");
          free(child_pids);
          return 1;
      }
      alarm(timeout);
  }

  int *array = malloc(sizeof(int) * array_size);
  if (array == NULL) {
    printf("Failed to allocate memory for array\n");
    free(child_pids);
    return 1;
  }
  GenerateArray(array, array_size, seed);
  
  int *pipes = NULL;
  char **filenames = NULL;
  
  if (!with_files) {
    pipes = malloc(2 * pnum * sizeof(int));
    if (pipes == NULL) {
        printf("Failed to allocate memory for pipes\n");
        free(array);
        free(child_pids);
        return 1;
    }
    
    // Создание pipe
    for (int i = 0; i < pnum; i++) {
      if (pipe(pipes + 2 * i) < 0) {
        printf("Failed to create pipe for process %d\n", i);
        free(pipes);
        free(array);
        free(child_pids);
        return 1;
      }
    }
  } else {
    filenames = malloc(pnum * sizeof(char*));
    if (filenames == NULL) {
        printf("Failed to allocate memory for filenames\n");
        free(array);
        free(child_pids);
        return 1;
    }
    
    // Создание уникального имени файла для каждого процесса
    for (int i = 0; i < pnum; i++) {
      filenames[i] = malloc(20 * sizeof(char));
      if (filenames[i] == NULL) {
          printf("Failed to allocate memory for filename %d\n", i);
          // Освобождаем уже выделенную память
          for (int j = 0; j < i; j++) {
              free(filenames[j]);
          }
          free(filenames);
          free(array);
          free(child_pids);
          return 1;
      }
      sprintf(filenames[i], "min_max_%d.txt", i);
    }
  }

  int active_child_processes = 0;

  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  int chunk_size = array_size / pnum;

  for (int i = 0; i < pnum; i++) {
    pid_t child_pid = fork();
    if (child_pid >= 0) {
      active_child_processes += 1;
      child_pids[i] = child_pid;
      
      if (child_pid == 0) {
        // Дочерний процесс
        if (timeout > 0) {
            alarm(0);
        }

        if (child_pids != NULL) {
            free(child_pids);
            child_pids = NULL;
        }

        int start = i * chunk_size;
        int end = (i == pnum - 1) ? array_size : start + chunk_size;
        
        struct MinMax local_min_max = GetMinMax(array, start, end);

        if (with_files) {
          FILE *file = fopen(filenames[i], "w");
          if (file == NULL) {
            printf("Failed to open file %s\n", filenames[i]);
            free(array);
            exit(1);
          }
          fprintf(file, "%d %d", local_min_max.min, local_min_max.max);
          fclose(file);
        } else {
          close(pipes[2 * i]); // Закрываем читающий конец
          write(pipes[2 * i + 1], &local_min_max.min, sizeof(int));
          write(pipes[2 * i + 1], &local_min_max.max, sizeof(int));
          close(pipes[2 * i + 1]); // Закрываем записывающий конец
        }
        
        free(array);
        exit(0);
      }

    } else {
      printf("Fork failed!\n");

      if (!with_files && pipes != NULL) free(pipes);
      if (with_files && filenames != NULL) {
        for (int j = 0; j < pnum; j++) {
          if (filenames[j] != NULL) free(filenames[j]);
        }
        free(filenames);
      }
      free(array);
      free(child_pids);
      return 1;
    }
  }

  free(array);

  // //---------------NEW----------------- Ожидание 
  int status;
  pid_t finished_pid;
  while (active_child_processes > 0) {
      finished_pid = wait(&status);
      if (finished_pid > 0) {
          active_child_processes -= 1;
          
          for (int i = 0; i < pnum; i++) {
              if (child_pids[i] == finished_pid) {
                  child_pids[i] = 0;
                  break;
              }
          }
      }
  }

  //---------------NEW-----------------
  if (timeout > 0 && !timeout_triggered) {
      alarm(0);
  }

  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;
  
  // Чтение если не было таймаута
  if (!timeout_triggered) {
    for (int i = 0; i < pnum; i++) {
      int min = INT_MAX;
      int max = INT_MIN;

      if (with_files) {
        FILE *file = fopen(filenames[i], "r");
        if (file == NULL) {
          printf("Failed to open file %s\n", filenames[i]);
          continue;
        }
        if (fscanf(file, "%d %d", &min, &max) == 2) {
          if (min < min_max.min) min_max.min = min;
          if (max > min_max.max) min_max.max = max;
        }
        fclose(file);
        remove(filenames[i]);
      } else {
        close(pipes[2 * i + 1]); // Закрываем записывающий конец
        if (read(pipes[2 * i], &min, sizeof(int)) > 0 && 
            read(pipes[2 * i], &max, sizeof(int)) > 0) {
          if (min < min_max.min) min_max.min = min;
          if (max > min_max.max) min_max.max = max;
        }
        close(pipes[2 * i]); // Закрываем читающий конец
      }
    }
  } else {
    printf("Timeout occurred, results may be incomplete\n");
    min_max.min = 0;
    min_max.max = 0;
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  if (!with_files && pipes != NULL) {
    free(pipes);
  }
  
  if (with_files && filenames != NULL) {
    for (int i = 0; i < pnum; i++) {
      if (filenames[i] != NULL) {
        free(filenames[i]);
      }
    }
    free(filenames);
  }
  
  if (child_pids != NULL) {
    free(child_pids);
    child_pids = NULL;
  }

  if (!timeout_triggered) {
    printf("Min: %d\n", min_max.min);
    printf("Max: %d\n", min_max.max);
  }
  printf("Elapsed time: %fms\n", elapsed_time);
  //---------------NEW-----------------
  if (timeout > 0) {
    if (timeout_triggered) {
      printf("Timeout: %d seconds (TIMEOUT REACHED)\n", timeout);
    } else {
      printf("Timeout: %d seconds\n", timeout);
    }
  }
  printf("Array size: %d\n", array_size);
  printf("Processes number: %d\n", pnum);
  printf("Seed: %d\n", seed);
  fflush(stdout);
  
  return timeout_triggered ? 1 : 0;
}