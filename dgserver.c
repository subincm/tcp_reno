#include "unpifiplus.h"
#include "config.h"
#include "ifi.h"
#include "packet.h"	
#include "client_list.h"	
#include <unistd.h>
#include <stdlib.h>
#include <setjmp.h>
#include "rtt_mod.h"
#include "assert.h"
#include "buffer.h"
#include "congestion_ctrl.h"
#include "persist_timer.h"

/* Globals */
client_list_t client_list;
rtt_info_t rttinfo;
static sigjmp_buf jmpbuf;
static sigjmp_buf jmpbuf_data;

void sig_alarm_handler(int signo)
{
  siglongjmp(jmpbuf, 1); 
}

void sig_alarm_data_handler(int signo) {
	siglongjmp(jmpbuf_data, 1); 
}

void sig_child_handler(int signo)
{
  pid_t pid, valid_pid;
  int stat;
  char msg[LINE_MAX];
  while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
  {
    valid_pid = pid;
    delete_from_client_list(&client_list, pid);
  }
  
  snprintf(msg, LINE_MAX, "[Info] Child terminated, PID: [%d]\n", valid_pid);
  write(fileno(stderr), msg, strlen(msg));
  print_client_list(&client_list);
}

/* Routine which sends Probe messages to the client.
 * The client is expected to reply back with the updated window size
 */
void send_probe(int sock_fd)
{
  packet_info_t pkt_info;
  packet_t *pkt = NULL;
  memset(&pkt_info, 0, sizeof(pkt_info));
  
  SET_PROBE_FLAG(&pkt_info);
  pkt_info.timestamp = rtt_ts(&rttinfo);

  pkt = build_packet(&pkt_info);
  assert(pkt);

  Write(sock_fd, (char *)pkt, HEADER_SIZE);
  
  free(pkt);
}

/* Generic routine to send a packet based on a packet_info structure
 */
void send_packet(int sock_fd, packet_info_t *pkt_info)
{
  packet_t *pkt = NULL;
 
  assert(pkt_info);

  pkt_info->timestamp = rtt_ts(&rttinfo);

  pkt = build_packet(pkt_info);
  assert(pkt);

  Write(sock_fd, (char *)pkt, pkt_info->data_len+HEADER_SIZE);
  
  free(pkt);
}

/* Retransmit a packet. The packet should be in the send_buffer */
void inline retransmit_packet(int sock_fd, packet_info_t *pkt_info)
{
  pkt_info->retransmit++;
  send_packet(sock_fd, pkt_info);
}

int send_file_contents(int sockfd, int file_fd, circ_buffer_t *send_buf, 
    int *eof, congestion_ctrl_info_t *cc_info)
{
  char *send_data = NULL, in_packet[PACKET_LEN];
  packet_info_t *pkt_info, *resend_pkt_info;
  int n, i = 0, slot = 0, send_count = 0;
  unsigned int seq;
  bool resend_done = FALSE;

  seq = send_buf->next_read_seq;
  
  /* Retransmit if we have as yet un-acked inflight data */
  if (!IS_BUFFER_EMPTY(send_buf))
  {
    for( ; i < cc_info->sendwin; i++)
    {
        if (seq != send_buf->next_contig_write_seq)
        {
          slot = SEQ_TO_SLOT(seq, send_buf);
          assert (resend_pkt_info = send_buf->slots[slot]);
          assert (seq == resend_pkt_info->seq);
          
          /* Retransmit the packet */
          retransmit_packet(sockfd, resend_pkt_info);
          send_count++;
          
          seq++;
        }
        else {
          resend_done = TRUE;
          break;
        }
    }
    if (!resend_done || *eof)
      return send_count;
  } /* !IS_BUFFER_EMPTY */

  /* Fresh Window of Transmission */
  rtt_newpack(&rttinfo);

  for( ; i < cc_info->sendwin; i++)
  {
    if (*eof) {
      break;
    }

    if ((n = read(file_fd, in_packet, PACK_DATA_LEN)) < 0)
    {
      if (errno == EINTR) 
        continue;
      else
        err_sys("[Error] Unknown Read Error");
    }
    else if (n < PACK_DATA_LEN)
    {
      *eof = 1;
    }

    /* Build pkt_info */
    assert (pkt_info = calloc(1, sizeof(*pkt_info)));

    SET_DATA_FLAG(pkt_info);

    if(*eof)
      SET_EOF_FLAG(pkt_info);

    pkt_info->seq = seq;
    pkt_info->ack = 0;
    pkt_info->data_len = n;

    assert(send_data = malloc(n));
    memcpy(send_data, in_packet, n);

    pkt_info->data = send_data;

    assert(pkt_info->seq>= send_buf->next_read_seq);
    /* Add the pkt_info to the Send Buffer at the right slot */
    slot = SEQ_TO_SLOT(pkt_info->seq, send_buf);
    assert (send_buf->slots[slot] == NULL);
    send_buf->slots[slot] = pkt_info;
    send_buf->next_contig_write_seq++;
    
    /* Now, Send the Packet */
    send_packet(sockfd, pkt_info);

    send_count++;

    seq += 1;
  }

  return (send_count);
}

