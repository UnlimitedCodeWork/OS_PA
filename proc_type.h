//PA #1
#include "param.h"

struct ps_info_record {
	int pid;
	int niceness;
	char state[11];
	char name[16];
};
struct ps_info {
	int arr_len;
	struct ps_info_record arr[NPROC];
};

#ifndef _PSTAT_H_
#define _PSTAT_H_

struct pstat {
	int inuse[NPROC];	// isn't UNUSED
	int nice[NPROC];	// nice
	int pid[NPROC];	// pid
	int ticks[NPROC];	// num of ticks accumulated
};

#endif

#ifndef _QUEUE_H_
#define _QUEUE_H_

typedef struct queue {
	int front;
	int rear;
	int pids[NPROC + 1]; // ptable slot id's
} queue;

void init_queue(queue*);
void enque(queue*, int);
int deque(queue*);
int rear_deque(queue*);
int deque_by_idx(queue*, int);
int empty(queue*);
int front(queue*);
void print_queue(queue*);

#endif