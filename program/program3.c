#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_THREADS 5
#define ITERATIONS 1000000

long long shared_counter = 0;

void *increment_counter(void *arg) {
    for (int i = 0; i < ITERATIONS; i++) {
        shared_counter++;
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];

    
         for (int i = 0; i < NUM_THREADS; i++) {
             if(pthread_create(&threads[i], NULL,increment_counter,NULL)!= 0) 			{
                  fprintf(stderr, "Error creating thread %d\n", i);
                  return 1;
                  }
                                                     }
    
// Wait for threads to complete
         for (int i = 0; i < NUM_THREADS; i++) {
                  pthread_join(threads[i],NULL);                                                          }
    
                                                                             printf("Final counter value: %lld\n", shared_counter);
                                                                                 printf("Expected value: %lld\n", (long long)NUM_THREADS * ITERATIONS);
    
                                                                                     return 0;
                                                                                     }
