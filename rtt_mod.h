#ifndef __RTT_H__
#define __RTT_H__

#include    "unp.h"

typedef struct rtt_info {
  int		rtt_rtt;	/* most recent measured RTT, in milliseconds */
  int		rtt_srtt;	/* smoothed RTT estimator, in milliseconds */
  int		rtt_rttvar;	/* smoothed mean deviation, in milliseconds */
  int		rtt_rto;	/* current RTO to use, in milliseconds */
  int		rtt_nrexmt;	/* # times retransmitted: 0, 1, 2, ... */
  uint32_t	rtt_base;	/* # sec since 1/1/1970 at start */
} rtt_info_t;

#define RTT_RXTMIN		1000 /* min retransmit timeout value, in milli seconds */ 
#define RTT_RXTMAX		3000 /* max retransmit timeout value, in milli seconds */
#define RTT_MAXNREXMT           12   /* max # times to retransmit */

void     rtt_debug(rtt_info_t *); 
void     rtt_init(rtt_info_t *); 
void     rtt_newpack(rtt_info_t *); 
int      rtt_start(rtt_info_t *); 
void     rtt_stop(rtt_info_t *, uint32_t);
int      rtt_timeout(rtt_info_t *); 
uint32_t rtt_ts(rtt_info_t *); 

extern int  rtt_d_flag; /* can be set to nonzero for addl info */

#endif 
