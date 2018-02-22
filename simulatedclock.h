/*
 * $Author: o1-mccune $
 * $Date: 2017/10/07 16:15:54 $
 * $Revison: $
 * $Log: simulatedclock.h,v $
 * Revision 1.3  2017/10/07 16:15:54  o1-mccune
 * Added function prototypes for simulated clock functions.
 *
 * Revision 1.2  2017/10/02 03:27:12  o1-mccune
 * Added clock_t definition.
 *
 */

#ifndef SIMULATEDCLOCK_H
#define SIMULATEDCLOCK_H

typedef struct{
  int seconds;
  int nanoseconds;
}sim_clock_t;

void setSimClockIncrement(int value);

void resetSimClock(sim_clock_t *simClock);

void incrementSimClock(sim_clock_t *simClock);

int compareSimClocks(sim_clock_t *clock, sim_clock_t *compareTo);

void addNanosecondsToSimClock(sim_clock_t *destination, sim_clock_t *source, int value);

void copySimClock(sim_clock_t *source, sim_clock_t *destination);

#endif
