#ifndef SHAREDMESSAGE_H
#define SHAREDMESSAGE_H

#include "simulatedclock.h"

typedef struct{
  sim_clock_t clock;
}shared_message_t;

void resetMessage(shared_message_t *message);

int messageEmpty(shared_message_t *message);

void setMessage(sim_clock_t *clock, shared_message_t *message);

#endif
