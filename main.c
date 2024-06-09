#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>
#include "util.h"
#include "process.h"

int *pids;
int pids_shmid;

struct sigaction sa;

void init() {
    printf("Initializing\n");
    mkfifo("/tmp/so_fifo", 0666);
    int semid = get_semaphore();
    union semun {
         int val;
         struct semid_ds *buf;
         unsigned short *array;
    } arg;
    arg.val = 1;
    semctl(semid, 0, SETVAL, arg);

    pids_shmid = shmget(IPC_PRIVATE, sizeof(int) * 3, 0666 | IPC_CREAT);
    pids = (int *)shmat(pids_shmid, NULL, 0);
    pids[0] = create_process(run_reading_process, pids_shmid);
    pids[1] = create_process(run_counting_process, pids_shmid);
    pids[2] = create_process(run_writing_process, pids_shmid);
    
}

void sigint_handler(int signum, siginfo_t *info, void *context) {
    if (info->si_pid != pids[1]) return;
    printf("Exiting\n");
    int semid = get_semaphore();
    sem_wait(semid);
    int shmid = get_shared_memory();
    int *shm = (int *)shmat(shmid, NULL, 0);
    *shm = signum;
    shmdt(shm);
    sem_signal(semid);

    kill(pids[0], SIGUSR1);
}

void sigusr1_handler(int signum, siginfo_t *info, void *context) {
    if (info->si_pid != pids[2]) return;
    printf("Exiting\n");
    shmctl(get_shared_memory(), IPC_RMID, NULL);
    shmctl(pids_shmid, IPC_RMID, NULL);
    semctl(get_semaphore(), 0, IPC_RMID);
    unlink("/tmp/so_fifo");
    exit(0);
}

int main() {
    ignore_all_signals();
    
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = *sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    init();

    for (int i = 0; i < 3; i++) {
        wait(NULL);
    }
}