#include<stdio.h>
#include<stdlib.h>



struct Node {
	int data;
	struct Node *next;
	};

int main(){
	struct Node *head;
	struct Node *second;
	struct Node *third;

	head = malloc(sizeof (struct Node));
	second = malloc(sizeof(struct Node));
	third =malloc (sizeof(struct Node));
        head->data = 10;
        second->data = 20;
        third->data = 30;

        head->next = second;
        second->next = third;
        third->next = NULL;

        printf("%d\n", head->data);
        printf("%d\n", second->data);
        printf("%d\n", third->data);

        free(head);
        free(second);
        free(third);

    return 0;
}

