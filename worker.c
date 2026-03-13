#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include "shared.h"

int main(int argc, char *argv[]) {
  if (argc != 3) {
        fprintf(stderr, "Usage: worker <seconds> <nanoseconds>\n");
        return 1;
  }

  int lifeSeconds = atoi(argv[1]);
  int lifeNano = atoi(argv[2]);

  int shmId = shmget(SHM_KEY, sizeof(SimClock), 0666);
  if (shmId == -1) { perror("shmget"); return 1; }
  SimClock *sysClock = (SimClock *)shmat(shmId, NULL, 0);
  if (sysClock == (void *)-1) { perror("shmat"); return 1; }

  int msgId = msgget(MSG_KEY, 0666);
  if (msgId == -1) { perror("msgget"); return 1; }

  int termSeconds = sysClock -> seconds + lifeSeconds;
  int termNano = sysClock -> nanoseconds + lifeNano;
  if (termNano >= 1000000000) {
        termSeconds++;
        termNano -= 1000000000;
  }

  printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d\n", getpid(), getppid(), sysClock -> seconds, sysClock -> nanoseconds);
  printf("TermTimeS: %d TermTimeNano: %d\n", termSeconds, termNano);
  printf("--Just Starting\n");



  shmdt(sysClock);
  return 0;
}

