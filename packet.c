#include "packet.h"
#include "assert.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Store the Contents of a Packet, its length and header fields in Host Order */ 
packet_info_t *get_packet_info(unsigned char* data, int size)
{
  unsigned char* d;
  int data_len;
  packet_info_t *pkt_info;

  packet_t *p = (packet_t *)data;

  assert(data);

  pkt_info = (packet_info_t *)calloc(1, sizeof(packet_info_t));
  assert(pkt_info);

  pkt_info->seq = ntohl(p->seq);
  pkt_info->ack = ntohl(p->ack);
  pkt_info->timestamp = ntohl(p->timestamp);
  pkt_info->window_size = ntohs(p->window_size);
  pkt_info->flags = p->flags;

  data_len = size-HEADER_SIZE;
  
  d = malloc(data_len);
  assert(d);
  
  memcpy(d, &(p->data), data_len);
  pkt_info->data = d;
  pkt_info->data_len = data_len;

  return pkt_info;
}

/* Build a Packet from a Packet info structure */ 
packet_t* build_packet(packet_info_t *pkt_info)
{
  packet_t* packet = (packet_t *)malloc(sizeof(char) * (HEADER_SIZE+pkt_info->data_len));

  assert(packet);
  assert(pkt_info);

  packet->seq = htonl(pkt_info->seq);
  packet->ack = htonl(pkt_info->ack);
  packet->timestamp = htonl(pkt_info->timestamp);
  packet->window_size = htons(pkt_info->window_size);
  packet->flags = pkt_info->flags;

  memcpy((char *)packet+HEADER_SIZE, pkt_info->data, pkt_info->data_len);
  return packet;
}

void free_pkt_info(packet_info_t *pkt_info)
{
    assert(pkt_info);
    assert(pkt_info->data);
    free(pkt_info->data);
    free(pkt_info);
}

void print_packet_info(packet_info_t *pkt_info)
{
  assert(pkt_info);

  printf(" Packet Info\n===============\n");
  printf(" pkt_info->seq = %u\n", pkt_info->seq);
  printf(" pkt_info->ack = %u\n", pkt_info->ack);
  printf(" pkt_info->timestamp = %u\n", pkt_info->timestamp);
  printf(" pkt_info->window_size = %hu\n", pkt_info->window_size);
  printf(" pkt_info->data_len = %d\n", pkt_info->data_len);
  printf(" pkt_info->flags = | ");
  if (IS_DATA(pkt_info))
      printf("DATA | ");
  if (IS_EOF(pkt_info))
      printf("EOF | ");
  if (IS_FILE(pkt_info))
      printf("FILE | ");
  if (IS_PROBE(pkt_info))
      printf("PROBE | ");
  if (IS_ERR(pkt_info))
      printf("ERR | ");
  printf("\n");
}
