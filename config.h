#ifndef __CONFIG_H__
#define __CONFIG_H__

#define ADDR_LEN    32
#define FILE_LEN    50
#include <sys/time.h>

typedef int bool;
#define TRUE        1
#define FALSE       0

#ifndef LINE_MAX
#define LINE_MAX    80
#endif 

typedef struct  server_config {
	short server_port;
	int window_size;
} server_config_t;

typedef struct client_config {
	char    server_ip[ADDR_LEN];
	short   server_port;
	char    file_name[FILE_LEN];
	unsigned short window_size;
	int     seed;
	double  prob_loss;
	int     mean;
} client_config_t;

extern int read_server_config(const char* file_name, server_config_t* config);
extern void print_server_config(server_config_t* cfg);
extern int read_client_config(const char* file_name, client_config_t* cfg);
extern void print_client_config(client_config_t* cfg);
extern int set_alarm(int msec);
#endif /* __CONFIG_H__ */
