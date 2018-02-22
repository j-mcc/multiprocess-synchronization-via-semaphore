#include "simulatedclock.h"
#include "sharedmessage.h"


void resetMessage(shared_message_t *message){
  message->clock.seconds = -1;
  message->clock.nanoseconds = -1;
}

int messageEmpty(shared_message_t *message){
  if(message->clock.seconds == -1 && message->clock.nanoseconds == -1) return 1;
  else return 0;
}

void setMessage(sim_clock_t *clock, shared_message_t *message){
  message->clock.seconds = clock->seconds;
  message->clock.nanoseconds = clock->nanoseconds;
}