/* Major Routine to do the file transfer */
int file_transfer(int sockfd, FILE *f, server_config_t *config, unsigned int init_seq)
{
  circ_buffer_t send_buf;
  sigset_t x;
  congestion_ctrl_info_t cc_info;
  bool persist_on = FALSE;
  char in_packet[PACKET_LEN];
  
  int n, num_send, timeout, eof = 0, found, file_fd = fileno(f);

  init_congestion_ctrl_info(&cc_info, config->window_size);

  /* For block/unblock later in code */
  sigemptyset (&x);
  sigaddset(&x, SIGALRM);
  
  Signal(SIGALRM, sig_alarm_data_handler);

  init_circular_buffer(&send_buf, config->window_size);

  /* Initialize the send buffer's read/write pointers to init_seq. */
  send_buf.next_read_seq = send_buf.next_contig_write_seq = init_seq;

  /* Send, send, send.. wait for ack.. */
  while(1)
  {
send_data:
    num_send = send_file_contents(sockfd, file_fd, &send_buf, &eof, &cc_info);

    /* Client Receive Window is 0, start Persist timer */
    if (num_send == 0)
    {
      persist_on = TRUE;
      timeout = get_current_persist_timer() * 1000;
      printf("[Info: Persist Timer] Start Persist Timer to fire in %d msec..\n", timeout);
      set_alarm(timeout); /* Set millisecond based alarm */
    }
    /* Sent something, so start retransmit timer */
    else
    {
      persist_on = FALSE;
      timeout = rtt_start(&rttinfo);
      printf("[Info: Retransmission Timer] Start Retransmit Timer to fire in %d msec..\n", timeout);
      set_alarm(timeout); /* Set millisecond based alarm */
    }

    /* Timeout Handling */
    if (sigsetjmp(jmpbuf_data, 1) != 0) {
      if (persist_on) {
        if (persist_timeout()) 
        {
            printf("[Error: Persist Timer] Timed Out Sending %d Probes with no response from client, giving up..\n",
                    PERSIST_MAXNREXMT);
            errno = ETIMEDOUT;
            return -1;
        }
        send_probe(sockfd);
        increment_persist_timer();
        printf("[Persist Timer] Retransmitting Probe Packet, next Persist timeout:%d ms.\n", 
                    get_current_persist_timer());
        goto send_data;
      }

      if (rtt_timeout(&rttinfo)) {
        printf("[Error: Retransmit Timer] Timed Out Sending %d retransmits with no response from client, "
                "giving up..\n", RTT_MAXNREXMT);
        errno = ETIMEDOUT;
        return -1;
      }
      printf("[Timeout] Retransmitting.., next RTO:%d ms\n", rttinfo.rtt_rto);
      
      /* Inform Congestion Control */
      congestion_detected(&cc_info, CONGESTION_TIMEOUT);
      goto send_data;
    }

    /* Wait for ACKs */
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

      if (!IS_ACK(pkt_info))
      {
        free_pkt_info(pkt_info);
        sigprocmask(SIG_UNBLOCK, &x, NULL);
        continue;
      }
  
      /* Compute the latest smoothed RTT */
      if (pkt_info->timestamp) 
          rtt_stop(&rttinfo, rtt_ts(&rttinfo) - pkt_info->timestamp);
      
      printf("[Info] Received ACK [%u]\n", pkt_info->ack);

      /* Got some ACK, reset the persist timer */
      if(persist_on)
      {
        persist_on = FALSE;
        reset_persist_timer();
        set_alarm(0);
      }

      packet_info_t *next_to_send = NULL;
      found = read_upto_seq(&send_buf, pkt_info->ack, &next_to_send);
      

      if (found < 0)
      {
        free_pkt_info(pkt_info);
        sigprocmask(SIG_UNBLOCK, &x, NULL);
        continue;
      }
      else
      {
        /* Inform Congestion Control */ 
        transmission_acked(&cc_info);

        /* Adjust Send window */
        cc_info.recvwin = pkt_info->window_size;
        recalc_send_win_size(&cc_info);
        printf("[Info] Client's window size:%d, cwnd:%d, Server's send window size:%d\n", 
            cc_info.recvwin, cc_info.cwnd, cc_info.sendwin); 
        
        /* Client has ACKed all that we sent, start sending again */
        if (NULL == next_to_send)
        {
          printf("Client has ACKed all that we sent, start sending again\n"); 
          free_pkt_info(pkt_info);
          
          /* Reset the Retransmit Timer */
          set_alarm(0);

          /* If Client ACKed our last segment, we are done, exit gracefully.
           * Don't unblock the signal to prevent getting yanked back up. 
           */
          if (eof)
            goto done;
          
          sigprocmask(SIG_UNBLOCK, &x, NULL);
          break;
        }

        /* Client ACKed only part of out inflight data,
         * Check if we need to Fast Retransmit */
        if ( ++next_to_send->dup_ack >= 3)
        {
          /* Inform Congestion Control */ 
          congestion_detected(&cc_info, CONGESTION_DUP_ACK);
          
          printf("[Info] Fast retransmit packet with Seq [%d]\n", next_to_send->seq);
          retransmit_packet(sockfd, next_to_send);
        }
        
        free_pkt_info(pkt_info);
        sigprocmask(SIG_UNBLOCK, &x, NULL);
      }
    
    } /* Wait for ACKs */
  
  } /* Send, send, send.. wait for ack.. */
  
  close(sockfd);

