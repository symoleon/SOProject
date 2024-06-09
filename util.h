struct msgbuf {
    long mtype;
    char mtext[1024];
};
extern int create_process(void (*process)(int* pids), int* pids);
extern void ignore_all_signals();
extern int get_message_queue();
extern int count_chars(char *text);
extern int get_shared_memory();
extern int get_semaphore();
extern void sem_wait(int semid);
extern void sem_signal(int semid);