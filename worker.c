#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include "shared.h"

int main(int argc, char *argv[]) {
  int lifeSeconds, lifeNano;
  int termSeconds, termNano;
  int done;
  int msgCount;
  SimClock *sysClock;
  int shmId, msgId;
  Message msg; 

  if (argc != 3) {
        fprintf(stderr, "Usage: worker <seconds> <nanoseconds>\n");
        return 1;
  }

  lifeSeconds = atoi(argv[1]);
  lifeNano = atoi(argv[2]);

  shmId = shmget(SHM_KEY, sizeof(SimClock), 0666);
  if (shmId == -1) { perror("shmget"); return 1; }
  sysClock = (SimClock *)shmat(shmId, NULL, 0);
  if (sysClock == (void *)-1) { perror("shmat"); return 1; }

  msgId = msgget(MSG_KEY, 0666);
  if (msgId == -1) { perror("msgget"); return 1; }

  termSeconds = sysClock -> seconds + lifeSeconds;
  termNano = sysClock -> nanoseconds + lifeNano;
  if (termNano >= 1000000000) {
        termSeconds++;
        termNano -= 1000000000;
  }

  printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d\n", getpid(), getppid(), sysClock -> seconds, sysClock -> nanoseconds);
  printf("TermTimeS: %d TermTimeNano: %d\n", termSeconds, termNano);
  printf("--Just Starting\n");

  done = 0;
  msgCount = 0;

  do {
        msgrcv(msgId, &msg, sizeof(msg) - sizeof(long), getpid(), 0);
        msgCount++;

        if (sysClock -> seconds > termSeconds || (sysClock -> seconds == termSeconds && sysClock -> nanoseconds >= termNano)) {
        done = 1;
        }

        printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d\n", getpid(), getppid(), sysClock -> seconds, sysClock -> nanoseconds);
        printf("TermTimeS: %d TermTimeNano: %d\n", termSeconds, termNano);

        if (done) {
          printf("--Terminating after sending message back to oss after %d received messages.\n", msgCount);
        } else {
          printf("--%d message received from oss\n", msgCount);
        }

        msg.mtype = getppid();
        msg.value = done ? 0 : 1;
        msgsnd(msgId, &msg, sizeof(msg) - sizeof(long), 0);

  } while (!done);

  shmdt(sysClock);
  return 0;
}
