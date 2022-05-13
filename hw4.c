#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>  //IPC
#include <sys/sem.h>  //semaphore
#include <fcntl.h>   //open
#include <pthread.h>
#include <time.h>
#include <unistd.h> //open, read

//global variables
int semid;
typedef struct consumer{
    int id;
    int N;
} consumer;

void* consumer_thread(void* arg);
void* producer_thread(void *arg);
void get_timestamp(char *ts);

int main(int argc, char *argv[]) {

    //check argument validity ./hw4 -C 10 -N 5 -F inputfilePath
    if (argc != 7) {
        fprintf(stderr,"Error: Invalid number of arguments. It should be like ./hw4 -C 10 -N 5 -F inputfilePath.\n");
        exit(EXIT_FAILURE);
    }

    //check if the arguments are valid case insensitive
    if (strcasecmp(argv[1],"-C") != 0 || strcasecmp(argv[3],"-N") != 0 || strcasecmp(argv[5],"-F") != 0) {
        fprintf(stderr,"Error: Invalid arguments. It should be like ./hw4 -C 10 -N 5 -F inputfilePath.\n");
        exit(EXIT_FAILURE);
    }

    //check if the arguments are valid
    if (atoi(argv[2]) <= 4 || atoi(argv[4]) <= 1) {
        fprintf(stderr,"Error: Invalid arguments. It should be like ./hw4 -C 10 -N 5 -F inputfilePath.\n");
        exit(EXIT_FAILURE);
    }

    char *input = argv[6];
    //check if the file exists
    FILE *fp = fopen(input, "r");
    if (fp == NULL) {
        fprintf(stderr,"Error: File does not exist.\n");
        exit(EXIT_FAILURE);
    }

    //create systemV semaphores for 2 materials
    semid = semget(IPC_PRIVATE, 2, 0666 | IPC_CREAT | IPC_EXCL);
    if (semid == -1) {
        perror("Error, semget failed");
        exit(EXIT_FAILURE);
    }

    //initialize the semaphores
    union semun sem_union;
    sem_union.val = 0;
    if (semctl(semid, 0, SETVAL, sem_union) == -1) {
        perror("Error, semctl failed");
        exit(EXIT_FAILURE);
    }

    sem_union.val = 0;
    if (semctl(semid, 1, SETVAL, sem_union) == -1) {
        perror("Error, semctl failed");
        exit(EXIT_FAILURE);
    }

    //create C consumer threads
    int consumerCount = atoi(argv[2]);
    int N = atoi(argv[4]);
    consumer **cons_struct = (struct consumer **) malloc(sizeof(consumer*) * consumerCount);
    pthread_t *consumers = (pthread_t *) malloc(consumerCount * sizeof(pthread_t));
    for (int i = 0; i < consumerCount; i++) {
        cons_struct[i] = (struct consumer*) malloc(sizeof(consumer));
        cons_struct[i]->id = i;
        cons_struct[i]->N = N;
        pthread_create(&consumers[i], NULL, (void *) consumer_thread, (void *) cons_struct[i]);
    }

    //create a thread for producer
    pthread_t supplier;
    pthread_create(&supplier, NULL, (void *) producer_thread, (void *) input);

    //detach the producer thread
    pthread_detach(supplier);

    //join all the threads
    for (int i = 0; i < consumerCount; i++) {
        pthread_join(consumers[i], NULL);
    }
    pthread_join(supplier, NULL);

    //destroy semaphores
    if (semctl(semid, 0, IPC_RMID, 0) == -1) {
        perror("Error, semctl deletion fail");
        exit(EXIT_FAILURE);
    }

    if (semctl(semid, 1, IPC_RMID, 0) == -1) {
        perror("Error, semctl deletion fail");
        exit(EXIT_FAILURE);
    }

    //free memory
    free(cons_struct);
    free(consumers);
}

void* producer_thread(void *arg){
    //read the file
    char* input = (char*) arg;

    int fp = open(input, O_RDONLY);
    if (fp == -1) {
        perror("Error, open failed");
        exit(EXIT_FAILURE);
    }

    //read the file one character at a time
    char c;
    struct sembuf sem_buf;
    
    while (read(fp, &c, 1) > 0) {
        //if the character is 1, increment the corresponding semaphore
        if (c == '1') {
            sem_buf.sem_num = 0;
            sem_buf.sem_op = 1;
            sem_buf.sem_flg = 0;
            if (semop(semid, &sem_buf, 1) == -1) {
                perror("Error, semop failed");
                exit(EXIT_FAILURE);
            }
        }

        //if the character is 2, increment the corresponding semaphore
        if (c == '2') {
            sem_buf.sem_num = 1;
            sem_buf.sem_op = 1;
            sem_buf.sem_flg = 0;
            if (semop(semid, &sem_buf, 1) == -1) {
                perror("Error, semop failed");
                exit(EXIT_FAILURE);
            }

            //TODO output message Supplier: read from input a ‘1’. Current amounts: 0 x ‘1’, 0 x ‘2’.
        }
    }

    //close the file
    close(fp);

    //terminate thread
    pthread_exit(NULL);
}

void* consumer_thread(void* arg){
    consumer *cons_struct = (consumer*)arg;
    struct sembuf sem_buf[2];
    sem_buf[0].sem_num = 0;
    sem_buf[0].sem_op = -1;
    sem_buf[0].sem_flg = 0;
    sem_buf[1].sem_num = 1;
    sem_buf[1].sem_op = -1;
    sem_buf[1].sem_flg = 0;
        
    for (int i = 0; i < cons_struct->N; i++)
    {
        //read the semaphores if one of them is not available wait
        if (semop(semid, sem_buf, 2) == -1) {
            perror("Error, semop failed");
            exit(EXIT_FAILURE);
        }
    }

    pthread_exit(NULL);
}

void get_timestamp(char *ts){
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(ts, 26, "%Y-%m-%d %H:%M:%S", tm);
}