done:
  printf("[Info] Server Child [%hu] file transfer done. Exiting...\n", getpid());

  return 0;
}

int ip_on_subnet(char* ip, char* subnet_addr) 
{
  struct sockaddr_in sa, subnet;
  inet_pton(AF_INET, subnet_addr, &subnet.sin_addr);
  inet_pton(AF_INET, ip, &sa.sin_addr);
  return ((subnet.sin_addr.s_addr & sa.sin_addr.s_addr) == subnet.sin_addr.s_addr);
}

int dg_server_child(ifi_t * ifi_array[], int listenfd_idx, 
    struct sockaddr_in *cli_addr, packet_info_t *in_pkt_info, server_config_t *config) 
{
  bool dont_route = FALSE, retransmit = FALSE;
  char *server_ip, *filename, client_ip[ADDR_LEN], in_packet[PACKET_LEN];
  int sockfd, server_port, n;
  struct sockaddr_in serv_addr;
  const int on = 1;
  packet_t *send_pkt = NULL;
  socklen_t sock_len = sizeof(struct sockaddr_in);
  packet_info_t *rcv_pkt_info, *send_pkt_info = calloc(1, sizeof(packet_info_t));

  /* May start with any number and it should work seemlessly */
  unsigned int init_seq = 0;

  Inet_ntop(AF_INET, &(cli_addr->sin_addr), client_ip, sizeof(client_ip));

  if (!strcmp(client_ip, "127.0.0.1"))
  {
    printf("[Info] Client on Localhost.\n");
    dont_route = TRUE;
  }
  /* Check whether on the same host or subnet */
  else if (ip_on_subnet(client_ip, ifi_array[listenfd_idx]->subnet))
  {
    printf("[Info] Server and Client on the same subnet.\n");
    dont_route = TRUE;
  }

  /* Create the child socket */
  bzero(&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = 0;
  Inet_pton(AF_INET, ifi_array[listenfd_idx]->ip_addr, &serv_addr.sin_addr);

  sockfd = Socket(AF_INET, SOCK_DGRAM, 0);

  if (dont_route)
    setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE, &on, sizeof(on));
  
  Bind(sockfd, (SA*) &serv_addr, sizeof(serv_addr));

  /* Get the ephemeral Port assigned by the Kernel */
  bzero(&serv_addr, sizeof(serv_addr));
  getsockname(sockfd, (SA*)&serv_addr, &sock_len);
  server_port = ntohs(serv_addr.sin_port);

  printf("[Server Child] Bound to [%s:%hu]\n", 
      ifi_array[listenfd_idx]->ip_addr, server_port);

