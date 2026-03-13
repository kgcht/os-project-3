#ifndef SHARED_H
#define SHARED_H

#include <sys/types.h>

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

#define PROCESS_TABLE_SIZE 20
#define SHM_KEY 0x1234
#define MSG_KEY 0x5678

#endif
