#include "unpifiplus.h"
#include "config.h"
#include "ifi.h"
#include "packet.h"
#include <assert.h>
#include <pthread.h>
#include "client_thread.h"
#include "rtt_mod.h"
#include <math.h>
#include <setjmp.h>

/* Globals */
pthread_mutex_t buf_mutex = PTHREAD_MUTEX_INITIALIZER;
static rtt_info_t rttinfo;
static sigjmp_buf jmpbuf;
static sigjmp_buf jmpbuf_close;

void sig_alarm(int signo)
{
	siglongjmp(jmpbuf, 1);
}

void sig_alarm_close(int signo)
{
	siglongjmp(jmpbuf_close, 1);
}

/* Routine to decide whether to simulate Packet Loss */
static inline int consume_random_packet(packet_info_t *pkt_info, double prob_loss, bool recv) 
{
  double rnd =  (double)rand()/RAND_MAX;
  if (rnd < prob_loss)
  {
    /* Drop on Rx */
    if( recv == TRUE)
    {
      if (IS_PROBE(pkt_info)) {
        printf("[Simulate Loss] Probe Packet dropped on Rx\n");
      }
      else {
        printf("[Simulate Loss] Packet wih seq number:%u dropped on Rx\n", pkt_info->seq);
      }
    }
    else {
      printf("[Simulate Loss] Packet wih ACK number:%u dropped on Tx\n", pkt_info->ack);
    }
    return TRUE;
  }
  return FALSE;
}

int ip_on_same_subnet(char* ip,ifi_t *ifi_array[], int total_ifi, char* cli_ip)
{	
  int i,ip_int,j=0,n,min=500,same_host=0;
  long int mask_int,net_int;
  char temp[10][ADDR_LEN];
  for(i=0;i<total_ifi;i++)
  {
    //check if server's on same host
    if(strcmp(ifi_array[i]->ip_addr,ip) == 0)
    {
      same_host = 1;
      printf("[Info] Destination on same host\n");
      strcpy(cli_ip,"127.0.0.1");
    }
    inet_pton(AF_INET,ifi_array[i]->nmask,&mask_int);
    inet_pton(AF_INET,ip,&ip_int);
    mask_int = (ip_int & (mask_int));
    inet_ntop(AF_INET,&mask_int,temp[i],ADDR_LEN);
    inet_pton(AF_INET,ifi_array[i]->subnet,&net_int);
    n = mask_int-net_int;
    if (n <= min && n >= 0)
    {	

      if(n = min)
      {
        if(temp[i] > ifi_array[j]->subnet)
        {
          min = n;
          j=i;
        }
      }
    }

  }
  for(i=0;i<total_ifi;i++)
  {
    if(strcmp(temp[i],ifi_array[i]->subnet) == 0)
      break;
  }
  if (i == total_ifi){
    return 0;
  }

  else{
    if(!same_host){
      strcpy(cli_ip,ifi_array[j]->ip_addr);
    }
    return 1;
  }
}
/* Routine which sends ACK responses to the Server.
 * Note: There is no timeout for ACKs
 */
void send_ack(int sock_fd, unsigned short window_size, unsigned int send_seq, 
            unsigned int ack_seq, unsigned int timestamp, double prob_loss)
{
  packet_info_t pkt_info;
  packet_t *pkt = NULL;
  memset(&pkt_info, 0, sizeof(pkt_info));

  pkt_info.seq = send_seq;
  pkt_info.ack = ack_seq;
  pkt_info.window_size = window_size;
  SET_ACK_FLAG(&pkt_info);

  pkt_info.data_len = 0;
  pkt_info.timestamp = rtt_ts(&rttinfo);

  if (!consume_random_packet(&pkt_info, prob_loss, FALSE))
  {
    pkt = build_packet(&pkt_info);
    assert(pkt);

    Write(sock_fd, (char *)pkt, HEADER_SIZE);

    free(pkt);
  }
}

/* This is the Thread which will read data from the Circular buffer 
 * and print out to the Console
 */
