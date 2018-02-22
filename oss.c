/*
 *  $Author: o1-mccune $
 *  $Date: 2017/10/12 04:31:29 $
 *  $Revision: 1.5 $
 *  $Log: oss.c,v $
 *  Revision 1.5  2017/10/12 04:31:29  o1-mccune
 *  Final revision.
 *
 *  Revision 1.4  2017/10/08 17:42:21  o1-mccune
 *  Updated for default and user specified logfile name and output to logfile.
 *
 *  Revision 1.3  2017/10/08 17:03:52  o1-mccune
 *  Updated for use with sharedmessage.h
 *
 *  Revision 1.2  2017/10/02 04:55:05  o1-mccune
 *  Added signal handling for SIGINT and SIGALRM.
 *  Added shared memory creation for semaphore as well as initialization of semaphore.
 *  Added cleanup for all shared memory locations.
 *
 *  Added initial forking code to test shared memory and semaphore.
 *  Added setup code for shared message and shared clock.
 *
 *  Revision 1.1  2017/10/01 21:22:44  o1-mccune
 *  Initial revision
 *
 */

#include "sharedmessage.h"
#include "simulatedclock.h"
#include <sys/wait.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>

static unsigned int maxProcessTime = 20;
static char defaultLogFilePath[] = "logfile.txt";
static char *logFilePath = NULL;
static FILE *logFile;
static unsigned int maxChildProcesses = 100;
static unsigned int numConcurrentProcesses = 5;
static shared_message_t *message;
static sem_t *semaphore;
static sim_clock_t *simClock;
static key_t clockSharedMemoryKey;
static key_t messageSharedMemoryKey;
static key_t semaphoreSharedMemoryKey;
static int semaphoreSharedMemoryId;
static int messageSharedMemoryId;
static int clockSharedMemoryId;
static pid_t *children;
static int childCounter = 0;

// Finds a childpid in the child pid list and returns its index
static int findId(pid_t childpid){
  int i;
  for(i = 0; i < numConcurrentProcesses; i++){
    if(children[i] == childpid) return i;
  }
  return -1;
}

static void printOptions(){
  fprintf(stderr, "OSS:  Command Help\n");
  fprintf(stderr, "\tOSS:  '-h': Prints Command Usage\n");
  fprintf(stderr, "\tOSS:  Optional '-l': Filename of log file. Default is logfile.txt\n");
  fprintf(stderr, "\tOSS:  Optional '-t': Input number of seconds before the main process terminates. Default is 20 seconds.\n");
  fprintf(stderr, "\tOSS:  Optional '-c': Input maximum number of child processes to create. Default is 100 child processes.\n");
  fprintf(stderr, "\tOSS:  Optional '-s': Input number of concurrent child processes. Default is 5, cannot exceed 19.\n");
}

static int parseOptions(int argc, char *argv[]){
  int c;
  int nCP = 0;
  while ((c = getopt (argc, argv, "ht:c:s:l:")) != -1){
    switch (c){
      case 'h':
        printOptions();
        abort();
      case 't':
        maxProcessTime = atoi(optarg);
        break;
      case 's':
        nCP = 1;
        numConcurrentProcesses = atoi(optarg);
        break;
      case 'c':
        maxChildProcesses = atoi(optarg);
        break;
      case 'l':
	logFilePath = malloc(sizeof(char) * (strlen(optarg) + 1));
        memcpy(logFilePath, optarg, strlen(optarg));
        logFilePath[strlen(optarg)] = '\0';
        break;
      case '?':
        if(optopt == 't'){
          maxProcessTime = 20;
	  break;
	}
	else if(isprint (optopt))
          fprintf(stderr, "OSS: Unknown option `-%c'.\n", optopt);
        else
          fprintf(stderr, "OSS: Unknown option character `\\x%x'.\n", optopt);
        default:
	  abort();
    }
  }
  //if(!nCP) maxChildProcesses < 20 ? numConcurrentProcesses = maxChildProcesses : 19;
  if(nCP && (numConcurrentProcesses > 19)){
    fprintf(stderr, "OSS: Cannot have more than 19 concurrent child processes.\n"); 
    fprintf(stderr, "OSS: Cleaning up...");
    free(logFilePath);
    fprintf(stderr, "Aborting\n");
    abort(); 
  }
  
  if(!logFilePath){
    logFilePath = malloc(sizeof(char) * strlen(defaultLogFilePath) + 1);
    memcpy(logFilePath, defaultLogFilePath, strlen(defaultLogFilePath));
    logFilePath[strlen(defaultLogFilePath)] = '\0';
  }

  return 0;
}

