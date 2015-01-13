#include "congestion_ctrl.h"
#include <stdio.h>


#define MIN(a,b) (((a) < (b))? (a): (b))

void recalc_send_win_size(congestion_ctrl_info_t* cc_info)
{
  /* Flow Control Window = MIN(Sender Window, Receiver Window  */
  cc_info->sendwin = MIN(cc_info->send_conf_win, cc_info->recvwin);
  /* MIN(Flow Control Window, Congestion Window */
  cc_info->sendwin = MIN(cc_info->sendwin, cc_info->cwnd);
}

void init_congestion_ctrl_info(congestion_ctrl_info_t* cc_info, int send_conf_win)
{
  cc_info->rounded_cwnd = 1.0;
  cc_info->cwnd = 1;
  cc_info->ssthresh = 128;  /*  (64K/Segment Size)- A.S Tanenbaum  */
  cc_info->recvwin = 0;
  cc_info->sendwin = cc_info->cwnd;
  cc_info->send_conf_win = send_conf_win;
}

void congestion_detected(congestion_ctrl_info_t* cc_info, int event)
{
  cc_info->ssthresh = MIN(2, cc_info->cwnd / 2);	
  
  printf("[Congestion Detected] Event type:%s, adjust ssthresh to:%d\n", 
      event == CONGESTION_TIMEOUT ? "timeout" : "dup ack", cc_info->ssthresh);

  if (CONGESTION_TIMEOUT == event)
  {
    cc_info->cwnd = 1;
    cc_info->rounded_cwnd = 1.0;
  }
}

void transmission_acked(congestion_ctrl_info_t* cc_info)
{
  if (cc_info->cwnd <= cc_info->ssthresh)
  {
    /* In Slow Start */
    cc_info->cwnd += 1;
    cc_info->rounded_cwnd += 1.0;
    printf("[In Slow Start]: Adjust Congestion Window to %d\n", cc_info->cwnd);
  }
  else
  {
    /* Congestion Avoidance */
    cc_info->rounded_cwnd += 1.0 / cc_info->cwnd;
    cc_info->cwnd = (int)(cc_info->rounded_cwnd + 0.5);
    printf("[In Congestion Avoidance]: cwin = %d\n", cc_info->cwnd);
  }
}
