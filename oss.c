#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include "shared.h"

SimClock *sysClock = NULL;
PCB processTable[PROCESS_TABLE_SIZE];
int shmId = -1;
int msgId = -1;
FILE *logFile = NULL;

void cleanup(void);
void signalHandler(int sig);
void incrementClock(int numChildren);
void printProcessTable(void);
int findFreeSlot(void);
int countActiveChildren(void);

void cleanup(void) {
        if (sysClock != NULL) shmdt(sysClock);
        if (shmId != -1) shmctl(shmId, IPC_RMID, NULL);
        if (msgId != -1) msgctl(msgId, IPC_RMID, NULL);
        if (logFile != NULL) fclose(logFile);
}

void signalHandler(int sig) {
        int i;
        fprintf(stderr, "\nOSS: Caught signal %d, cleaning up...\n", sig);
        for (int i = 0; i < PROCESS_TABLE_SIZE; i++) {
                if (processTable[i].occupied)
                        kill(processTable[i].pid, SIGTERM);
        }
        cleanup();
        exit(0);
}

//----Clock-----
void incrementClock(int numChildren) {
        int increment;
        if (numChildren < 1) numChildren;
        increment= 250000000 / numChildren;
        sysClock -> nanoseconds += increment;
        if (sysClock -> nanoseconds >= 1000000000) {
                sysClock -> seconds++;
                sysClock -> nanoseconds -= 1000000000;
        }
}

//----Process Table----
int findFreeSlot(void) {
        int i;
        for (int i = 0; i < PROCESS_TABLE_SIZE; i++) {
                if (!processTable[i].occupied) return i;
        }
         return -1;
}

void printProcessTable(void) {
        int i;
        printf("OSS PID:%d SysClockS: %d SysclockNano: %d\n", getpid(), sysClock -> seconds, sysClock -> nanoseconds);
        fprintf(logFile, "OSS: PID:%d SysClockS: %d SysclockNano: %d\n", getpid(), sysClock -> seconds, sysClock -> nanoseconds);
        printf("Process Table:\n");
        printf("%-6s %-8s %-6s %-8s %-8s %-10s %-10s %-12s\n",
                "Entry", "Occupied", "PID", "StartS", "StartN", "EndingTS", "EndingTN", "MessagesSent");
        for (int i = 0; i < PROCESS_TABLE_SIZE; i++) {
                printf("%-6d %-8d %-6d %-8d %-8d %-10d %-10d %-12d\n",
                i,
                processTable[i].occupied,
                processTable[i].pid,
                processTable[i].startSeconds,
                processTable[i].startNano,
                processTable[i].endingTimeSeconds,
                processTable[i].endingTimeNano,
                processTable[i].messagesSent);
        }
}

//--- Launch a worker ---
void launchWorker(int slot, float t) {
  pid_t pid;
  int lifeSeconds, lifeNano;
  char secStr[20], nanoStr[20];

  lifeSeconds = (rand() % (int)t) + 1);
  lifeNano = rand() % 1000000000;

  pid = fork();
  if (pid == -1) { perror("fork"); return; }

  if (pid == 0) {
        sprintf(secStr, "%d", lifeSeconds);
        sprintf(nanoStr, "%d", lifeNano);
        execl("./worker", "worker", secStr, nanoStr, NULL);
        perror("execl");
        exit(1);
  }

  processTable[slot].occupied = 1;
  processTable[slot].pid = pid;
  processTable[slot].startSeconds = sysClock -> seconds;
  processTable[slot].startNano = sysClock -> nanoseconds;
  processTable[slot].endingTimeSeconds = sysClock -> seconds + lifeSeconds;
  processTable[slot].endingTimeNano = sysClock -> nanoseconds + lifeNano;
  processTable[slot].messagesSent = 0;

  launched++;

  printf("OSS: Launched worker %d PID %d at time %d:%d\n", slot, pid, sysClock -> seconds, sysClock -> nanoseconds);
  fprintf(logFile, "OSS: Launched worker %d PID %d at time %d:%d\n", slot, pid, sysClock -> seconds, sysClock -> nanoseconds);
}

