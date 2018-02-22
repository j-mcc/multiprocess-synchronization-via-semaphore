/*
 * $Author: o1-mccune $
 * $Date: 2017/10/12 04:32:45 $
 * $Revison: $
 * $Log: simulatedclock.c,v $
 * Revision 1.5  2017/10/12 04:32:45  o1-mccune
 * Final Revision.
 *
 * Revision 1.4  2017/10/08 17:06:57  o1-mccune
 * Updated clock increment to 1 nanosecond.
 *
 * Revision 1.3  2017/10/07 21:06:56  o1-mccune
 * Updated increment and clock resolution to nanosecond.
 *
 * Revision 1.2  2017/10/07 16:14:28  o1-mccune
 * Added common functions that manipulate and interact with the simulated clock.
 *
 */

#include "simulatedclock.h"
#include <sys/shm.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

#define BILLION 1000000000

static int increment = 10000;  //Default is 20 nanoseconds

void setSimClockIncrement(int value){
  increment = value;
}

void resetSimClock(sim_clock_t *simClock){
  simClock->seconds = 0;
  simClock->nanoseconds = 0;
}

void incrementSimClock(sim_clock_t *simClock){
  if(simClock->nanoseconds + increment >= BILLION){
    simClock->seconds += 1;
    simClock->nanoseconds = (simClock->nanoseconds + increment) % BILLION;
  }
  else simClock->nanoseconds += increment;
}

int compareSimClocks(sim_clock_t *clock, sim_clock_t *compareTo){
  if(clock->seconds == compareTo->seconds){
    if(clock->nanoseconds == compareTo->nanoseconds) return 0;  //clock times are the same
    if(clock->nanoseconds < compareTo->nanoseconds) return -1;  //clock time is less than
    return 1;  //clock time is greater
  }
  else if(clock->seconds < compareTo->seconds){
    return -1;  //clock time is less than
  }
  else{
    return 1;  //clock time is greater than
  }
}

void addNanosecondsToSimClock(sim_clock_t *destination, sim_clock_t *source, int value){
    if(value % BILLION == 0){  //value is multiple of 1 second
      destination->seconds = source->seconds + value/BILLION;
      destination->nanoseconds = source->nanoseconds;
    }
    else if(value + source->nanoseconds < BILLION){  //can add value to nanoseconds without a seconds overflow
      destination->seconds = source->seconds;
      destination->nanoseconds = value + source->nanoseconds;
    }
    else if(value + source->nanoseconds > BILLION){  //take whole seconds from value and add to seconds, then add remaining to nanoseconds
      int seconds = floor((source->nanoseconds + value)/BILLION);  
      destination->seconds = source->seconds + seconds;
      destination->nanoseconds = (source->nanoseconds + value) - (seconds * BILLION);
    }
}

void copySimClock(sim_clock_t *source, sim_clock_t *destination){
  destination->seconds = source->seconds;
  destination->nanoseconds = source->nanoseconds;
}
