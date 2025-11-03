#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include "common.h"

struct Server {
  char ip[255];
  int port;
};

typedef struct {
  struct Server server;
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
  uint64_t result;
} ServerTask;

bool ConvertStringToUI64(const char *str, uint64_t *val) {
  char *end = NULL;
  unsigned long long i = strtoull(str, &end, 10);
  if (errno == ERANGE) {
    fprintf(stderr, "Out of uint64_t range: %s\n", str);
    return false;
  }

  if (errno != 0)
    return false;

  *val = i;
  return true;
}

void *RunServerTask(void *args) {
  ServerTask *task = (ServerTask *)args;
  struct hostent *hostname = gethostbyname(task->server.ip);
  if (hostname == NULL) {
    fprintf(stderr, "gethostbyname failed with %s\n", task->server.ip);
    pthread_exit(NULL);
  }

  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_port = htons(task->server.port);
  server.sin_addr.s_addr = *((unsigned long *)hostname->h_addr);

  int sck = socket(AF_INET, SOCK_STREAM, 0);
  if (sck < 0) {
    fprintf(stderr, "Socket creation failed!\n");
    pthread_exit(NULL);
  }

  if (connect(sck, (struct sockaddr *)&server, sizeof(server)) < 0) {
    fprintf(stderr, "Connection to %s:%d failed\n", task->server.ip, task->server.port);
    close(sck);
    pthread_exit(NULL);
  }

  char msg[sizeof(uint64_t) * 3];
  memcpy(msg, &task->begin, sizeof(uint64_t));
  memcpy(msg + sizeof(uint64_t), &task->end, sizeof(uint64_t));
  memcpy(msg + 2 * sizeof(uint64_t), &task->mod, sizeof(uint64_t));

  if (send(sck, msg, sizeof(msg), 0) < 0) {
    fprintf(stderr, "Send failed\n");
    close(sck);
    pthread_exit(NULL);
  }

  char response[sizeof(uint64_t)];
  if (recv(sck, response, sizeof(response), 0) < 0) {
    fprintf(stderr, "Receive failed\n");
    close(sck);
    pthread_exit(NULL);
  }

  memcpy(&task->result, response, sizeof(uint64_t));
  close(sck);
  pthread_exit(NULL);
}

int main(int argc, char **argv) {
  uint64_t k = -1;
  uint64_t mod = -1;
  char servers_path[255] = {'\0'};

  while (true) {
    int current_optind = optind ? optind : 1;

    static struct option options[] = {{"k", required_argument, 0, 0},
                                      {"mod", required_argument, 0, 0},
                                      {"servers", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);

    if (c == -1)
      break;

    switch (c) {
    case 0: {
      switch (option_index) {
      case 0:
        ConvertStringToUI64(optarg, &k);
        break;
      case 1:
        ConvertStringToUI64(optarg, &mod);
        break;
      case 2:
        memcpy(servers_path, optarg, strlen(optarg));
        break;
      default:
        printf("Index %d is out of options\n", option_index);
      }
    } break;

    case '?':
      printf("Arguments error\n");
      break;
    default:
      fprintf(stderr, "getopt returned character code 0%o?\n", c);
    }
  }

  if (k == -1 || mod == -1 || !strlen(servers_path)) {
    fprintf(stderr, "Using: %s --k 1000 --mod 5 --servers /path/to/file\n",
            argv[0]);
    return 1;
  }

  FILE *file = fopen(servers_path, "r");
  if (!file) {
    perror("Cannot open servers file");
    return 1;
  }

  struct Server *servers = malloc(sizeof(struct Server) * 100);
  int servers_num = 0;
  while (fscanf(file, "%254[^:]:%d\n", servers[servers_num].ip, &servers[servers_num].port) == 2) {
    servers_num++;
  }
  fclose(file);

  if (servers_num == 0) {
    fprintf(stderr, "No servers found in file\n");
    free(servers);
    return 1;
  }

  pthread_t threads[servers_num];
  ServerTask tasks[servers_num];

  uint64_t step = k / servers_num;
  uint64_t current = 1;

  for (int i = 0; i < servers_num; i++) {
    tasks[i].server = servers[i];
    tasks[i].begin = current;
    tasks[i].end = (i == servers_num - 1) ? k : (current + step - 1);
    tasks[i].mod = mod;
    tasks[i].result = 1;
    current = tasks[i].end + 1;

    pthread_create(&threads[i], NULL, RunServerTask, (void *)&tasks[i]);
  }

  uint64_t total = 1;
  for (int i = 0; i < servers_num; i++) {
    pthread_join(threads[i], NULL);
    total = MultModulo(total, tasks[i].result, mod);
  }

  printf("Factorial(%llu) mod %llu = %llu\n", k, mod, total);

  free(servers);
  return 0;
}
