#include "persist_timer.h"

static float _persist_timer = 1.5; 
static int   _num_retransmit = 0; 

void reset_persist_timer()
{
  _persist_timer = 1.5;
  _num_retransmit = 0; 
}

void increment_persist_timer() {
  _persist_timer *= 2;
  _num_retransmit++; 
}

int get_current_persist_timer()
{
  int timeout = (int)(_persist_timer); 
  if (timeout < PT_MIN_SECONDS) {
    timeout = PT_MIN_SECONDS;
  }
  else if (timeout > PT_MAX_SECONDS) {
    timeout = PT_MAX_SECONDS;
  }

  return timeout;
}

#define PERSIST_MAXNREXMT   12   /* max # of times to retransmit probes*/
int persist_timeout()
{
    return (_num_retransmit >= PERSIST_MAXNREXMT); 
}