void* consumer_thread(void *arg) 
{
  circ_buffer_t *rcv_buf;
  double mean, smoothed_mean;
  int sockfd;
  packet_info_t *pkt_info;
  double get_rand;
  bool data_present, was_buf_full, done = FALSE;
  unsigned int ack_seq;
  unsigned short new_win;
  double prob_loss;

  assert(arg);
  assert(rcv_buf = ((thread_arg *)arg)->rcv_buf);
  assert(((thread_arg *)arg)->config);
  mean = smoothed_mean = ((thread_arg *)arg)->config->mean;
  prob_loss = ((thread_arg *)arg)->config->prob_loss;
  sockfd = ((thread_arg *)arg)->sockfd;
  free(arg);

  Pthread_detach(pthread_self());

  while (!done) {
    get_rand = (double)rand()/RAND_MAX;
    smoothed_mean = -1.0 * mean * log(get_rand);

    printf("[Consumer Thread] Wake Up in %lf ms\n", smoothed_mean);
    usleep((useconds_t)(smoothed_mean * 1000));

    data_present = FALSE;
    was_buf_full = FALSE;

    Pthread_mutex_lock(&buf_mutex);
    if (IS_BUFFER_FULL(rcv_buf))
      was_buf_full = TRUE;

    while(read_from_buffer(rcv_buf, &pkt_info) >= 0)
    {
      data_present = TRUE;
      printf("[Consumer thread] [seq:%u]\n%.*s\n", pkt_info->seq,
          pkt_info->data_len, pkt_info->data);

      if(IS_EOF(pkt_info))
      {
        done = TRUE;
        break;
      }
      free_pkt_info(pkt_info);
      pkt_info = NULL;
    }
   
    /* Save off these values as we are releasing the lock below */
    new_win = window_size(rcv_buf); 
    ack_seq = NEXT_ACK(rcv_buf); 

    Pthread_mutex_unlock(&buf_mutex);
    
    if (!data_present)
      printf("[Consumer Thread] No data this time around\n");

    if(was_buf_full)
    {
      /* Advertise New Opened Up window */
       send_ack(sockfd, new_win, 0, ack_seq, 0, prob_loss); 
    }
  }
  exit(0);
}

/* This routine does the first phase of the client-server Setup.
 * On successfule return, the out parameter sockfd will have the 
 * connection socket fd.
 * */
