#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define QUEUE_CAPACITY 5

#pragma pack(push, 1)
typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint8_t  flags;      // e.g., 0x01 = SYN, 0x02 = ACK
    uint8_t  checksum;
} Header;

typedef struct {
    Header header;
    char payload[32];
} Packet;
#pragma pack(pop)

typedef struct {
    Packet buffer[QUEUE_CAPACITY];
    int head;
    int tail;
    int count;
} PacketQueue;

void init_queue(PacketQueue *q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
}

bool enqueue(PacketQueue *q, Packet pkt) {
    if (q->count == QUEUE_CAPACITY) return false;
    q->buffer[q->tail] = pkt;
    q->tail = (q->tail + 1) % QUEUE_CAPACITY;
    q->count++;
    return true;
}

bool dequeue(PacketQueue *q, Packet *pkt) {
    if (q->count == 0) return false;
    *pkt = q->buffer[q->head];
    q->head = (q->head + 1) % QUEUE_CAPACITY;
    q->count--;
    return true;
}

int main() {
    PacketQueue q;
    init_queue(&q);

    
    Packet p1;
    p1.header.src_port = 8080;
    p1.header.dest_port = 9090;
    p1.header.length = 11;
    p1.header.flags = 0x01;
    p1.header.checksum = 0xFF;
    strcpy(p1.payload, "data_packet");

    printf("Header Size: %zu bytes\n", sizeof(Header));

    if (enqueue(&q, p1)) {
        printf("Packet queued successfully!\n");
    }

    Packet recv_pkt;
    if (dequeue(&q, &recv_pkt)) {
        printf("Dequeued Packet - Flags: 0x%02X, Payload: %s\n", 
               recv_pkt.header.flags, recv_pkt.payload);
    }

    return 0;
}
