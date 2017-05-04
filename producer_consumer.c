#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#define BUFF_SIZE 10 

void* producer(void *arg);
void* consumer(void *arg);

int buff[BUFF_SIZE];

int in = 0, out = 0;
sem_t full, empty;
pthread_mutex_t lock;

int main(int argc, char const *argv[]){

	pthread_t pid, cid;
	sem_init(&empty, 0, BUFF_SIZE);
	sem_init(&full, 0, 0);
	pthread_create(&pid,NULL,producer,NULL);
	pthread_create(&cid,NULL,consumer,NULL);
	pthread_join(pid,NULL);
	pthread_join(cid,NULL);

	return 0;
}

void* producer(void *arg){
	int stopProducing = 0;
	int data = 0;
	while(!stopProducing){
		sem_wait(&empty);
		pthread_mutex_lock(&lock);
		/*critical section starts*/
		printf("\nP : %d", data);
		buff[in] = data;

		in = (in + 1) % BUFF_SIZE;
		data = (data + 1) % BUFF_SIZE;

		display_current_buff();
		pthread_mutex_unlock(&lock);
		sem_post(&full);
		sleep(rand() % 5);
	}
	return  NULL;
}

void* consumer(void *arg){
	int stopconsuming = 0;
	while(!stopconsuming){
		sem_wait(&full);
		pthread_mutex_lock(&lock);
		/*critical section starts*/
		printf("\nC : %d",buff[out]);
		
		out = (out + 1) % BUFF_SIZE;
		
		display_current_buff();
		pthread_mutex_unlock(&lock);
		sem_post(&empty);
		sleep(rand() % 5);
	}
	return NULL;
}

void display_current_buff(){

	if (in != out){
		printf("\t|");
		
		if (in > out){
			int i = out;
			while(i<in){
				printf(" %d |", buff[i]);
				i = (i + 1)%BUFF_SIZE;
			}
		}else{
			for(int i=out; i<10; i++)
				printf(" %d |", buff[i]);

			for(int i=0; i<in; i++)
				printf(" %d |", buff[i]);
		}
		printf("\n");
	}else{
		printf("\t|..Empty..|\n");
	}

}