static int initSemaphoreSharedMemory(){
  if((semaphoreSharedMemoryKey = ftok("./oss", 1)) == -1) return -1;
  //fprintf(stderr, "OSS: Semaphore Shared Memory Key: %d\n", semaphoreSharedMemoryKey);
  if((semaphoreSharedMemoryId = shmget(semaphoreSharedMemoryKey, sizeof(sem_t), IPC_CREAT | 0644)) == -1){
    perror("OSS: Failed to get shared memory for semaphore");
    return -1;
  }
  //fprintf(stderr, "OSS: Semaphore Shared Memory ID: %d\n", semaphoreSharedMemoryId);
  return 0;
}

static int initMessageSharedMemory(){
  if((messageSharedMemoryKey = ftok("./oss", 2)) == -1) return -1;
  //fprintf(stderr, "OSS: Message Shared Memory Key: %d\n", messageSharedMemoryKey);
  if((messageSharedMemoryId = shmget(messageSharedMemoryKey, sizeof(shared_message_t), IPC_CREAT | 0644)) == -1){
    perror("OSS: Failed to get shared memory for message");
    return -1;
  }
  //fprintf(stderr, "OSS: Message Shared Memory ID: %d\n", messageSharedMemoryId);
  return 0;
}

static int initClockSharedMemory(){
  if((clockSharedMemoryKey = ftok("./oss", 3)) == -1) return -1;
  //fprintf(stderr, "OSS: Clock Shared Memory Key: %d\n", clockSharedMemoryKey);
  if((clockSharedMemoryId = shmget(clockSharedMemoryKey, sizeof(sim_clock_t), IPC_CREAT | 0644)) == -1){
    perror("OSS: Failed to get shared memory for clock");
    return -1;
  }
  //fprintf(stderr, "OSS: Clock Shared Memory ID: %d\n", clockSharedMemoryId);
  return 0;
}

static int removeSemaphoreSharedMemory(){
  if(shmctl(semaphoreSharedMemoryId, IPC_RMID, NULL) == -1){
    perror("OSS: Failed to remove semaphore shared memory");
    return -1;
  }
  return 0;
}

static int removeMessageSharedMemory(){
  if(shmctl(messageSharedMemoryId, IPC_RMID, NULL) == -1){
    perror("OSS: Failed to remove message shared memory");
    return -1;
  }
  return 0;
}

static int removeClockSharedMemory(){
  if(shmctl(clockSharedMemoryId, IPC_RMID, NULL) == -1){
    perror("OSS: Failed to remove clock shared memory");
    return -1;
  }
  return 0;
}

static int detachSemaphoreSharedMemory(){
  return shmdt(semaphore);
}

static int detachClockSharedMemory(){
  return shmdt(simClock);
}

static int detachMessageSharedMemory(){
  return shmdt(message);
}

