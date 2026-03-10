#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#define MAX_PROCESSES 20
#define NANO_PER_SEC 1000000000
#define HALF_SEC_NS 500000000

typedef struct {
  int seconds;
  int nanoseconds;
} SimClock;

typedef struct {
  int occupied;
  pid_t pid;
  int startSeconds;
  int startNano;
  int endingTimeSeconds;
  int endingTimeNano;
  int messagesSent;
} PCB;

typedef struct {
  long mtype;
  int mvalue;
} Message;

static int shmId = -1;
static int msgId = -1;
static SimClock *clock_ptr = NULL;
static PCB processTable[MAX_PROCESSES];

static int totalLaunched = 0;
static int totalMessages = 0;
static long lastTablePrint = 0;

static FILE *logFp = NULL;

void cleanup(void);
void signalHandler(int sig);
void initProcessTable(void);
void printProcessTable(void);
void incrementClock(int numChildren);
int findFreeSlot(void);
int countActiveChildren(void);
void updatePCBOnLaunch(int slot, pid_t pid, int termSec, int termNano, int numActive);
void clearPCBSlot(int slot);
int findSlotByPid(pid_t pid_;
void logAndPrint(const char *fmt, ...);

#include <stdarg.h>
void logAndPrint(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
  if (logFp) {
    va_start(args, fmt);
    vfprintf(logFp, fmt, args);
    va_end(args);
  }
}

void cleanup(void) {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (processTable[i].occupied) {
      kill(processTable[i].pid, SIGTERM);
    }
  }

  while (wait(NULL) > 0);

  if (clock_ptr) shmdt(clock_ptr);
  if (shmId != -1) shmctl(shmId, IPC_RMID, NULL);
  if (msgId != -1) msgctl(msgId, IPC_RMID, NULL);
  if (logFp) fclose(logFp);
}

void signalHandler(int sig) {
  (void)sig;
  logAndPrint("\nOSS: Signal received, cleaning up.\n");
  cleanup();
  exit(0);
}

void initProcessTable(void) {
  memset(processTable, 0, sizeof(processTable));
}

void printProcessTable(void) {
  logAndPrint("OSS PID:%d SysClockS: %d SysclockNano: %d\n", getpid(), clock_ptr -> seconds, clock_ptr -> nanoseconds);
  logAndPrint("Process Table:\n");
  logAndPrint("%-6s %-8s %-7s %-8s %-8s %-10s %-10s %-12s\n", "Entry", "Occupied", "PID", "StartS", "StartN", "EndingTS", "EndingTN", "MessagesSent");
  for (int i = 0; i < MAX_PROCESSES; i++) {
    logAndPrint("%-6d %-8d %-7d %-8d %-8d %-10d %-10d %-12d\n",
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

void incrementClock(int numChildren) {
  int inc = (numChildren > 0) ? (250000000 / numChildren) : 250000000;
  clock_ptr -> nanoseconds += inc;
  if (clock_ptr -> nanoseconds >= NANO_PER_SEC) {
    clock_ptr -> seconds++;
    clock_ptr -> nanoseconds -= NANO_PER_SEC;
  }
}

int findFreeSlot(void) {
  for (int i = 0; i < MAX_PROCESSES; i++)
    if (!processTable[i].occupied) return i;
  return -1;
}

int countActiveChildren(void) {
  int count = 0;
  for (int i = 0; i < MAX_PROCESSES; i++)
    if (processTable[i].occupied) count++;
  return count;
}

void updatePCBOnLaunch(int slot, pid_t pid, int termSec, int termNano, int numActive) {
  processTable[slot].occupied = 1;
  processTable[slot].pid = pid;
  processTable[slot].startSeconds = clock_ptr -> seconds;
  processTable[slot].startNano = clock_ptr -> nanoseconds;
  processTable[slot].messagesSent = 0;

int estEndNano = clock_ptr -> nanoseconds + termNano * numActive;
int estEndSec = clock_ptr -> seconds + termSec * numActive;
if (estEndNano >= NANO_PER_SEC) {
  estEndSec++;
  estEndNano -= NANO_PER_SEC;
}
processTable[slot].endingTimeSeconds = estEndSec;
processTable[slot].endingTimeNano = estEndNano;
}

void clearPCBSlot(int slot) {
  memset(&processTable[slot], 0, sizeof(PCB));
}

int findSlotByPid(pid_t pid) {
  for (int i = 0; i < MAX_PROCESSES; i++;)
    if (processTable[i].occupied && processTable[i].pid == pid)
      return i;
  return -1;
}

int main(int argc, char *argv[]) {