  if (connect(sockfd, (SA *)cli_addr, sizeof(*cli_addr)) < 0)
  {
    err_quit("[Error] Unable to Connect to client [%s : %hu]\n", client_ip, 
        ntohs(cli_addr->sin_port));
  }
  
  printf("[Server Child] Connected to client [%s : %hu]\n", client_ip, 
      ntohs(cli_addr->sin_port));

  filename = in_pkt_info->data;
  printf("[Client Request] Filename:%s Window Size:%hu\n", filename, 
      in_pkt_info->window_size);

  /* Prepare to send ephemeral port information to the client.
   * If the file does not exist on the server, it is also conveyed.
   */
  send_pkt_info->seq = init_seq;
  send_pkt_info->ack = in_pkt_info->seq+1;
  send_pkt_info->window_size = config->window_size;
  
  SET_ACK_FLAG(send_pkt_info);
  SET_DATA_FLAG(send_pkt_info);

  FILE* f = fopen(filename, "r");
  if (f == NULL) {
    printf("[Error] File %s does not exist, inform client", filename);
    SET_ERR_FLAG(send_pkt_info);
  }

  send_pkt_info->data = malloc(sizeof(short));
  memcpy(send_pkt_info->data, &serv_addr.sin_port, sizeof(short));
  send_pkt_info->data_len = sizeof(short);
  
  free_pkt_info(in_pkt_info);

  rtt_init(&rttinfo);

  Signal(SIGALRM, sig_alarm_handler);
  rtt_newpack(&rttinfo);

  printf("[Info] Sending New Port number to the client ..\n");

sendagain:
  send_pkt_info->timestamp = rtt_ts(&rttinfo);
  send_pkt = build_packet(send_pkt_info);

  Sendto(ifi_array[listenfd_idx]->sockfd, send_pkt, 
      send_pkt_info->data_len+HEADER_SIZE, 0, (SA*)cli_addr, sizeof(*cli_addr));
  
  if (retransmit)
  {
    Write(sockfd, (char *)send_pkt, send_pkt_info->data_len+HEADER_SIZE);
  }
  
  /* set alarm for RTO seconds using setitimer */
  set_alarm(rtt_start(&rttinfo));
 
  if (sigsetjmp(jmpbuf, 1) != 0) {
    if (rtt_timeout(&rttinfo)) 
    {
      if (IS_ERR(send_pkt_info))
      {
      printf("[Info] Client ACK in response to File error message Lost, exit gracefully..\n");
      errno = 0;
      }
      else
      {
        printf("[Error] Timed out Sending File Name, giving Up\n");
        errno = ETIMEDOUT;
      }
      free_pkt_info(send_pkt_info);
      free(send_pkt);
      return -1;
    }
    retransmit = TRUE;
    printf("[Timeout] Retransmitting Port number through both Listening and "
        "Connected Socket: next RTO:%d ms\n", rttinfo.rtt_rto);
    free(send_pkt);
    goto sendagain;
  }
 
  /* Now wait for the ACK of our Port message from the client */
  while (1)
  {
    if ((n = read(sockfd, in_packet, PACKET_LEN)) < 0)
    {
      if (errno == EINTR) 
        continue;
      else
        err_sys("[Error] Read Error while waiting for ACK of our Port message");
    }

    rcv_pkt_info = get_packet_info(in_packet, n);

    if (IS_ACK(rcv_pkt_info) && (rcv_pkt_info->ack == (send_pkt_info->seq+1)))
    {
      break;
    }
    else
    {
      free_pkt_info(rcv_pkt_info);
      continue;
    }
  }

  set_alarm(0);     /* Turn off the Alarm */

  /* Update RTO */
  rtt_stop(&rttinfo, rtt_ts(&rttinfo) - rcv_pkt_info->timestamp);

  /* If we got an ACK for our message indicating an error, then exit gracefully */
  if(NULL == f)
  {
    printf("[Info] Client ACK in response to File error message received, exit gracefully..\n");
    errno = 0;
    free_pkt_info(send_pkt_info);
    free_pkt_info(rcv_pkt_info);
    free(send_pkt);
    return -1;
  }

  printf("[RTT Info] RTO updated, next rto: %d ms\n", rttinfo.rtt_rto);

  printf("[Info] Client received Server Port correctly, closing listening socket, ready for file transfer.\n\n");

