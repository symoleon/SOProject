#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include "util.h"
#include "process.h"

int create_process(void (*process)(int* pids), int* pids) {
    int pid = fork();
    if (pid == 0) {
        process(pids);
        exit(0);
    }
    return pid;
}

void ignore_all_signals() {
    for (int i = 0; i < 32; i++) {
        signal(i, SIG_IGN);
    }
}

int get_message_queue() {
    key_t key = ftok(".", 1);
    int msgid = msgget(key, 0666 | IPC_CREAT);
    return msgid;
}

int count_chars(char *text) {
    int count = 0;
    while (text[count] != '\0' && text[count] != '\n') {
        count++;
    }
    return count;
}

int get_shared_memory() {
    key_t key = ftok(".", 2);
    int shmid = shmget(key, 1024, 0666 | IPC_CREAT);
    return shmid;
}

int get_semaphore() {
    key_t key = ftok(".", 3);
    int semid = semget(key, 1, 0666 | IPC_CREAT);
    return semid;
}

void sem_wait(int semid) {
    struct sembuf sops;
    sops.sem_num = 0;
    sops.sem_op = -1;
    sops.sem_flg = 0;
    semop(semid, &sops, 1);
}

void sem_signal(int semid) {
    struct sembuf sops;
    sops.sem_num = 0;
    sops.sem_op = 1;
    sops.sem_flg = 0;
    semop(semid, &sops, 1);
}