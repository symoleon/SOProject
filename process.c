#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include "process.h"
#include "util.h"

int* siblings_pids;

int reading_process_can_exit = 0;

void reading_process_sigusr1_handler(int signum, siginfo_t *info, void *context) {
    if (info->si_pid != getppid()) return;
    printf("Received SIGINT from the parent\n");
    int semid = get_semaphore();
    int shmid = get_shared_memory(); 
    int *shm = (int *)shmat(shmid, NULL, 0);
    sem_wait(semid);
    int signal = *shm;
    shmdt(shm);
    sem_signal(semid);
    if (signal == SIGINT) {
        reading_process_can_exit = 1;
    }
}

void run_reading_process(int *pids) {
    ignore_all_signals();
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = reading_process_sigusr1_handler;
    sigaction(SIGUSR1, &sa, NULL);
    siblings_pids = pids;
    int msgid = get_message_queue();
    struct msgbuf message;
    message.mtype = 1;
    size_t buffer_size = 1024;
    char *buffer = (char *)malloc(buffer_size * sizeof(char));
    while (!reading_process_can_exit) {
        printf("Wybierz tryb pracy:\n");
        printf("1. Wprowadzanie danych ręcznie\n");
        printf("2. Wczytywanie danych z pliku\n");
        int mode;
        scanf("%d", &mode);
        if (mode == 1 && !reading_process_can_exit) {
            while (1) {
                scanf("%s", buffer);
                if (buffer[0] == '.' && buffer[1] == '\0') break;

                strcpy(message.mtext, buffer);
                msgsnd(msgid, &message, sizeof(message), 0);
            }
        } else if (mode == 2 && !reading_process_can_exit) {
            char filename[128];
            printf("Podaj nazwę pliku: ");
            scanf("%s", filename);
            FILE *file = fopen(filename, "r");
            while (getline(&buffer, &buffer_size, file) != -1) {
                strcpy(message.mtext, buffer);
                msgsnd(msgid, &message, sizeof(message), 0);
            }
            fclose(file);
        }
    }
    free(buffer);
    kill(pids[1], SIGUSR1);
    exit(0);
}

int counting_process_can_exit = 0;

void user_sigint_handler(int signum) {
    printf("\nSending SIGINT to the parent\n");
    kill(getppid(), SIGINT);
}

void counting_process_sigusr1_handler(int signum, siginfo_t *info, void *context) {
    if (info->si_pid != siblings_pids[0]) return;
    printf("Received SIGUSR1 from the first process\n");
    int semid = get_semaphore();
    int shmid = get_shared_memory();
    int *shm = (int *)shmat(shmid, NULL, 0);
    sem_wait(semid);
    int signal = *shm;
    shmdt(shm);
    sem_signal(semid);
    if (signal == SIGUSR1) {
        counting_process_can_exit = 1;
    }
    printf("Counting process can exit\n");
}

void run_counting_process(int *pids) {
    ignore_all_signals();
    signal(SIGINT, user_sigint_handler);
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = counting_process_sigusr1_handler;
    sigaction(SIGUSR1, &sa, NULL);
    siblings_pids = pids;
    int msgid = get_message_queue();
    struct msgbuf message;
    int fd = open("/tmp/so_fifo", O_WRONLY);
    int bytes_read;
    
    while (1) {
        bytes_read = msgrcv(msgid, &message, sizeof(message), 1, IPC_NOWAIT);
        if (bytes_read != -1) {
            int count = count_chars(message.mtext);
            write(fd, &count, sizeof(count));
        }
        if (counting_process_can_exit && bytes_read == -1) {
            break;
        }
    }
    printf("Exited the loop\n");
    msgctl(msgid, IPC_RMID, NULL);
    close(fd);
    kill(pids[2], SIGUSR1);
    exit(0);
}

int writing_process_can_exit = 0;

void writing_process_sigusr1_handler(int signum, siginfo_t *info, void *context) {
    if (info->si_pid != siblings_pids[1]) return;
    printf("Received SIGUSR1 from the second process\n");
    int semid = get_semaphore();
    int shmid = get_shared_memory();
    int *shm = (int *)shmat(shmid, NULL, 0);
    sem_wait(semid);
    int signal = *shm;  
    shmdt(shm);
    sem_signal(semid);
    if (signal == SIGUSR1) {
        writing_process_can_exit = 1;
    }
}

void run_writing_process(int *pids) {
    ignore_all_signals();
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = writing_process_sigusr1_handler;
    sigaction(SIGUSR1, &sa, NULL);
    siblings_pids = pids;
    int fd = open("/tmp/so_fifo", O_RDONLY);
    int count;

    while (1) {
        int bytes_read = read(fd, &count, sizeof(count));
        if (bytes_read > 0) {
            printf("Liczba znaków: %d\n", count);
        }
        if (writing_process_can_exit && bytes_read <= 0) {
            break;
        }
    }
    close(fd);
    kill(getppid(), SIGUSR1);
    exit(0);
}