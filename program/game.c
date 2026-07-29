#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define MAX_TASKS 20
#define NUM_WORKERS 3

typedef struct
{
    void (*task_function)(int);
    int entity_id;
} GameTask;

GameTask task_queue[MAX_TASKS];
int task_count = 0;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

/* ================= TASKS ================= */

void calculate_physics(int id)
{
    printf("[PHYSICS] Calculating gravity and collisions for Falling Rock #%d...\n", id);
    usleep(50000);
}

void update_enemy_ai(int id)
{
    printf("[AI] Enemy #%d is pathfinding toward the player...\n", id);
    usleep(80000);
}

void play_3d_audio(int id)
{
    printf("[AUDIO] Decompressing and panning 3D audio clip #%d...\n", id);
    usleep(30000);
}

/* ================= QUEUE ================= */

void enqueue_task(void (*func)(int), int id)
{
    pthread_mutex_lock(&queue_mutex);

    if (task_count < MAX_TASKS)
    {
        task_queue[task_count].task_function = func;
        task_queue[task_count].entity_id = id;
        task_count++;

        pthread_cond_signal(&queue_cond);
    }
    else
    {
        printf("Task Queue Full!\n");
    }

    pthread_mutex_unlock(&queue_mutex);
}

/* ================= WORKER ================= */

void* worker_thread_loop(void* arg)
{
    int thread_id = *(int*)arg;
    free(arg);

    while (1)
    {
        pthread_mutex_lock(&queue_mutex);

        while (task_count == 0)
        {
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }

        GameTask task = task_queue[0];

        for (int i = 1; i < task_count; i++)
        {
            task_queue[i - 1] = task_queue[i];
        }

        task_count--;

        pthread_mutex_unlock(&queue_mutex);

        printf("CPU Core %d picked up work:\n", thread_id);
        task.task_function(task.entity_id);
    }

    return NULL;
}

/* ================= MAIN ================= */

int main()
{
    printf("=== Starting Game Engine Thread Pool ===\n");

    for (int i = 0; i < NUM_WORKERS; i++)
    {
        pthread_t thread;

        int *id = malloc(sizeof(int));
        *id = i + 1;

        pthread_create(&thread, NULL, worker_thread_loop, id);
        pthread_detach(thread);
    }

    for (int frame = 1; frame <= 3; frame++)
    {
        printf("\n--- STARTING GAME FRAME %d ---\n", frame);

        enqueue_task(calculate_physics, 101);
        enqueue_task(update_enemy_ai, 5);
        enqueue_task(play_3d_audio, 9);
        enqueue_task(calculate_physics, 102);
        enqueue_task(update_enemy_ai, 6);

        usleep(500000);
    }

    printf("\n=== Game Closed. Exiting Engine ===\n");

    sleep(2);

    return 0;
}
