/**************************************************
 * This file contains routines to manage the Circular Buffer which 
 * we will be using to store and retreive the packets.
 * The slot number where a packet can be found is obtained by using a 
 * simple modulo arithmetic on the sequence number
 */
#include "buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <assert.h>

void init_circular_buffer(circ_buffer_t *buffer, int size)
{
	buffer->size = size;
        buffer->next_read_seq = 0;
        buffer->next_contig_write_seq = 0;
	buffer->slots = (packet_info_t **)calloc(size, sizeof(packet_info_t *));
}

inline int window_size(circ_buffer_t *buffer)
{
   assert(buffer->next_read_seq <= buffer->next_contig_write_seq);
   return (buffer->size - ((buffer->next_contig_write_seq) - buffer->next_read_seq));
}

int write_to_buffer(circ_buffer_t *buf, packet_info_t *pkt_info)
{
  int slot = 0;
  if(!ALLOWED_IN_BUF(pkt_info->seq,buf))
  {
    printf("    [Info] Stray Packet seq. no %d , discard\n", pkt_info->seq);
    return -E_STRAY_SEQ;
  }

  if (IS_BUFFER_FULL(buf)) {
    printf("    [Info] Buffer Full seq. no %d , discard\n", pkt_info->seq);
    return -E_BUF_FULL;
  }

  /* Add the entry in the Circular buffer at the right slot */
  slot = SEQ_TO_SLOT(pkt_info->seq, buf);
  if (buf->slots[slot] == NULL)
  {
    buf->slots[slot] = pkt_info;
  }
  else
  {
    printf("    [Info] Duplicate Packet received for seq = %d\n", pkt_info->seq);
    buf->slots[slot]->dup_count++;
  }

  if(pkt_info->seq == buf->next_contig_write_seq)
  {
    while (window_size(buf))
    {
      /* Check if we can do Cumulative ACKs */
      slot = SEQ_TO_SLOT(++(buf->next_contig_write_seq), buf);
      /* If an entry already exists, munch it */
      if (buf->slots[slot])
        continue;
      else 
        break;
    }
  }

  return 0;
}

int read_from_buffer(circ_buffer_t *buf, packet_info_t **entry)
{
  int slot = 0;
  assert(entry);

  if(IS_BUFFER_EMPTY(buf))
  {
    printf("Buffer Empty, can read no more\n");
    return -E_BUF_EMPTY;
  }
  /* Find the next Entry that can be read */
  slot = SEQ_TO_SLOT(buf->next_read_seq, buf);
  assert(*entry = buf->slots[slot]);
 
  assert((*entry)->seq == buf->next_read_seq);
  buf->slots[slot] = NULL;

  /* Advance the read pointer */
  buf->next_read_seq++;

  return 0;
}

/* Routine to Slide the Left Edge of the window to the Right on receiving 
 * ACKs from the client.
 */
int read_upto_seq(circ_buffer_t *buf, unsigned int seq, packet_info_t **entry)
{
  int slot = 0;
  assert(entry);

  *entry = NULL;
  
  if(!IS_IN_BUFFER(seq, buf))
  {
    printf("    read_upto_seq: Stray ACK, Ignore.\n");
    return -E_NOT_FOUND;
  }

  while(!IS_BUFFER_EMPTY(buf))
  {
    /* Find the next Entry that can be read */
    slot = SEQ_TO_SLOT(buf->next_read_seq, buf);
    assert(*entry = buf->slots[slot]);

    assert((*entry)->seq == buf->next_read_seq);

    /* Found the Next Packet to Send */
    if ((*entry)->seq == seq)
      return 0;
   
    /* Release the structure as we have got the ACK from the Client */
    free(*entry);

    *entry = NULL;
    buf->slots[slot] = NULL;

    /* Advance the read pointer */
    buf->next_read_seq++;
  }
  return 0;
}

void free_circ_buffer(circ_buffer_t *buf)
{  
  int i ;
  packet_info_t *entry;
  for (i = 0; i < buf->size; i++) {
    if (entry = buf->slots[i])
      free_pkt_info(entry);
  }
  free(buf->slots);
}

void print_circ_buffer(circ_buffer_t *buf)
{
  int i;
  packet_info_t *entry = NULL;
  for (i = 0; i < buf->size; i++) {
    if( entry = buf->slots[i])
    {
      fprintf(stdout, "[ Seq Number:%d\nData:\n[%.*s]\n", entry->seq, entry->data_len, entry->data);
    }
  }
}

