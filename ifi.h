#ifndef __IFI_H__
#define __IFI_H__
#include "unpifiplus.h"
#include "config.h"

#define IFI_NAME_LEN    16
#define IFI_MAX         20

#ifndef ADDR_LEN
#define ADDR_LEN        32
#endif

typedef struct ifi {
    int  sockfd;
    char name[IFI_NAME_LEN];
    char ip_addr[ADDR_LEN]; 
    char nmask[ADDR_LEN];
    char subnet[ADDR_LEN];
} ifi_t;


extern int  get_ifi(ifi_t *ifi_array[]);
extern void free_ifi(ifi_t *ifi_array[], int size);
extern void print_ifi(ifi_t *ifi_array[], int size);
extern void bind_ifi(ifi_t * ifi_array[], int num_ifi, short port);

#endif /* __IFI_H_ */
