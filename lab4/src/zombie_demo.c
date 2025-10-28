#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {    
    pid_t pid = fork();
    
    if (pid == 0) {
        // Дочерний
        printf("Дочерний процесс: PID=%d\n", getpid());
        printf("Дочерний процесс завершается...\n");
        exit(0);
    } else {
        // Родительский
        printf("Родительский процесс: PID=%d\n", getpid());
        printf("Дочерний PID: %d\n", pid);
        
        printf("Родительский процесс спит 30 секунд...\n");
        sleep(30); 
        
        printf("Родительский процесс завершается\n");
    }
    
    return 0;
}