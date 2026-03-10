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
  int n = 5;
  int s = 3;
  float t = 5.0f;
  float i_interval = 1.0f;
  char logfile[256] = "oss.log";

int opt;
while ((opt = getopt(argc, argv, "hn:s:t:i:f:")) != -1) {
  switch (opt) {
case 'h':
  printf("Usage: oss [-h] [-n proc] [-s simul] [-t timelimit] [-i interval] [-f logfile]\n");
  return 0;
case 'n': n = atoi(optarg); break;
case 's': s = atoi(optarg); break;
case 't': t = atof(optarg); break;
case 'i': i_interval = atof(optarg); break;
case 'f': strncpy(logfile, optarg, sizeof(logfile)-1); break;
default:
  fprintf(stderr, "Unknown option. Use -h for help.\n");
  return 1;
  }
}

logFp = fopen(logfile, "w");
if (!logFp) {
  perror("fopen logfile");
  return 1;
}

signal(SIGINT, signalHandler);
signal(SIGTERM, signalHandler);
signal(SIGALRM, signalHandler);
alarm(60);

key_t shmKey = ftok(oss.c", 42);
shmId = shmget(shmKey, sizeof(SimClock), IPC_CREAT | 0666);
if (shmId < 0) { perror("shmget"); cleanup(); return 1; }
clock_ptr = (SimClock *)shmat(shmId, NULL, 0);
if (clock_ptr == (void*)-1) { perror("shmat"); cleanup();
 return 1; }
clock_ptr -> seconds = 0;
clock_ptr -> nanoseconds = 0;

key_t msgKey = ftok("oss.c", 99);
msgId = msgget(msgKey, IPC_CREAT | 0666);
if (msgId < 0) { perror("msgget"); cleanup(); return 1; }

initProcessTable();

srand(time(NULL));

int launched = 0;
int nextChild = 0;

long launchIntervalNs = (long)(i_interval * NANO_PER_SEC);
long lastLaunchNs = -(launchIntervalNs + 1);

while (1) {
  int active = countActiveChildren();

  if (launched >= n && active == 0) break;

  incrementClock(active > 0 ? active : 1);

  long nowNs = (long)clock_ptr -> seconds * NANO_PER_SEC + clock_ptr -> nanoseconds;

  if (launched < n && active < s && (nowNs - lastLaunchNs) >= launchIntervalNs) {
    int slot = findFreeSlot();
    if (slot != -1) {
      int maxSec = (int)t;
      if (maxSec < 1) maxSec = 1;
      int termSec = (rand() % maxSec) + 1;
      int termNano = rand() % NANO_PER_SEC;

      int fracNano = (int)((t - (int)t) * NANO_PER_SEC);
      if (termSec == maxSec && frackNano > 0) {
        termNano = rand() % (fracNano + 1);
      }

      char secArg[32], nanoArg[32]; 
      snprintf(secArg, sizeof(secArg), "%d", termSec);
      snprintf(nanoArg, sizeof(nanoArg), "%d", termNano);

      pid_t pid = fork();
      if (pid < 0) { perror("fork"); cleanup(); return 1; {
      if (pid == 0) {
        execl("./worker", "worker", secArg, nanoArg, NULL);
        perror("execl");
        exit(1);
      }

      int numActive = countActiveChildren() + 1;
      updatePCBOnLaunch(slot, pid, termSec, termNano, numActive);
      launched++;
      totalLaunched++;
      lastLaunched = nowNs;

      logAndPrint("OSS: Launched worker PID %d in slot %d at time %d:%d\n", pid, slot, clock_ptr -> seconds, clock_ptr -> nanoseconds);
      printProcessTable();
      }
                   }

      active = countActiveChildren();
      if (active == 0) continue;

      int found = -1;
      for (int attempt = 0; attempt < MAX_PROCESSES; attempt++) {
        int idx = (nextChild + attempt) % MAX_PROCESSES;
        if (processTable[idx].occupied) {
          found = idx;
          nextChild = (idx + 1) % MAX_PROCESSES;
          break;
        }
      }
      if (found == -1) continue;

      pid_t targetPid = processTable[found].pid;

      Message outMsg;
      outMsg.mtype = targetPid;
      outMsg.mvalue = 1;
      if (msgsnd(msgId, &outMsg, sizeof(int), 0) < 0) {
        perror("msgsnd");
      }
      processTable[found].messagesSent++;
      totalMessages++;
      logAndPrint("OSS: Sending message to worker %d PID %d at time %d:%d\n", found, targetPid, clock_ptr -> seconds, clock_ptr -> nanoseconds);

      Message inMsg;
      if (msgrcv(msgId, &inMsg, sizeof(int), (long)getpid(), 0) < 0) {
        if (errno != EINTR) perror("msgrcv");
      } else {
        logAndPrint("OSS: Receiving message from worker %d PID %d at time %d:%d\n", found, targetPid, clock_ptr -> seconds, clock_ptr -> nanoseconds);

        if (inMsg.mvalue == 0) {
          logAndPrint("OSS: Worker %d PID %d is planning to terminate.\n", found, targetPid);
          waitpid(targetPid, NULL, 0);
          clearPCBSlot(found);
        }
      }

      pid_t dead;
        while ((dead = waitpid(-1, NULL, WNOHANG)) > 0) {
          int dslot = findSlotByPid(dead);
          if (dslot != -1) clearPCBSlot(dslot);
        }

      nowNs = (long)clocl_ptr -> seconds * NANO_PER_SEC + clock_ptr -> nanoseconds);
      if (nowNs - lastTablePrint >= HALF_SEC_NS) {
        printProcessTable();
        lastTablePrint = nowNs;
      }
    }

    logAndPrint("\n=== OSS Ending Report ===\n");
    logAndPrint("Total processes launched : %d\n", totalLaunched);
    logAndPrint("Total messages sent: %d\n", totalMessages);

    cleanup();
    return 0;
  }
