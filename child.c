/*
 * $Author: o1-mccune $
 * $Date: 2017/10/12 04:31:51 $
 * $Revision: 1.6 $
 * $Log: child.c,v $
 * Revision 1.6  2017/10/12 04:31:51  o1-mccune
 * Final Revision.
 *
 * Revision 1.5  2017/10/08 17:10:09  o1-mccune
 * Updated for use with sharedmessage.h and removed unneccesary printf statements.
 *
 * Revision 1.4  2017/10/08 17:04:28  o1-mccune
 * Updated for use with sharedmessage.h
 *
 * Revision 1.3  2017/10/07 16:16:42  o1-mccune
 * Updated to use simulatedclock functions.
 *
 * Revision 1.2  2017/10/02 04:58:26  o1-mccune
 * Added signal handling for SIGINT and SIGALRM.
 * Added attachment of shared memory for semaphore.
 * Includes initial test loop for semaphore operations.
 *
 */

#include "sharedmessage.h"
#include "simulatedclock.h"
#include <signal.h>
#include <errno.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/shm.h>

static key_t sharedSemaphoreId;
static key_t sharedMessageId;
static key_t sharedClockId;
static sem_t *semaphore;
static sim_clock_t *simClock;
static shared_message_t *message;
//static char logFilePath[] = "ChildSemaphoreUseLogFile.txt\0";
//static FILE *logFile = NULL;

static void printOptions(){
  fprintf(stderr, "CHILD:  Command Help\n");
  fprintf(stderr, "\tCHILD:  Optional '-h': Prints Command Usage\n");
  fprintf(stderr, "\tCHILD:  '-m': Shared memory ID for shared message.\n");
  fprintf(stderr, "\tCHILD:  '-s': Shared memory ID for semaphore.\n");
  fprintf(stderr, "\tCHILD:  '-c': Shared memory ID for Operating System Simulator clock.\n");
}

static int parseOptions(int argc, char *argv[]){
  int c;
  int nCP = 0;
  while ((c = getopt (argc, argv, "hm:s:c:")) != -1){
    switch (c){
      case 'h':
        printOptions();
        abort();
      case 'c':
        sharedClockId = atoi(optarg);
        break;
      case 's':
        sharedSemaphoreId = atoi(optarg);
        break;
      case 'm':
	sharedMessageId = atoi(optarg);
        break;
      case '?':
	if(isprint (optopt))
          fprintf(stderr, "CHILD: Unknown option `-%c'.\n", optopt);
        else
          fprintf(stderr, "CHILD: Unknown option character `\\x%x'.\n", optopt);
        default:
	  abort();
    }
  }
  return 0;
}

static int detachSharedSemaphore(){
  return shmdt(semaphore);
}

static int detachSharedMessage(){
  return shmdt(message);
}

static int detachSharedClock(){
  return shmdt(simClock);
}

static int attachSharedSemaphore(){
  if((semaphore = shmat(sharedSemaphoreId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

static int attachSharedMessage(){
  if((message = shmat(sharedMessageId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

static int attachSharedClock(){
  if((simClock = shmat(sharedClockId, NULL, SHM_RDONLY)) == (void *)-1) return -1;
  return 0;
}

static void cleanUp(int signal){
  if(detachSharedSemaphore() == -1) perror("CHILD: Failed to detach shared semaphore");
  if(detachSharedMessage() == -1) perror("CHILD: Failed to detach shared message");
  if(detachSharedClock() == -1) perror("CHILD: Failed to detach shared clock");
  //if(logFile) fclose(logFile);
}

static void signalHandler(int signal){
  cleanUp(signal);
  exit(0);
}

static int initAlarmWatcher(){
  struct sigaction action;
  action.sa_handler = signalHandler;
  action.sa_flags = 0;
  return (sigemptyset(&action.sa_mask) || sigaction(SIGALRM, &action, NULL));
}

static int initInterruptWatcher(){
  struct sigaction action;
  action.sa_handler = signalHandler;
  action.sa_flags = 0;
  return (sigemptyset(&action.sa_mask) || sigaction(SIGINT, &action, NULL));
}


int main(int argc, char **argv){
  parseOptions(argc, argv);
  //if(fopen(logFilePath, "a") == NULL) perror("CHILD");

  if(initAlarmWatcher() == -1){
    perror("CHILD: Failed to init SIGALRM watcher");
    exit(1);
  }
  if(initInterruptWatcher() == -1){
    perror("CHILD: Failed to init SIGINT watcher");
    exit(2);
  }
  if(attachSharedSemaphore() == -1){
    perror("CHILD: Failed to attach semaphore");
    exit(3);
  }
  if(attachSharedMessage() == -1){
    perror("CHILD: Failed to attach message");
    exit(4);
  }
  if(attachSharedClock() == -1){
    perror("CHILD: Failed to attach clock");
    exit(5);
  }
  
  srand(time(0) + getpid()); 
  int aliveTime = rand() % 1000001;  //range is 0-1,000,000 microseconds
  sim_clock_t endTime;
  addNanosecondsToSimClock(&endTime, simClock, aliveTime);  
  //fprintf(stderr, "CHILD %d: Ends at time %d : %0d\n",getpid(), endTime.seconds, endTime.nanoseconds);

  while(1){
    int status;
    fprintf(stderr, "CHILD %d: Waiting on semaphore\n", getpid());
    if(sem_wait(semaphore) == -1) perror("CHILD");
    fprintf(stderr, "CHILD %d: Acquired  semaphore\n", getpid());
    int result;
    if((result = compareSimClocks(simClock, &endTime)) != -1){  //clock is >= endTime
      //try to terminate
      if(messageEmpty(message)){  //message is clear
        setMessage(simClock, message);
        fprintf(stderr, "CHILD %d: Passing semaphore\n", getpid());
        if(sem_post(semaphore) == -1) perror("CHILD");  //give up critical section
        break;  //break from loop
      }
    }
    fprintf(stderr, "CHILD %d: Passing semaphore\n", getpid());
    if(sem_post(semaphore) == -1) perror("CHILD");
  }
  //fprintf(stderr, "CHILD %d: Terminating\n", getpid());
  cleanUp(2);

  return 0;
}

