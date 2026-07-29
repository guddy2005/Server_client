#include<stdio.h>
#include<stdlib.h>

#define SIZE 5

typedef struct{
	int arr[SIZE];
	int front;
	int rear;
}Queue;

void init(Queue *q)
{
q->front =-1;
q->rear =-1;

}

void display(Queue *q)
{
    if (q->front == -1)
    {
        printf("Queue is Empty\n");
        return;
    }

    for (int i = q->front; i <= q->rear; i++)
    {
        printf("%d ", q->arr[i]);
    }

    printf("\n");
}
void enqueue(Queue *q,int value)
{
    if(q->rear==SIZE-1)
    {
        printf("Overflow\n");
        return;
    }

    if(q->front==-1)
        q->front=0;

    q->rear++;

    q->arr[q->rear]=value;
}
int dequeue(Queue *q)
{
    if(q->front==-1)
    {
        printf("Underflow\n");
        return -1;
    }

    int value=q->arr[q->front];

    if(q->front==q->rear)
    {
        q->front=-1;
        q->rear=-1;
    }
    else
    {
        q->front++;
    }

    return value;
}




int main()
{
    Queue q;

    init(&q);

    enqueue(&q, 10);
    enqueue(&q, 20);
    enqueue(&q, 30);

    printf("Queue after enqueue:\n");
    display(&q);

    printf("Deleted: %d\n", dequeue(&q));

    printf("Queue after dequeue:\n");
    display(&q);

    enqueue(&q, 40);
    enqueue(&q, 50);

    printf("Queue after more enqueue:\n");
    display(&q);

    return 0;
}
