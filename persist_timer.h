#ifndef __PERSIST_TIMER_H__
#define __PERSIST_TIMER_H__

#define PT_MIN_SECONDS      5
#define PT_MAX_SECONDS      60
#define PERSIST_MAXNREXMT   12   /* max # of times to retransmit probes */

void reset_persist_timer();
void increment_persist_timer(); 
int  get_current_persist_timer();
int  persist_timeout();

#endif /* __PERSIST_TIMER_H__ */
