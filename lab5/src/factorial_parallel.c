#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

typedef struct {
    int start;
    int end;
    int mod;
} ThreadArgs;

long long result = 1;          // общий результат
pthread_mutex_t mut;           // мьютекс для синхронизации

void *factorial_part(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    long long local_result = 1;

    for (int i = args->start; i <= args->end; i++) {
        local_result = (local_result * i) % args->mod;
    }

    // синхронизация общей переменной result
    pthread_mutex_lock(&mut);
    result = (result * local_result) % args->mod;
    pthread_mutex_unlock(&mut);

    return NULL;
}

int main(int argc, char *argv[]) {
    int k = -1, pnum = -1, mod = -1;
    int option_index = 0;
    static struct option long_options[] = {
        {"pnum", required_argument, 0, 0},
        {"mod", required_argument, 0, 0},
        {0, 0, 0, 0}
    };

    while (1) {
        int c = getopt_long(argc, argv, "k:", long_options, &option_index);
        if (c == -1) break;

        switch (c) {
            case 'k':
                k = atoi(optarg);
                break;
            case 0:
                if (option_index == 0)
                    pnum = atoi(optarg);
                else if (option_index == 1)
                    mod = atoi(optarg);
                break;
            default:
                printf("Usage: %s -k <num> --pnum=<threads> --mod=<mod>\n", argv[0]);
                return 1;
        }
    }

    if (k <= 0 || pnum <= 0 || mod <= 0) {
        printf("Invalid arguments!\n");
        printf("Usage: %s -k <num> --pnum=<threads> --mod=<mod>\n", argv[0]);
        return 1;
    }

    pthread_t threads[pnum];
    ThreadArgs args[pnum];

    pthread_mutex_init(&mut, NULL);

    int step = k / pnum;
    int remainder = k % pnum;

    int current = 1;
    for (int i = 0; i < pnum; i++) {
        args[i].start = current;
        args[i].end = current + step - 1;
        if (i == pnum - 1) args[i].end += remainder; // последний поток добирает остаток
        args[i].mod = mod;
        current = args[i].end + 1;

        pthread_create(&threads[i], NULL, factorial_part, &args[i]);
    }

    for (int i = 0; i < pnum; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&mut);

    printf("Factorial(%d) mod %d = %lld\n", k, mod, result);
    return 0;
}
