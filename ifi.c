#include "assert.h"
#include "ifi.h"

int get_ifi(ifi_t * ifi_array[]) {
  struct ifi_info *ifi, *ifihead;
  struct sockaddr	*sa;
  struct sockaddr_in *ip = NULL;
  struct sockaddr_in *nmask = NULL;
  struct in_addr subnet;
  int i, index = 0;

  memset(ifi_array, 0, sizeof(ifi_array));
  for (ifihead = ifi = Get_ifi_info_plus(AF_INET, 0); ifi != NULL; ifi = ifi->ifi_next)
  {

    /* Only Bind Unicast Address */
    if ((sa = ifi->ifi_addr) == NULL) 
      continue;

    ifi_array[index] = malloc(sizeof(ifi_t));
    assert(ifi_array[index]);

    strncpy(ifi_array[index]->name, ifi->ifi_name, strlen(ifi->ifi_name));

    if (sa->sa_family == AF_INET) {

      if ((sa = ifi->ifi_addr) != NULL) {
        ip = (struct sockaddr_in *) sa;
        if (inet_ntop(AF_INET, &ip->sin_addr, 
              ifi_array[index]->ip_addr, ADDR_LEN) == NULL) {
          err_quit("inet_ntop: ip address error.");
        }
      }
      if ( (sa = ifi->ifi_ntmaddr) != NULL) {
        nmask = (struct sockaddr_in*) sa;
        if (inet_ntop(AF_INET, &nmask->sin_addr,
              ifi_array[index]->nmask, ADDR_LEN) == NULL) {
          err_quit("inet_ntop: nmask error.");
        }
      }
      if (ip && nmask) {
        subnet.s_addr = ip->sin_addr.s_addr & nmask->sin_addr.s_addr;
        if (inet_ntop(AF_INET, &subnet, 
              ifi_array[index]->subnet, ADDR_LEN) == NULL) {
          err_quit("inet_ntop: Subnet error.");
        }
      }
    }
    index++;
  }
  free_ifi_info_plus(ifihead);

  return index;
}

void free_ifi(ifi_t * ifi_array[], int size)
{
  int i;
  for (i = 0; i < size; i++) {
    if (ifi_array[i])
      free(ifi_array[i]);
  }
}

void print_ifi(ifi_t * ifi_array[], int size)
{
  int i;
  printf("\nInterface List: (Total = %d)\n", size);
  for (i = 0; i < size; i++) {
    if (ifi_array[i]) {
      printf("\tIP address:     %s\n", ifi_array[i]->ip_addr);
      printf("\tNetwork Mask:   %s\n", ifi_array[i]->nmask);
      printf("\tSubnet:         %s\n\n", ifi_array[i]->subnet);
    }
  }
}

void bind_ifi(ifi_t * ifi_array[], int num_ifi, short port)
{
    int i, on = 1;
    struct sockaddr_in servaddr;
  for(i=0;i < num_ifi; i++){

    ifi_array[i]->sockfd = Socket(AF_INET, SOCK_DGRAM, 0);

    Setsockopt(ifi_array[i]->sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    
    inet_pton(AF_INET, ifi_array[i]->ip_addr, &servaddr.sin_addr);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    Bind(ifi_array[i]->sockfd, (SA *) &servaddr, sizeof(servaddr));
    
    printf("Bound to %s\n", Sock_ntop((SA *)&servaddr, sizeof(servaddr)));
  }
}


