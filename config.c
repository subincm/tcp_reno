#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "string.h"

#define MAX_LINE 80

int read_server_config(const char* file_name, server_config_t* cfg) {
	FILE *file = fopen (file_name, "r");
	char line[MAX_LINE];
	if (NULL == file) {
		err_quit("server config file %s doesn't exist.", file_name);
		return -1;
	}

	if (fgets(line, sizeof(line), file) == NULL) {
		err_quit("No  server port number in file %s\n", file_name);
		return -1;
	}
	cfg->server_port = atoi(line);

	if (fgets(line, sizeof(line), file) == NULL) {
		err_quit("No Send window size in file %s\n", file_name);
		return -1;
	}
	cfg->window_size = atoi(line);

	fclose(file);

	return 0;
}

void print_server_config(server_config_t* cfg) {
	printf("Server Config:\n================\n");
	printf("\tServer Port:%d\n", cfg->server_port);
	printf("\tSend Window Size:%d\n", cfg->window_size);
}

int read_client_config(const char* file_name, client_config_t* cfg) {
	FILE *file = fopen (file_name, "r");
	char line[MAX_LINE];
	int i, expected_lines = 7;

	if (NULL == file) {
		err_quit("client config file %s doesn't exist.", file_name);
		return -1;
	}

	for (i = 0; i < expected_lines; i++) 
        {
		if (NULL == fgets(line, sizeof(line), file)) {
			err_quit("Invalid Client config file. %s\n", file_name);
			return -1;
		}
                switch(i)
                {
                  case 0:
                    strncpy(cfg->server_ip, line, strlen(line)+1);
                    cfg->server_ip[strlen(line)-1] = '\0';
                    break;
                  case 1:
                    cfg->server_port = atoi(line);
                    break;
                  case 2:
                    strncpy(cfg->file_name, line, strlen(line)+1);
                    cfg->file_name[strlen(line)-1] = '\0';
                  case 3:
                    cfg->window_size = atoi(line);
                  case 4:
                    cfg->seed = atoi(line);
                  case 5:
                    cfg->prob_loss = atof(line);
                  case 6:
                    cfg->mean = atoi(line);
                }
        }

	fclose(file);

	return 0;
}

void print_client_config(client_config_t* cfg) {
	printf("Client Config:\n==================\n");
	printf("\tServer IP:        %s\n", cfg->server_ip);
	printf("\tServer Port:      %d\n", cfg->server_port);
	printf("\tFile:             %s\n", cfg->file_name);
	printf("\tWindow Size:      %d\n", cfg->window_size);
	printf("\tSeed:             %d\n", cfg->seed);
	printf("\tLoss Probability: %f\n", cfg->prob_loss);
	printf("\tMean:             %d\n", cfg->mean);
}

int set_alarm(int msec)
{
  struct itimerval old, new;
  int seconds = msec / 1000;
  int remainder = msec % 1000;

  new.it_interval.tv_sec = 0;
  new.it_interval.tv_usec = 0;

  new.it_value.tv_sec = seconds;
  new.it_value.tv_usec = remainder * 1000;

  return (setitimer(ITIMER_REAL, &new, &old));
}

