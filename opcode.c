#include<stdio.h>

typedef enum {
UPDATE_LOCATION =2,
SEND_ROUTING_INFO =22,
MT_FORWARD_SM=44,
MO_FORWARD_SM =45
}MAP_OPCODE;

int main(){
	MAP_OPCODE op = MT_FORWARD_SM;
	printf("Opcode =%d\n",op);
	return 0;

}