int connection_setup(int *sockfd, ifi_t *ifi_array[], int num_ifi,
    circ_buffer_t *rcv_buf, client_config_t *config)
{
  packet_t *send_pkt;
  int n, optval=1;
  char in_packet[PACKET_LEN],client_ip[ADDR_LEN];	
  struct sockaddr_in serv_addr, cliaddr, tempaddr;
  unsigned int ack_seq;
  unsigned short curr_win;
  bool is_error = FALSE;
  sigset_t x;
  packet_info_t *rcv_pkt_info, *send_pkt_info = calloc(1, sizeof(packet_info_t));
  
  *sockfd = Socket(AF_INET, SOCK_DGRAM, 0);

  if(ip_on_same_subnet(config->server_ip,ifi_array, num_ifi, client_ip))
  {	
    printf("[Info] Server on same subnet or host, Client Bound to %s\n",client_ip);
    setsockopt(*sockfd,SOL_SOCKET, SO_DONTROUTE, &optval, sizeof(optval));
  }	

  bzero(&cliaddr, sizeof(cliaddr));
  cliaddr.sin_family = AF_INET;
  cliaddr.sin_port = 0;
  inet_pton(AF_INET,client_ip,&cliaddr.sin_addr);
  Bind(*sockfd, (SA*) &cliaddr, sizeof(cliaddr));

  //cli addr and port using getsockname
  int cli_port;
  char cli_ip[ADDR_LEN];
  socklen_t len = sizeof(tempaddr);
  bzero(&tempaddr, sizeof(tempaddr));
  getsockname(*sockfd, (SA*) &tempaddr, &len);
  Inet_ntop(AF_INET, &tempaddr.sin_addr, cli_ip, ADDR_LEN);
  cli_port = ntohs(tempaddr.sin_port);

  //connect
  bzero(&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(config->server_port);
  Inet_pton(AF_INET, config->server_ip, &serv_addr.sin_addr);
  Connect(*sockfd, (SA *)&serv_addr, sizeof(serv_addr));

  assert(send_pkt_info);

  /* For block/unblock later in code */
  sigemptyset (&x);
  sigaddset(&x, SIGALRM);

  Signal(SIGALRM, sig_alarm);
  rtt_newpack(&rttinfo);          /* initialize for this packet */

  /* Prepare to send file name */
  send_pkt_info->seq = 0;
  send_pkt_info->ack = 0;
  send_pkt_info->window_size = config->window_size;
  SET_FILE_FLAG(send_pkt_info);
  send_pkt_info->data = strdup(config->file_name);
  send_pkt_info->data_len = strlen(config->file_name) + 1;

  printf("[Info] Sending file name %s to server ..\n", send_pkt_info->data);

sendagain:
  send_pkt_info->timestamp = rtt_ts(&rttinfo);
  send_pkt = build_packet(send_pkt_info);

  Write(*sockfd, (char *)send_pkt, send_pkt_info->data_len+HEADER_SIZE);

  /* set alarm for RTO seconds using setitimer */
  set_alarm(rtt_start(&rttinfo));
  if (sigsetjmp(jmpbuf, 1) != 0)
  {
    if (rtt_timeout(&rttinfo))
    {
      printf("[Error] Timed out Sending File Name, giving Up\n");
      free_pkt_info(send_pkt_info);
      free(send_pkt);
      errno = ETIMEDOUT;
      return -1;
    }
    printf("[Timeout] Retransmitting file name, next RTO:%d ms\n", rttinfo.rtt_rto);
    free(send_pkt);
    goto sendagain;
  }

  /* Now Attempt to read the Port message from the Server */
  while (1)
  {
    if ((n = read(*sockfd, in_packet, PACKET_LEN)) < 0)
    {
      if (errno == EINTR) 
        continue;
      else
        err_sys("[Error] Read Error while waiting for Port number");
    }

    sigprocmask(SIG_BLOCK, &x, NULL);
    rcv_pkt_info = get_packet_info(in_packet, n);

    if (consume_random_packet(rcv_pkt_info, config->prob_loss, TRUE))
    {
      free_pkt_info(rcv_pkt_info);
      sigprocmask(SIG_UNBLOCK, &x, NULL);
      continue;
    }

    if (IS_ACK(rcv_pkt_info) && (rcv_pkt_info->ack == (send_pkt_info->seq+1)))
    {
      break;
    }
    else
    {
      free_pkt_info(rcv_pkt_info);
      sigprocmask(SIG_UNBLOCK, &x, NULL);
      continue;
    }
  }

  set_alarm(0);     /* Turn off the Alarm */
  sigprocmask(SIG_UNBLOCK, &x, NULL);

  free_pkt_info(send_pkt_info);
  free(send_pkt);

  assert(rcv_pkt_info->data_len == sizeof(short));
  /* Fetch the new port from the server message */
  memcpy(&serv_addr.sin_port, rcv_pkt_info->data, sizeof(short));
  
  printf("[Info] Received new Port number %hu from Server.\n", ntohs(serv_addr.sin_port));

  /* Connect to the new port of the server child process */
  if (connect(*sockfd, (SA *)&serv_addr, sizeof(serv_addr)) < 0) {
    printf("[Error] Connect failure to server child: [%s : %hu]\n", config->server_ip, ntohs(serv_addr.sin_port));
    return -1;
  }

  printf("[Info] Connected to server's child process.\n");

  /* Advance the Circular buffer's read/write pointers to rcv_pkt_info->seq+1.
   * Basically, we are simulating the producer/consumer behavior here, in that we 
   * have written and read from the buffer.
   * Note: The server has to continue the file transfer starting from rcv_pkt_info->seq+1
   * as we will not accept anything lower than this sequence for this session
   * This is similar to the SYN+ACK in TCP
   */
  rcv_buf->next_read_seq = rcv_buf->next_contig_write_seq = rcv_pkt_info->seq+1;

  curr_win = window_size(rcv_buf); 
  ack_seq = NEXT_ACK(rcv_buf); 

  /* Exit if the file does not exist on the server after sending ACK.
   * In the event this ACK is lost, the server ichild timeout mechanism will kick in
   * and eventually it will timeout and give up
   */
  if(is_error = IS_ERR(rcv_pkt_info))
  {
    printf("[Info] Received Error message from server [Seq: %u] Responding with [Ack:%u] [Window Size:%hu]\n", 
        rcv_pkt_info->seq, ack_seq, curr_win);
  }
  else
  {
    printf("[Info] Received Port message [Seq: %u] Responding with [Ack:%u] [Window Size:%hu]\n", 
        rcv_pkt_info->seq, ack_seq, curr_win);
  }

  /* Simulate Loss On Tx */
  if (!consume_random_packet(rcv_pkt_info, config->prob_loss, FALSE))
  {
    send_ack(*sockfd, curr_win, rcv_pkt_info->seq, ack_seq, rcv_pkt_info->timestamp, config->prob_loss);
  }

  free_pkt_info(rcv_pkt_info);
  
  if(is_error)
  {
      printf("[Error] File %s does not exist, terminating..\n", config->file_name);
      errno = EBADF;
      return -1;
  }
  
  printf("[Info] Successful Connection Setup with [%s:%u], ready for file reception\n\n", 
      config->server_ip, ntohs(serv_addr.sin_port));
  return 0;

}

void handle_close(int sockfd, circ_buffer_t *rcv_buffer)
{
  int n;
  char in_packet[PACKET_LEN];	
  unsigned int ack_seq;
  unsigned short curr_win;
  sigset_t x;
  bool is_probe = FALSE, is_error = FALSE;

  /* Stick around for some more time handling ACKS in case our 
   * first ACK for eof got lost and the Server is retransmitting. 
   */
  sigemptyset (&x);
  sigaddset(&x, SIGALRM);

  Signal(SIGALRM, sig_alarm_close);
  rtt_newpack(&rttinfo);          /* initialize for this packet */

  printf("[Handle Close]: Hang around for some more time\n");
  /* We wait for 2 times the conservative RTO and exit. */
  set_alarm(2 * rtt_start(&rttinfo));
  while (1)
  {
    if ((n = read(sockfd, in_packet, PACKET_LEN)) < 0)
    {
      if (errno == EINTR) 
        continue;
      else
        err_sys("[Error] Unknown Read Error");
    }

    sigprocmask(SIG_BLOCK, &x, NULL);
    packet_info_t *pkt_info = get_packet_info(in_packet, n);

    if (!IS_DATA(pkt_info) && !(is_probe = IS_PROBE(pkt_info))
        && !(is_error = IS_ERR(pkt_info)))
    {
      free_pkt_info(pkt_info);
      sigprocmask(SIG_UNBLOCK, &x, NULL);
      continue;
    }

    Pthread_mutex_lock(&buf_mutex);
    /* Save off these values as we are releasing the lock below */
    curr_win = window_size(rcv_buffer); 
    ack_seq = NEXT_ACK(rcv_buffer);
    Pthread_mutex_unlock(&buf_mutex);

    printf("[Info] Received [Seq: %u] Responding with [Ack:%u] [Window Size:%hu]\n", 
        pkt_info->seq, ack_seq, curr_win);

    send_ack(sockfd, curr_win, pkt_info->seq, ack_seq, pkt_info->timestamp, 0);

    free_pkt_info(pkt_info);
    sigprocmask(SIG_UNBLOCK, &x, NULL);
  }
  if (sigsetjmp(jmpbuf_close, 1) != 0)
  {
    printf("[Handle Close]: Done Hanging around, exit gracefully..\n");
    return;
  }
}

int main(int argc, char **argv)
{

  int n,sockfd, on=1,i,j,maxfdp,optval=1;
  char in_packet[PACKET_LEN];	
  client_config_t config;
  int num_ifi = 0;
  pthread_t tid;
  thread_arg* arg;
  bool done = FALSE, is_probe = FALSE, is_error = FALSE;
  unsigned int ack_seq, seq, timestamp;
  unsigned short curr_win;
  sigset_t x;

  circ_buffer_t rcv_buffer;

  ifi_t *ifi_array[IFI_MAX];

  read_client_config("client.in", &config);
  print_client_config(&config);

  num_ifi = get_ifi(ifi_array);
  print_ifi(ifi_array, num_ifi);

  srand(config.seed);
  init_circular_buffer(&rcv_buffer, config.window_size);
  
  rtt_init(&rttinfo);

  if (connection_setup(&sockfd, ifi_array, num_ifi, &rcv_buffer, &config) < 0)
    err_sys("[Error] Connection Setup Error, Terminating..\n");

  arg = (thread_arg*)calloc(1, sizeof(thread_arg));
  arg->rcv_buf = &rcv_buffer;
  arg->config = &config;
  arg->sockfd = sockfd;
  Pthread_create(&tid, NULL, &consumer_thread, arg);

  /* Below is the Producer Logic which reads from the socket and fills 
   * up the receive Buffer.
   */
  while (!done)
  {
    if ((n = read(sockfd, in_packet, PACKET_LEN)) < 0)
    {
      if (errno == EINTR) 
        continue;
      else
        err_sys("[Error] Unknown Read Error");
    }

    packet_info_t *pkt_info = get_packet_info(in_packet, n);

    if (!IS_DATA(pkt_info) && !(is_probe = IS_PROBE(pkt_info))
          && !(is_error = IS_ERR(pkt_info)))
    {
      free_pkt_info(pkt_info);
      continue;
    }

    if (consume_random_packet(pkt_info, config.prob_loss, TRUE))
    {
      free_pkt_info(pkt_info);
      continue;
    }

    if(IS_EOF(pkt_info) || is_error)
      done = TRUE;

    Pthread_mutex_lock(&buf_mutex);
    /* Special Handling for Probes & Errors, send an ACK, don't store in buffer */
    if(!is_probe && !is_error)
    {
      write_to_buffer(&rcv_buffer , pkt_info);
    }
    /* Save off these values as we are releasing the lock below */
    curr_win = window_size(&rcv_buffer); 
    ack_seq = NEXT_ACK(&rcv_buffer);
    seq = pkt_info->seq;
    timestamp = pkt_info->timestamp;

    Pthread_mutex_unlock(&buf_mutex);

    if(is_probe)
      printf("[Info] Persist Timer Response [Ack:%u] [Window Size:%hu]\n", ack_seq, curr_win);
    else
      printf("[Info] Received [Seq: %u] Responding with [Ack:%u] [Window Size:%hu]\n", 
          seq, ack_seq, curr_win);

    send_ack(sockfd, curr_win, seq, ack_seq, timestamp, config.prob_loss);

  }
  /* Try to be graceful to the Server */
  handle_close(sockfd, &rcv_buffer);  

  pthread_exit(NULL);
}
