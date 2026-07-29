#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#define PORT 18080
#define MAX_EVENTS 64
#define BUFFER_SIZE 1024
#define QUEUE_CAPACITY 1024
#define NUM_WORKERS 4

typedef struct {
    int client_sockets[QUEUE_CAPACITY];
    int front;
    int rear;
    int size;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} ThreadQueue;

void init_queue(ThreadQueue *q) {
    q->front = 0;
    q->rear = -1;
    q->size = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void enqueue(ThreadQueue *q, int client_fd) {
    pthread_mutex_lock(&q->lock);
    while (q->size == QUEUE_CAPACITY) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    q->rear = (q->rear + 1) % QUEUE_CAPACITY;
    q->client_sockets[q->rear] = client_fd;
    q->size++;
    pthread_mutex_unlock(&q->lock);
    pthread_cond_signal(&q->not_empty);
}

int dequeue_nonblocking(ThreadQueue *q) {
    pthread_mutex_lock(&q->lock);
    if (q->size == 0) {
        pthread_mutex_unlock(&q->lock);
        return -1;
    }
    int client_fd = q->client_sockets[q->front];
    q->front = (q->front + 1) % QUEUE_CAPACITY;
    q->size--;
    pthread_mutex_unlock(&q->lock);
    pthread_cond_signal(&q->not_full);
    return client_fd;
}

void set_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

void handle_client_read(int client_fd) {
    char buffer[BUFFER_SIZE];

    while (1) {
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';

            char response[BUFFER_SIZE + 64];
            int resp_len = snprintf(response, sizeof(response), "FD %d : %s\n", client_fd, buffer);

            send(client_fd, response, resp_len, 0);
            printf(" %s \n", response);
        } 
        else if (bytes_read == 0) {
            printf("[FD %d] Client disconnected\n", client_fd);
truct pollfd {
    int fd;         // the socket descriptor
    short events;   // bitmap of events we're interested in
    short revents;  // on return, bitmap of events that occurred
};
           close(client_fd);
            break;
        } 
        else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; 
            }
            printf("[FD %d] Recv error, closing connection\n", client_fd);
            close(client_fd);
            break;
        }
    }
}

void *worker_thread_func(void *arg) {
    ThreadQueue *q = (ThreadQueue *)arg;

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");
        pthread_exit(NULL);
    }

    struct epoll_event event;
    struct epoll_event events[MAX_EVENTS];

    while (1) {
        int new_fd;
        while ((new_fd = dequeue_nonblocking(q)) > 0) {
            set_non_blocking(new_fd);

            event.events = EPOLLIN | EPOLLET;
            event.data.fd = new_fd;
            epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_fd, &event);
            printf("Client FD %d \n", new_fd);
        }

        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 10);
        for (int i = 0; i < nfds; i++) {
            if (events[i].events & EPOLLIN) {
                handle_client_read(events[i].data.fd);
            }
        }
    }

    close(epoll_fd);
    return NULL;
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int opt = 1;

    ThreadQueue q;
    init_queue(&q);

    pthread_t threads[NUM_WORKERS];
    for (int i = 0; i < NUM_WORKERS; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread_func, &q) != 0) {
            perror("pthread_create failed");
            exit(EXIT_FAILURE);
        }
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 128) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }

        printf("New client accepted on FD %d\n", client_fd);

        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
        enqueue(&q, client_fd);
    }

    close(server_fd);
    return 0;
}