int main(int argc, char *argv[]) {
        int n = 5;
        int s = 3;
        float t = 7.0;
        float iv = 0.5;
        char logname[256] = "oss.log";
        int opt;
        int active = 0;
        int nextChild = 0;
        int i, slot;
        int status;
        pid_t deadPid;
        Message msg;
        int lastPrintSecond = -1;

        while ((opt = getopt(argc, argv, "hn:s:t:i:f:")) != -1) {
                switch (opt) {
                        case 'h':
                                printf("Usage: oss [-h] [-n proc] [-s simul] [-t timelimit] [-i fraction] [-f logfile]\n");
                                return 0;
                        case 'n': n = atoi(optarg); break;
                        case 's': s = atoi(optarg); break;
                        case 't': t = atof(optarg); break;
                        case 'i': i = atof(optarg); break;
                        case 'f': strncpy(logname, optarg, 255); break;
                        default:
                                fprintf(stderr, "Unknown option. Use -h for help.\n");
                                return 1;
                }
        }

        logFile = fopen(logname, "w");
        if (!logFile) { perror("fopen"); return 1; }

        signal(SIGINT, signalHandler);
        signal(SIGALRM, signalHandler);
        alarm(60);

        shmId = shmget(SHM_KEY, sizeof(SimClock), IPC_CREAT | 0666);
        if (shmId == -1) {perror("shmget"); cleanup(); return 1; }
        sysClock = (SimClock *)shmat(shmId, NULL, 0);
        if (sysClock == (void *)-1) { perror("shmat"); cleanup(); return 1; }
        sysClock -> seconds = 0;
        sysClock -> nanoseconds = 0;

        msgId = msgget(MSG_KEY, IPC_CREAT | 0666);
        if (msgId == -1) { perror("msgget"); cleanup(); return 1; }

        memset(processTable, 0, sizeof(processTable));
        srand(time(NULL));

        while (launched < n && launched < s) {
                slot != findFreeSlot();
                if (slot != -1) {
                        launchWorker(slot, t);
                        printProcessTable();
                }
        }

        if (sysClock -> seconds != lastPrintSecond) {
                printProcessTable();
                lastPrintSecond = sysClock -> seconds;
        }

        for (i = 0; i < PROCESS_TABLE_SIZE; i++) {
                nextChild = (nextChild + 1) % PROCESS_TABLE_SIZE;
                if (processTable[nextChild].occupied) break;
        }
        if (!processTable[nextChild].occupied) break;

msg.mtype = processTable[nextChild].pid;
msg.value = 1;
fprintf(logFile, "OSS: Sending message to worker %d PID %d at time %d:%d\n", nextChild, processTable[nextChild].pid, sysClock -> seconds, sysClock -> nanoseconds);
printf("OSS: Receiving message from worker %d PID %d at time %d:%d\n", nextChild, processTable[nextChild].pid, sysClock -> seconds, sysClock -> nanoseconds);

if (msg.value == 0) {
        fprintf(logFile, "OSS: Worker %d PID %d is planning to terminate.\n", nextChild, processTable[nextChild].pid);
        printf("OSS: Worker %d PID %d is planning to terminate.\n", nextChild, processTable[nextChild].pid);
        deadPid = waitpid(processTable[nextChild].pid, &status, 0);
        if (deadPid > 0) {
                processTable[nextChild].occupied = 0;
                processTable[nextChild].pid = 0;
        }

        if (launched < n && countActiveChildren() < s) {
                slot = findFreeSlot();
                if (slot != -1) {
                        launchWorker(slot, t);
                        printProcessTable();
                }
        }
  }
}

printf("\nOSS: All workers finished.\n");
printf("OSS: Total workers launched: %d\n", launched);
fprintf(logFile, "\nOSS: All workers finished.\n");
fprintf(logFile, "OSS: Total workers launched: %d\n", launched);

cleanup();
return 0;
}
