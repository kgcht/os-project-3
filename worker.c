#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>

#define NANO_PER_SEC 1000000000

typedef struct {
  int seconds;
  int nanoseconds;
} SimClock;

typedef struct {
  long mtype;
  int mvalue;
} Message;

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "Usage: worker <termSeconds> <termNanoseconds>\n");
    return 1;
  }

  int termSec = atoi(argv[1]);
  int termNano = atoi(argv[2]);

key_t shmKey = ftok("oss.c", 42);
int shmId = shmget(shmKey, sizeof(SimClock), 0666);
if (shmId < 0) { perror("worker shmget"); return 1; }
SimClock *clock_ptr = (SimClock *)shmat(shmId, NULL, 0);
if (clock_ptr == (void*)-1) { perror("worker shmat"); return 1; }

key_t msgKey = ftok("oss.c", 99);
int msgId = msgget(msgKey, 0666);
if (msgId < 0) { perror("worker msgget"); return 1; }

int targetSec = clock_ptr -> seconds + termSec;
int targetNano = clock_ptr -> nanoseconds + termNano;
if (targetNano >= NANO_PER_SEC) {
  targetSec++;
  targetNano -= NANO_PER_SEC;
}

printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d\n", getpid(), getppid(), clock_ptr -> seconds, clock_ptr -> nanoseconds);
printf("TermTimeS: %d TermTimeNano: %d\n", targetSec, targetNano);
printf("--Just Starting\n");
fflush(stdout);

int msgCount = 0;
int done = 0;
pid_t myPid = getpid();
pid_t my Ppid = getppid();

do {
  Message inMsg;
  if (msgrcv(msgId, &inMsg, sizeof(int), (long)myPid, 0) < 0) {
    perror("worker msgrcv");
    break;
  }
  msgCount++;

  int curSec = clock_ptr -> seconds;
  int curNano = clock_ptr -> nanoseconds;

  int shouldTerminate = 0;
  if (curSec > targetSec || (curSec == targetSec && curNano >= targetNano)) {
      shouldTerminate = 1;
  }

  printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d\n", myPid, myPpid, curSec, curNano);
  printf("TermTimeS: %d TermTimeNano: %d\n", targetSec, targetNano);
  if (shouldTerminate) {
    printf("--Terminating after sending message back to oss after %d received messages. \n", msgCount);
  } else {
    printf("--%d message%s received from oss\n", msgCount, msgCount == 1 ? "" : "s");
  }
  fflush(stdout);

  Message outMsg;
  outMsg.mtype = (long)myPpid;
  outMsg.mvalue = shouldTerminate ? 0 : 1;
  if (msgsnd(msgId, &outMsg, sizeof(int), 0) < 0) {
    perror("worker msgsnd");
  }
  if (shouldTerminate) done = 1;

} while (!done);

shmdt(clock_ptr);
return 0;
}
