#include "rtt_mod.h"

int		rtt_d_flag = 0;		/* debug flag; can be set by caller */

/*
 * Calculate the RTO value based on current estimators:
 *		smoothed RTT plus four times the deviation
 */
#define	RTT_RTOCALC(ptr) ((ptr)->rtt_srtt + (((ptr)->rtt_rttvar)) << 2)

static int rtt_minmax(int rto)
{
	if (rto < RTT_RXTMIN)
		rto = RTT_RXTMIN;
	else if (rto > RTT_RXTMAX)
		rto = RTT_RXTMAX;
	return rto;
}

void rtt_init(rtt_info_t *ptr)
{
	struct timeval	tv;

	Gettimeofday(&tv, NULL);
	ptr->rtt_base = tv.tv_sec;		/* # sec since 1/1/1970 at start */

	ptr->rtt_rtt    = 0;
	ptr->rtt_srtt   = 0;
	ptr->rtt_rttvar = 750;
	ptr->rtt_rto = rtt_minmax(RTT_RTOCALC(ptr));
        printf("Initial RTO is %d\n", ptr->rtt_rto);
	/* first RTO at (srtt + (4 * rttvar)) = 3 seconds */
}

/*
 * Return the current timestamp.
 * Our timestamps are 32-bit integers that count milliseconds since
 * rtt_init() was called.
 */

/* include rtt_ts */
uint32_t
rtt_ts(rtt_info_t *ptr)
{
	uint32_t		ts;
	struct timeval	tv;

	Gettimeofday(&tv, NULL);
	ts = ((tv.tv_sec - ptr->rtt_base) * 1000) + (tv.tv_usec / 1000);
	return(ts);
}

void rtt_newpack(rtt_info_t *ptr)
{
	ptr->rtt_nrexmt = 0;
}

int rtt_start(rtt_info_t *ptr)
{
	return ptr->rtt_rto;
	/* return value can be used as: alarm(rtt_start(&foo)) */
}

/*
 * A response was received.
 * Stop the timer and update the appropriate values in the structure
 * based on this packet's RTT.  We calculate the RTT, then update the
 * estimators of the RTT and its mean deviation.
 * This function should be called right after turning off the
 * timer with alarm(0), or right after a timeout occurs.
 */

/* include rtt_stop */
void rtt_stop(rtt_info_t *ptr, uint32_t ms)
{
	int delta;

        ptr->rtt_rtt = ms; /*  measured in milli seconds */

	/*
	 * Update our estimators of RTT and mean deviation of RTT.
	 * See Jacobson's SIGCOMM '88 paper, Appendix A, for the details.
	 */

	delta = ptr->rtt_rtt - ptr->rtt_srtt;
	ptr->rtt_srtt += (delta>>3);		/* g = 1/8 */

	if (delta < 0)
		delta = -delta;				/* |delta| */

	ptr->rtt_rttvar += ((delta - ptr->rtt_rttvar)>>2);	/* h = 1/4 */

	ptr->rtt_rto = rtt_minmax(RTT_RTOCALC(ptr));
}

int rtt_timeout(rtt_info_t *ptr)
{
	ptr->rtt_rto <<= 1;	/* Double the next RTO */

	ptr->rtt_rto = rtt_minmax(RTT_RTOCALC(ptr));
	
        if (++ptr->rtt_nrexmt > RTT_MAXNREXMT)
		return 1;			

	return 0;
}

void rtt_debug(rtt_info_t *ptr)
{
	if (rtt_d_flag == 0)
		return;

	fprintf(stderr, "rtt = %d, srtt = %df, rttvar = %d, rto = %d\n",
			ptr->rtt_rtt, ptr->rtt_srtt, ptr->rtt_rttvar, ptr->rtt_rto);
	fflush(stderr);
}

