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