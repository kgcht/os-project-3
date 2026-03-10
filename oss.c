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

  if (clock_ptr)