  close(ifi_array[listenfd_idx]->sockfd);

  free_pkt_info(send_pkt_info);
  free_pkt_info(rcv_pkt_info);
  free(send_pkt);

  /* Initiate File Transfer */
  return (file_transfer(sockfd, f, config, init_seq+1));
}

int main(int argc, char **argv)
{

  int on=1, i, j, n, maxfdp, nready, num_ifi = 0;
  struct sockaddr_in servaddr, cliaddr;
  ifi_t *ifi_array[IFI_MAX];
  fd_set rset;
  server_config_t config;
  sigset_t x;
  pid_t pid, child_pid, server_child_pid;
  socklen_t sock_len = sizeof(cliaddr);
  char rcv_line[LINE_MAX], client_id[ADDR_LEN], tmp[ADDR_LEN];
  short client_port;

  read_server_config("server.in", &config);
  print_server_config(&config);

  num_ifi = get_ifi(ifi_array);
  print_ifi(ifi_array, num_ifi);

  bind_ifi(ifi_array, num_ifi, config.server_port);

  init_client_list(&client_list);

  Signal(SIGCHLD, sig_child_handler);

  for(;;)
  {
    FD_ZERO(&rset);
    
    maxfdp = ifi_array[0]->sockfd;

    for (j=0;j < num_ifi; j++) {

      FD_SET(ifi_array[j]->sockfd, &rset);
      if(ifi_array[j]->sockfd > maxfdp)
        maxfdp = ifi_array[j]->sockfd;
    }

    maxfdp++;

    nready = select(maxfdp, &rset, NULL, NULL, NULL);

    if (nready < 0 && errno == EINTR) {
      continue;
    }

    for (i = 0; i < num_ifi; i++) 
    {
      if (FD_ISSET(ifi_array[i]->sockfd, &rset)) 
      {

        if ((n = recvfrom(ifi_array[i]->sockfd, rcv_line, LINE_MAX, 0, 
                (SA*) &cliaddr, &sock_len)) < 0)
          continue;

        /* Create a Client ID as [IP:Port] and check if it already exists */
        Inet_ntop(AF_INET, &cliaddr.sin_addr, tmp, sizeof(tmp));
        client_port = ntohs(cliaddr.sin_port);
        snprintf(client_id, sizeof(client_id), "%s:%hu", tmp, client_port);


        /* Block SIGCHLD while traversing the Client List.
         * This is because the CHLD handler can delete an item from the list
         * when it receives a SIGCHLD from a termintating server child.
         * And all hell can break lose.
         */
        sigemptyset (&x);
        sigaddset(&x, SIGCHLD);
        sigprocmask(SIG_BLOCK, &x, NULL);
        server_child_pid = find_in_client_list(&client_list, client_id);
        sigprocmask(SIG_UNBLOCK, &x, NULL);
        
        /* If a server_child exists for the same client:port, ignore */
        if(server_child_pid >= 0)
        {
          printf("[Info] Duplicate(possibly retransmitted) Request from Client %s\n"
              "Existing Child process [%d] working on it already\n", 
              client_id, server_child_pid);
          continue;
        }

        printf("[New Connection] from [%s]\n", client_id); 

        /* Fork off the child */ 
        if ((pid = fork()) < 0) {
          err_quit("[Error] Fork failure..\n");
        }
        if (pid == 0) {

          /* Child Process */
          child_pid = getpid();
          printf("[Info] New Server Child [%d]  to Handle Client ID [%s]\n", child_pid, client_id);

          /* Close all sockets except the listening one*/
          for (j = 0; j < num_ifi; j++) 
          {
            if (i == j)
              continue;
            close(ifi_array[j]->sockfd);
          }

            packet_info_t *pkt_info = get_packet_info(rcv_line, n);

            dg_server_child(ifi_array, i, &cliaddr, pkt_info, &config); 
            exit(0);
        }
        
        /* Back to Parent */

        /* Block SIGCHLD while manipulating the Client List. */
        sigemptyset (&x);
        sigaddset(&x, SIGCHLD);
        sigprocmask(SIG_BLOCK, &x, NULL);
        add_to_client_list(&client_list, client_id, pid);
        print_client_list(&client_list);
        sigprocmask(SIG_UNBLOCK, &x, NULL);
      } /* if (FD_ISSET() */
    } /* for (i = 0; i < num_ifi; i++) */
  } /* for(;;) */
}
