#include<stdio.h>
#include<stdlib.h>

#define SIZE 5

typedef struct
{
    int arr[SIZE];
    int front;
    int rear;
} Queue;
void init(Queue *q)
{
    q->front = -1;
    q->rear = -1;
}
void enqueue(Queue *q, int value)
{
    if ((q->rear + 1) % SIZE == q->front)
    {
        printf("Queue Overflow\n");
        return;
    }

    if (q->front == -1)
    {
        q->front = 0;
        q->rear = 0;
    }
    else
    {
        q->rear = (q->rear + 1) % SIZE;
    }

    q->arr[q->rear] = value;
}
int dequeue(Queue *q)
{
    if (q->front == -1)
    {
        printf("Queue Underflow\n");
        return -1;
    }

    int value = q->arr[q->front];

    if (q->front == q->rear)
    {
        q->front = -1;
        q->rear = -1;
    }
    else
    {
        q->front = (q->front + 1) % SIZE;
    }

    return value;
}
void display(Queue *q)
{
    if (q->front == -1)
    {
        printf("Queue Empty\n");
        return;
    }

    int i = q->front;

    while (1)
    {
        printf("%d ", q->arr[i]);

        if (i == q->rear)
            break;

        i = (i + 1) % SIZE;
    }

    printf("\n");
}
int main()
{
    Queue q;

    init(&q);

    enqueue(&q, 10);
    enqueue(&q, 20);
    enqueue(&q, 30);
    enqueue(&q, 40);
    enqueue(&q, 50);

    display(&q);

    printf("Deleted = %d\n", dequeue(&q));
    printf("Deleted = %d\n", dequeue(&q));

    display(&q);

    enqueue(&q, 60);
    enqueue(&q, 70);

    display(&q);

    return 0;
}
