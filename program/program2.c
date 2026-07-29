#include <stdio.h>
#include <pthread.h>
#include <unistd.h>


#define MAX_ITEMS 5
int packet_buffer[MAX_ITEMS];
int count =0;

pthread_mutex_t lock =PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void* packet_receiver(void* arg){
	for(int i=1; i<=MAX_ITEMS; i++){
		pthread_mutex_lock(&lock);
		packet_buffer[count++] =i*10;
		printf("[receiver Thread] Received Packed id: %d\n",i*10);
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&lock);
		sleep(1);
	}
	return NULL;
}

void* packet_processor(void* arg){
	for(int i=0;i<MAX_ITEMS;i++){
		pthread_mutex_lock(&lock);
		while(count==0){
			pthread_cond_wait(&cond,&lock);
		}

		int pkt = packet_buffer[--count];
		printf("[processor thread] processed packet ID:%d\n",pkt);
		pthread_mutex_unlock(&lock);
	}
	return NULL;
}
int main(){
	pthread_t t1,t2;
	  pthread_create(&t1, NULL, packet_receiver, NULL);
    pthread_create(&t2, NULL, packet_processor, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);

    return 0;
}









































