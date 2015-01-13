#ifndef __CONGESTION_CTRL_H__
#define __CONGESTION_CTRL_H__

enum {
  CONGESTION_TIMEOUT,
  CONGESTION_DUP_ACK
};

typedef struct _congestion_ctrl_info {
	int cwnd;               /* Congestion Window */
	int ssthresh;           /* Slow Start Threshold */
	int recvwin;		/* receiver Advertised Window */
        int send_conf_win;      /* Configured Send Size.. i.e size of the Send Buffer */
	int sendwin; 		/* the minimum of cwnd, recvwin and send_conf_win */
	double rounded_cwnd;    /* Rounded Congestion Window */			
} congestion_ctrl_info_t;

void init_congestion_ctrl_info(congestion_ctrl_info_t* pctrl, int conf_window);
void congestion_detected(congestion_ctrl_info_t* cc_info, int event);
void transmission_acked(congestion_ctrl_info_t* cc_info);
void recalc_send_win_size(congestion_ctrl_info_t* pccinfo);

#endif /* __CONGESTION_CTRL_H__ */

