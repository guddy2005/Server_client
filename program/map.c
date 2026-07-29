#include<stdio.h>
#include<string.h>
#include<stdlib.h>


typedef struct{
	int transaction_id;
	char imsi[20];
	char msisdn[20];
	char sms[160];
	}map_message;

int main(){
	map_message msg;
	msg.transaction_id=1001;
	strcpy(msg.imsi, "404450123456789");
        strcpy(msg.msisdn, "919876543210");

	strcpy(msg.sms, "Hello Guddy");
        printf("Transaction ID : %d\n", msg.transaction_id);
        printf("IMSI           : %s\n", msg.imsi);
        printf("MSISDN         : %s\n", msg.msisdn);
        printf("SMS            : %s\n", msg.sms);

    return 0;
}



