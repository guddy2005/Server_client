#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_THREADS 5
#define ITERATIONS 1000000

long long shared_counter = 0;
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

void *increment_counter(void *arg) {
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&counter_mutex);
        shared_counter++;
        pthread_mutex_unlock(&counter_mutex);
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];

    
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, increment_counter, NULL) != 0) {
            fprintf(stderr, "Error creating thread %d\n", i);
            return 1;
        }
    }

   
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Final counter value: %lld\n", shared_counter);
    printf("Expected value: %lld\n", (long long)NUM_THREADS * ITERATIONS);

    pthread_mutex_destroy(&counter_mutex);

    return 0;
}
