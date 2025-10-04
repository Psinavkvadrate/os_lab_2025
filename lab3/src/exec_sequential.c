#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s --seed <seed> --array_size <size> --pnum <processes>\n", argv[0]);
        return 1;
    }

    pid_t pid = fork();
    
    if (pid < 0) {
        perror("fork failed");
        return 1;
    } else if (pid == 0) {
        printf("Child process: PID = %d, Parent PID = %d\n", getpid(), getppid());
        
        char *new_argv[] = {
            "sequential_min_max",  
            argv[1],               
            argv[2],               
            argv[3],              
            argv[4],              
            argv[5],          
            argv[6],              
            NULL                 
        };

         // Замена процесса
        execv("./sequential_min_max", new_argv);
        
        perror("execv failed");
        exit(1);
    } else {
        printf("Parent process: PID = %d, Child PID = %d\n", getpid(), pid);
        
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status)) {
            printf("Child process exited with status: %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Child process terminated by signal: %d\n", WTERMSIG(status));
        }
        
        printf("Parent process finished\n");
    }
    
    return 0;
}