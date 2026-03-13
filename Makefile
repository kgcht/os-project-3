CC = gcc
CFLAGS = -Wall -g -std=c99

all: oss worker

oss: oss.c shared.h
        $(CC) $(CGLAGS) -o oss oss.c

worker: worker.c shared.h
        $(CC) $(CFLAGS) -o worker worker.c

clean:
        rm -f oss worker *.o oss.log


