#ifndef __CLIENT_THREAD_H__
#define __CLIENT_THREAD_H__
#include "config.h"
#include "buffer.h"

typedef struct thread_arg {
	circ_buffer_t* rcv_buf;
	client_config_t *config;
	int sockfd;
} thread_arg;

#endif