static int attachSemaphoreSharedMemory(){
  if((semaphore = shmat(semaphoreSharedMemoryId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

static int attachMessageSharedMemory(){
  if((message = shmat(messageSharedMemoryId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

static int attachClockSharedMemory(){
  if((simClock = shmat(clockSharedMemoryId, NULL, 0)) == (void *)-1) return -1;
  return 0;
}

/*
 * Before detaching and removing IPC shared memory, use this to destroy the semaphore.
 */
static int removeSemaphore(sem_t *semaphore){
  if(sem_destroy(semaphore) == -1){
    perror("OSS: Failed to destroy semaphore");
    return -1;
  }
  return 0;
}

/*
 * After IPC shared memory has been allocated and attached to process, use this to initialize the semaphore. 
 */
static int initSemaphore(sem_t *semaphore, int processOrThreadSharing, unsigned int value){
  if(sem_init(semaphore, processOrThreadSharing, value) == -1){
    perror("OSS: Failed to initialize semaphore");
    return -1;
  }
  return 0;
}

static void cleanUp(int signal){
  int i;
  for(i = 0; i < numConcurrentProcesses; i++){
    if(children[i] > 0){
      if(signal == 2) fprintf(stderr, "Parent sent SIGINT to Child %d\n", children[i]);
      else if(signal == 14)fprintf(stderr, "Parent sent SIGALRM to Child %d\n", children[i]);
      kill(children[i], signal);
      waitpid(-1, NULL, 0);
    }
  }
  free(children);
  fclose(logFile);
  if(detachMessageSharedMemory() == -1) perror("OSS: Failed to detach message memory");
  if(removeMessageSharedMemory() == -1) perror("OSS: Failed to remove message memory");
  if(detachClockSharedMemory() == -1) perror("OSS: Failed to detach clock memory");
  if(removeClockSharedMemory() == -1) perror("OSS: Failed to remove clock memory");
  if(removeSemaphore(semaphore) == -1) fprintf(stderr, "OSS: Failed to remove semaphore");
  if(detachSemaphoreSharedMemory() == -1) perror("OSS: Failed to detach semaphore shared memory");
  if(removeSemaphoreSharedMemory() == -1) perror("OSS: Failed to remove seamphore shared memory");
  fprintf(stderr, "OSS: Total children created :: %d\n", childCounter); 
}

static void signalHandler(int signal){
  cleanUp(signal);
  exit(signal);
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

/*
 *  Transform an integer into a string
 */
static char *itoa(int num){
  char *asString = malloc(sizeof(char)*16);
  snprintf(asString, sizeof(char)*16, "%d", num);
  return asString;
}

int main(int argc, char **argv){
  int status;
  parseOptions(argc, argv);

  children = malloc(sizeof(pid_t) * numConcurrentProcesses);
  logFile = fopen(logFilePath, "w");


  if(initAlarmWatcher() == -1) perror("OSS: Failed to init SIGALRM watcher");
  if(initInterruptWatcher() == -1) perror("OSS: Failed to init SIGINT watcher");
  if(initSemaphoreSharedMemory() == -1) perror("OSS: Failed to init semaphore shared memory");
  if(attachSemaphoreSharedMemory() == -1) perror("OSS: Failed to attach semaphore shared memory");
  if(initSemaphore(semaphore, 1, 1) == -1) fprintf(stderr, "OSS: Failed to create semaphore");
  if(initMessageSharedMemory() == -1) perror("OSS: Failed to init message memory");
  if(attachMessageSharedMemory() == -1) perror("OSS: Failed to attach message memory");
  if(initClockSharedMemory() == -1) perror("OSS: Failed to init clock memory");
  if(attachClockSharedMemory() == -1) perror("OSS: Failed to attach clock memory");
 


  resetMessage(message);
  resetSimClock(simClock);
  alarm(maxProcessTime);
  pid_t childpid;
  
  //loop forks off initial number of concurrent processes; each child exec's to its actually program
  int i;
  for(childCounter = 0; childCounter < numConcurrentProcesses; childCounter++){
    if((childpid = fork()) > 0){  //parent code
      children[childCounter] = childpid;
    }
    else if(childpid == 0){  //child code
      execl("./child", "./child", "-s", itoa(semaphoreSharedMemoryId), "-m", itoa(messageSharedMemoryId), "-c", itoa(clockSharedMemoryId), NULL);
    }
    else{
      perror("OSS: Failed to fork");
    }
  }


  //create endTime condition
  sim_clock_t endTime;
  endTime.seconds = 2;
  endTime.nanoseconds = 0;

  //loop to increment simulated clock and read messages from child processes; if a message is received, a child is terminating and should be replaced.
  //this loop is valid until 2 seconds have passed in the simulated clock or maxChildProcesses have been created
  while(1){
    incrementSimClock(simClock);
    if(compareSimClocks(simClock, &endTime)  != -1){
      fprintf(stderr, "OSS: Out of Time\n"); 
      break;
    }
    if(messageEmpty(message)){  
      continue;
    }
    else{  // child is terminating. 
      //wait for child and get pid
      childpid = waitpid(-1, NULL, 0);
      if(childpid == -1) perror("OSS: Error waiting for child");
     
      //resolve pid as index in children table
      int id;
      if((id = findId(childpid)) != -1){
        children[id] = -1;
        //output message to logfile
        fprintf(stderr, "MASTER: Child %d is terminating at time %d.%10d because it reached %d.%10d in slave\n", childpid, simClock->seconds, simClock->nanoseconds, message->clock.seconds, message->clock.nanoseconds);
        fprintf(logFile, "MASTER: Child %d is terminating at time %d.%10d because it reached %d.%10d in slave\n", childpid, simClock->seconds, simClock->nanoseconds, message->clock.seconds, message->clock.nanoseconds);
        //fork another child
        if(childCounter < maxChildProcesses){
          if((childpid = fork()) > 0){ //parent code
            children[id] = childpid;  //store new child's pid in children list
            childCounter++;
          }
          else if(childpid == 0){  //child code
            execl("./child", "./child", "-s", itoa(semaphoreSharedMemoryId), "-m", itoa(messageSharedMemoryId), "-c", itoa(clockSharedMemoryId), NULL);
          }
          else perror("OSS: Failed to fork");
        }
        resetMessage(message); //clear message
      }
    }
  }
  cleanUp(2);

  return 0;
}


