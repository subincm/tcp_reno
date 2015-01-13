#ifndef __BUFFER_H__
#define __BUFFER_H__
#include "packet.h"

enum {
        E_BUF_EMPTY = 1,
        E_BUF_FULL,
        E_DUP_ENTRY,
        E_STRAY_SEQ,
        E_NOT_FOUND
};

typedef struct circ_buf {
	packet_info_t** slots;
	int size;
	unsigned int next_read_seq;
        unsigned int next_contig_write_seq;
} circ_buffer_t;

#define IS_BUFFER_FULL(p)   ( ((p)->next_read_seq != (p)->next_contig_write_seq) && \
                              (((p)->next_read_seq % (p)->size) == \
                              ((p->next_contig_write_seq) % (p)->size)) )

#define IS_BUFFER_EMPTY(p)  ((p)->next_read_seq == (p)->next_contig_write_seq)

#define IS_IN_BUFFER(seq, p)  ((seq >= (p)->next_read_seq) &&   \
                                (seq <= (p)->next_contig_write_seq))

#define SEQ_TO_SLOT(seq, buf)  ( (seq) % buf->size)

#define ALLOWED_IN_BUF(seq,buf) ((seq >= buf->next_read_seq) && \
        (seq <= (buf->next_contig_write_seq + window_size(buf))))

#define NEXT_ACK(buf) ((buf)->next_contig_write_seq)

void init_circular_buffer(circ_buffer_t *buffer, int size);
int write_to_buffer(circ_buffer_t *buf, packet_info_t *pkt_info);
int  read_from_buffer(circ_buffer_t *buf, packet_info_t ** entry);
int read_upto_seq(circ_buffer_t *buf, unsigned int seq, packet_info_t **entry);
void print_circ_buffer(circ_buffer_t *buf);
void free_circ_buffer(circ_buffer_t *buf);
inline int window_size(circ_buffer_t *buffer);
//int available_buffer_space(circ_buffer_t *buf);

#endif /* __BUFFER_H__ */ 
