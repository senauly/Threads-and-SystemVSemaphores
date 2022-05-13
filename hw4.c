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
#include <errno.h>
#include <signal.h>         
 
#define NO_EINTR(stmt) while((stmt) < 0 && errno == EINTR);
//global variables
int semid;
int consumerCount;
int N;

/*
union semun {
    int val;   
    struct semid_ds *buf;    
    unsigned short *array;
    struct seminfo *__buf;
}; */

sig_atomic_t interruptHappened = 0;

void sigIntHandler(int sigNum){
    if(sigNum == SIGINT){
        interruptHappened = 1;
    }
}

void* consumer_thread(void* arg);
void* producer_thread(void *arg);
void get_timestamp(char *ts);

int main(int argc, char *argv[]) {
    struct sigaction sigAction;
    memset(&sigAction, 0, sizeof(struct sigaction));
    sigAction.sa_handler = sigIntHandler;
    sigaction(SIGINT, &sigAction, NULL);
    
    setvbuf(stdout, NULL, _IONBF, 0);  //disable buffering

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

    char *input = (char*)malloc(sizeof(char) * 1000);
    strcpy(input, argv[6]);
    
    //check if the file exists
    FILE *fp = fopen(input, "r");
    if (fp == NULL) {
        fprintf(stderr,"Error: File does not exist.\n");
        free(input);
        exit(EXIT_FAILURE);
    }
    fclose(fp);

    //create systemV semaphores for 2 materials
    semid = semget(IPC_PRIVATE, 2, 0666 | IPC_CREAT | IPC_EXCL);
    if (semid == -1) {
        perror("Error, semget failed");
        free(input);
        exit(EXIT_FAILURE);
    }

    //initialize the semaphores
    union semun sem_union;
    sem_union.val = 0;
    
    if (semctl(semid, 0, SETVAL, sem_union) == -1) {
        perror("Error, semctl failed");
        free(input);
        exit(EXIT_FAILURE);
    }

    sem_union.val = 0;
    if (semctl(semid, 0, SETVAL, sem_union) == -1) {
        perror("Error, semctl failed");
        free(input);
        exit(EXIT_FAILURE);
    }

    //create C consumer threads
    consumerCount = atoi(argv[2]);
    N = atoi(argv[4]);
    int **id = (int**)malloc(sizeof(int*) * consumerCount);

    pthread_t *consumers = (pthread_t *) malloc(consumerCount * sizeof(pthread_t));
    for (int i = 0; i < consumerCount; i++) {
        id[i] = (int*)malloc(sizeof(int));
        *(id[i]) = i;
        pthread_create(&consumers[i], NULL, (void *) consumer_thread, (void *) id[i]);
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

    free(consumers);
    free(input);
    //free id 2D array
    for (int i = 0; i < consumerCount; i++) {
        free(id[i]);
    }
    free(id);

    //destroy semaphores
    if (semctl(semid, 0, IPC_RMID, 0) == -1) {
        perror("Error, semctl deletion fail");
        exit(EXIT_FAILURE);
    }
}

void* producer_thread(void *arg){
    //read the file
    char* input = (char*) arg;

    int fp;
    NO_EINTR(fp = open(input, O_RDONLY));
    if (fp == -1) {
        perror("Error, open failed");
        exit(EXIT_FAILURE);
    }

    //read the file one character at a time
    char c;
    struct sembuf sem_buf;
    char ts[26];
    int r;
    int byte_count = 0;
    
    NO_EINTR(r = read(fp, &c, 1));
    while (r > 0) {
        byte_count++;
        get_timestamp(ts);
        fprintf(stdout,"%s Supplier: read from input a ‘%c’. Current amounts: %d x ‘1’, %d ‘2’.\n", ts, c, semctl(semid, 0, GETVAL, 0), semctl(semid, 1, GETVAL, 0));
        //if the character is 1, increment the corresponding semaphore
        if (c == '1') {
            sem_buf.sem_num = 0;
            sem_buf.sem_op = 1;
            sem_buf.sem_flg = 0;
            NO_EINTR(r = semop(semid, &sem_buf, 1));

            if (r < 0) {
                perror("Error, semop failed");
                break;
            }
        }

        //if the character is 2, increment the corresponding semaphore
        if (c == '2') {
            sem_buf.sem_num = 1;
            sem_buf.sem_op = 1;
            sem_buf.sem_flg = 0;
            NO_EINTR(r = semop(semid, &sem_buf, 1));
            if (r < 0) {
                perror("Error, semop failed");
                break;
            }
        }

        get_timestamp(ts);
        fprintf(stdout,"%s Supplier: delivered a ‘%c’. Post-delivery amounts: %d x ‘1’, %d x ‘2’.\n", ts, c, semctl(semid, 0, GETVAL, 0), semctl(semid, 1, GETVAL, 0));  
        NO_EINTR(r = read(fp, &c, 1));
        if(interruptHappened) {
            break;
        }
    }

    //prevent deadlock situation
    if(byte_count < N * consumerCount * 2 || interruptHappened) {
        interruptHappened = 1;
        union semun sem_union;
        sem_union.array = (unsigned short *) malloc(sizeof(unsigned short) * 2);
        sem_union.array[0] = consumerCount;
        sem_union.array[1] = consumerCount;
        
        if(semctl(semid, 0, SETALL, sem_union) < 0) {
            perror("Error, semctl failed");
            free(sem_union.array);
            exit(EXIT_FAILURE);
        }
        
        free(sem_union.array);
    }

    //close the file
    close(fp);
    //terminate thread
    get_timestamp(ts);
    fprintf(stdout,"%s The Supplier has left.\n", ts);
    pthread_exit(NULL);
}

void* consumer_thread(void* arg){
    int *id = (int*) arg;
    struct sembuf sem_buf[2];
    sem_buf[0].sem_num = 0;
    sem_buf[0].sem_op = -1;
    sem_buf[0].sem_flg = 0;
    sem_buf[1].sem_num = 1;
    sem_buf[1].sem_op = -1;
    sem_buf[1].sem_flg = 0;

    char ts[26];
    int r;
        
    for (int i = 0; i < N; i++)
    {
        get_timestamp(ts);
        fprintf(stdout,"%s Consumer-%d at iteration %d (waiting). Current amounts: %d x ‘1’, %d x ‘2’.\n", ts, *id, i, semctl(semid, 0, GETVAL, 0), semctl(semid, 1, GETVAL, 0));
        //read the semaphores if one of them is not available wait
        NO_EINTR(r = semop(semid, sem_buf, 2));
        if (r < 0) {
            perror("Error, semop failed");
            break;
        }

        if(interruptHappened) {
            break;
        }

        get_timestamp(ts);
        fprintf(stdout,"%s Consumer-%d at iteration %d (consumed). Post-consumption amounts: %d x ‘1’, %d x ‘2’.\n", ts, *id, i, semctl(semid, 0, GETVAL, 0), semctl(semid, 1, GETVAL, 0));
    }

    get_timestamp(ts);
    fprintf(stdout,"%s Consumer-%d has left.\n", ts, *id);
    pthread_exit(NULL);
}

void get_timestamp(char *ts){
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(ts, 26, "%Y-%m-%d %H:%M:%S", tm);
}