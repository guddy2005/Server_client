#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<pthread.h>
#include <sys/socket.h>   
#include <unistd.h>



#define BUFFER_SIZE 1024
#define MAX_QUEUE 100
#define PORT 45501
#define WORKER_THREADS 3

typedef struct{

int client_fd[MAX_QUEUE];
int front;
int rear;
int count;
pthread_mutex_t mutex;
pthread_cond_t cond;

}queue_t;
typedef struct
{
    int server_fd;
    queue_t *queue;

} thread_data_t;


void queue_init(queue_t *q)
{
q->front =0;
q->rear =0;
q->count =0;

pthread_mutex_init(&q->mutex,NULL);
pthread_cond_init(&q->cond,NULL);
}

void enqueue(queue_t *q,int fd)
{
    pthread_mutex_lock(&q->mutex);
    q->client_fd[q->rear] =fd;
    q->rear=(q->rear +1) % MAX_QUEUE;
    q->count++;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);
}

int dequeue(queue_t *q)
{
    pthread_mutex_lock(&q->mutex);
    while(q->count==0)
       pthread_cond_wait(&q->cond,&q->mutex);
    int fd =q->client_fd[q->front];
    q->front=(q->front +1 )% MAX_QUEUE;
    q->count--;
    pthread_mutex_unlock(&q->mutex);
    return fd;

}
void *accept_thread(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;

    int client_fd;

    struct sockaddr_in client_addr;

    socklen_t client_len = sizeof(client_addr);

    while(1)
    {
        client_fd = accept(data->server_fd,
                           (struct sockaddr *)&client_addr,
                           &client_len);

        if(client_fd < 0)
        {
            perror("accept");
            continue;
        }

        printf("Client Connected : %d\n", client_fd);

        enqueue(data->queue, client_fd);
    }

    return NULL;
}
void *worker_thread(void *arg)
{
    queue_t *queue = (queue_t *)arg;

    int client_fd;
    int bytes;

    char buffer[BUFFER_SIZE];

    while(1)
    {
        client_fd = dequeue(queue);

        printf("Worker handling client : %d\n", client_fd);

        while(1)
        {
            bytes = recv(client_fd,
                         buffer,
                         BUFFER_SIZE - 1,
                         0);

            if(bytes <= 0)
            {
                printf("Client %d Disconnected\n", client_fd);
                close(client_fd);
                break;
            }

            buffer[bytes] = '\0';

            printf("Received : %s\n", buffer);

            send(client_fd,
                 buffer,
                 strlen(buffer),
                 0);
        }
    }

    return NULL;
}  
int main()
{
    int server_fd;
    int opt = 1;

    struct sockaddr_in server_addr;

    pthread_t accept_tid;
    pthread_t worker_tid[WORKER_THREADS];

    queue_t queue;

    thread_data_t data;

   
    queue_init(&queue);

  
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if(server_fd < 0)
    {
        perror("socket");
        return -1;
    }

    
    setsockopt(server_fd,
               SOL_SOCKET,
               SO_REUSEADDR,
               &opt,
               sizeof(opt));


    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    
    if(bind(server_fd,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)) < 0)
    {
        perror("bind");
        return -1;
    }

    
    if(listen(server_fd, 10) < 0)
    {
        perror("listen");
        return -1;
    }

    printf("Server Started...\n");

    /* Thread Data */
    data.server_fd = server_fd;
    data.queue = &queue;

    /* Accept Thread */
    pthread_create(&accept_tid,
                   NULL,
                   accept_thread,
                   &data);

    /* Worker Threads */
    for(int i = 0; i < WORKER_THREADS; i++)
    {
        pthread_create(&worker_tid[i],
                       NULL,
                       worker_thread,
                       &queue);
    }

    /* Wait */
    pthread_join(accept_tid, NULL);

    for(int i = 0; i < WORKER_THREADS; i++)
    {
        pthread_join(worker_tid[i], NULL);
    }

    close(server_fd);

    return 0;
}



































