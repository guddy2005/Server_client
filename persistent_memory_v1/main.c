#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAX_STR  50


typedef struct {
	char key[MAX_STR];
	char value[MAX_STR];
        int is_empty;
}storage;


int main(){
 const char *filepath ="data.dat";

 int fd =open(filepath,O_RDWR | O_CREAT,0666);
 if (fd <0){
	perror("Failed to open storage file");
	return 1;
 }

 if (ftruncate(fd,sizeof(storage))== -1){
	perror("Failed to allocate file size ");
	close(fd);
	return 1;
 }
 
 storage *store =(storage *)mmap(
 	NULL,sizeof(storage),PROT_READ | PROT_WRITE,MAP_SHARED, fd, 0);

 if (store == MAP_FAILED){
 	perror("memory mappng failed");
	close(fd);
	return 1;
 }


 if (store -> is_empty != 0 && store-> is_empty !=1){
	store->is_empty=1;
 }

 int choice;
    printf("=== PERSISTENT KEY-VALUE WORKSHOP ===\n");
    printf("1. View Current Data\n");
    printf("2. Save New Key-Value\n");
    printf("3. Exit Program\n");
    printf("Enter choice: ");
    scanf("%d", &choice);

 getchar();
  if (choice == 1) {
        if (store->is_empty) {
            printf("\n[Result] Storage is currently empty!\n");
        } else {
            printf("\n[Result] Found Saved Data:\n");
            printf("Key:   %s\n", store->key);
            printf("Value: %s\n", store->value);
        }
    } 

 else if (choice == 2) {
        printf("Enter Key (Max %d chars): ", MAX_STR - 1);
        fgets(store->key, MAX_STR, stdin);
        store->key[strcspn(store->key, "\n")] = 0; 

        printf("Enter Value (Max %d chars): ", MAX_STR - 1);
        fgets(store->value, MAX_STR, stdin);
        store->value[strcspn(store->value, "\n")] = 0;
	store->is_empty = 0; 
        msync(store, sizeof(storage), MS_SYNC);
        printf("\n[Success] Data successfully persisted directly to disk storage!\n");
    }


munmap(store, sizeof(storage));
    close(fd);
    return 0;

}
