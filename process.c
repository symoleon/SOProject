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

int sibling_pids_shmid;

int reading_process_can_exit = 0;
int reading_process_is_paused = 0;

void reading_process_sigusr1_handler(int signum, siginfo_t *info, void *context) {
    if (info->si_pid != getppid()) return;
    printf("Received SIGUSR1 from the parent\n");
    int semid = get_semaphore();
    int shmid = get_shared_memory(); 
    int *shm = (int *)shmat(shmid, NULL, 0);
    sem_wait(semid);
    int signal = *shm;
    shmdt(shm);
    sem_signal(semid);
    if (signal == SIGINT) {
        reading_process_can_exit = 1;
        return;
    }
    if (signal == SIGTSTP) {
        reading_process_is_paused = 1;
    }
    if (signal == SIGCONT) {
        reading_process_is_paused = 0;
    }
    int *sibling_pids = (int *)shmat(sibling_pids_shmid, NULL, 0);
    kill(sibling_pids[1], SIGUSR1);
    shmdt(sibling_pids);
}

void run_reading_process(int pids_shmid) {
    ignore_all_signals();
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = reading_process_sigusr1_handler;
    sigaction(SIGUSR1, &sa, NULL);
    sibling_pids_shmid = pids_shmid;
    int msgid = get_message_queue();
    struct msgbuf message;
    message.mtype = 1;
    size_t buffer_size = 1024;
    char *buffer = (char *)malloc(buffer_size * sizeof(char));
    int mode = 0;
    while (!reading_process_can_exit) {
        if (mode == 0) {
            printf("Wybierz tryb pracy:\n");
            printf("1. Wprowadzanie danych ręcznie\n");
            printf("2. Wczytywanie danych z pliku\n");
            mode = -1;
        }
        scanf("%d", &mode);
        wait_if_paused(&reading_process_is_paused);
        if (mode == 1 && !reading_process_can_exit) {
            int flag = 0;
            while (1) {
                scanf("%s", buffer);
                wait_if_paused(&reading_process_is_paused);
                if (buffer[0] == '.' && buffer[1] == '\0') break;

                if (buffer[0] == '\0') continue;
                strcpy(message.mtext, buffer);
                msgsnd(msgid, &message, sizeof(message), 0);
                buffer[0] = '\0';
            }
            mode = 0;
        } else if (mode == 2 && !reading_process_can_exit) {
            char filename[128];
            printf("Podaj nazwę pliku: ");
            scanf("%s", filename);
            wait_if_paused(&reading_process_is_paused);
            FILE *file = fopen(filename, "r");
            while (getline(&buffer, &buffer_size, file) != -1) {
                wait_if_paused(&reading_process_is_paused);
                strcpy(message.mtext, buffer);
                msgsnd(msgid, &message, sizeof(message), 0);
            }
            fclose(file);
            mode = 0;
        }
    }
    free(buffer);
    int *sibling_pids = (int *)shmat(sibling_pids_shmid, NULL, 0);
    kill(sibling_pids[1], SIGUSR1);
    shmdt(sibling_pids);
    exit(0);
}

int counting_process_can_exit = 0;
int counting_process_is_paused = 0;

void user_signal_handler(int signum) {
    printf("\nSending signal to the parent\n");
    kill(getppid(), signum);
}

void counting_process_sigusr1_handler(int signum, siginfo_t *info, void *context) {
    int *sibling_pids = (int *) shmat(sibling_pids_shmid, NULL, 0);
    if (info->si_pid != sibling_pids[0]) {
        shmdt(sibling_pids);
        return;
    }
    printf("Received SIGUSR1 from the first process\n");
    int semid = get_semaphore();
    int shmid = get_shared_memory();
    int *shm = (int *)shmat(shmid, NULL, 0);
    sem_wait(semid);
    int signal = *shm;
    shmdt(shm);
    sem_signal(semid);
    if (signal == SIGINT) {
        counting_process_can_exit = 1;
        return;
    }
    if (signal == SIGTSTP) {
        counting_process_is_paused = 1;
    }
    if (signal == SIGCONT) {
        counting_process_is_paused = 0;
    }
    kill(sibling_pids[2], SIGUSR1);
    shmdt(sibling_pids);
}

void run_counting_process(int pids_shmid) {
    ignore_all_signals();
    signal(SIGINT, user_signal_handler);
    signal(SIGTSTP, user_signal_handler);
    signal(SIGCONT, user_signal_handler);
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = counting_process_sigusr1_handler;
    sigaction(SIGUSR1, &sa, NULL);
    sibling_pids_shmid = pids_shmid;
    int msgid = get_message_queue();
    struct msgbuf message;
    int fd = open("/tmp/so_fifo", O_WRONLY);
    int bytes_read;
    
    while (1) {
        wait_if_paused(&counting_process_is_paused);
        bytes_read = msgrcv(msgid, &message, sizeof(message), 1, IPC_NOWAIT);
        if (bytes_read != -1) {
            int count = count_chars(message.mtext);
            write(fd, &count, sizeof(count));
        }
        if (counting_process_can_exit && bytes_read == -1) {
            break;
        }
    }
    msgctl(msgid, IPC_RMID, NULL);
    close(fd);
    int *sibling_pids = (int *)shmat(sibling_pids_shmid, NULL, 0);
    kill(sibling_pids[2], SIGUSR1);
    shmdt(sibling_pids);
    exit(0);
}

int writing_process_can_exit = 0;
int writing_process_is_paused = 0;

void writing_process_sigusr1_handler(int signum, siginfo_t *info, void *context) {
    int *sibling_pids = (int *)shmat(sibling_pids_shmid, NULL, 0);
    if (info->si_pid != sibling_pids[1]) {
        shmdt(sibling_pids);
        return;
    }
    shmdt(sibling_pids);
    printf("Received SIGUSR1 from the second process\n");
    int semid = get_semaphore();
    int shmid = get_shared_memory();
    int *shm = (int *)shmat(shmid, NULL, 0);
    sem_wait(semid);
    int signal = *shm;  
    shmdt(shm);
    sem_signal(semid);
    if (signal == SIGINT) {
        writing_process_can_exit = 1;
        return;
    }
    if (signal == SIGTSTP) {
        writing_process_is_paused = 1;
    }
    if (signal == SIGCONT) {
        writing_process_is_paused = 0;
    }
}

void run_writing_process(int pids_shmid) {
    ignore_all_signals();
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = writing_process_sigusr1_handler;
    sigaction(SIGUSR1, &sa, NULL);
    sibling_pids_shmid = pids_shmid;
    int fd = open("/tmp/so_fifo", O_RDONLY);
    int count;

    while (1) {
        wait_if_paused(&writing_process_is_paused);
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