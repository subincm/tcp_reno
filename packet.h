#ifndef __PACKET_H__
#define __PACKET_H__

/*
 * Header format:
 * 4 bytes seq number
 * 4 bytes ack number
 * 4 bytes timestamp
 * 2 bytes win-size
 * 1 byte for flags
 * 1 byte padding
 * Remaining 496 bytes data
 * */

#define PACKET_LEN 512
#define HEADER_SIZE 16
#define PACK_DATA_LEN 496

#define DATA_FLAG 	0x1     /* Packet Contains Data */
#define ACK_FLAG 	0x2     /* Packet is an ACK */
#define FILE_FLAG	0x4     /* Packet Contains File name */
#define ERR_FLAG	0x8     /* File does not exist, misc. errs */
#define EOF_FLAG	0x10    /* Packet containing the last file segment */
#define PROBE_FLAG	0x20    /* Probe Packet */

typedef struct packet {
	unsigned int seq;
	unsigned int ack;
	unsigned int timestamp;
	unsigned short window_size;
	unsigned char flags;
	unsigned char reserved;
        char data[0];
} packet_t;

typedef struct packet_info {
	unsigned int seq;
	unsigned int ack;
	unsigned int timestamp;
	unsigned short window_size;
	unsigned char flags;
	unsigned char reserved;
	unsigned char* data;
	unsigned int data_len;
        int dup_count;
        int dup_ack;
        int retransmit;
} packet_info_t;


packet_t* build_packet(packet_info_t *pack); 
packet_info_t* get_packet_info(unsigned char* data, int size);
void free_pkt_info(packet_info_t *pkt_info);
void print_packet_info(packet_info_t *pkt_info);

#define IS_DATA(p)      (((packet_t *)p)->flags & DATA_FLAG)
#define IS_ACK(p)       (((packet_t *)p)->flags & ACK_FLAG)
#define IS_EOF(p)       (((packet_t *)p)->flags & EOF_FLAG)
#define IS_FILE(p)      (((packet_t *)p)->flags & FILE_FLAG)
#define IS_PROBE(p)     (((packet_t *)p)->flags & PROBE_FLAG)
#define IS_ERR(p)       (((packet_t *)p)->flags & ERR_FLAG)

#define SET_DATA_FLAG(p)        (((packet_t *)p)->flags |= DATA_FLAG)
#define SET_ACK_FLAG(p)         (((packet_t *)p)->flags |= ACK_FLAG)
#define SET_EOF_FLAG(p)         (((packet_t *)p)->flags |= EOF_FLAG)
#define SET_FILE_FLAG(p)        (((packet_t *)p)->flags |= FILE_FLAG)
#define SET_PROBE_FLAG(p)       (((packet_t *)p)->flags |= PROBE_FLAG)
#define SET_ERR_FLAG(p)         (((packet_t *)p)->flags |= ERR_FLAG)

#endif

