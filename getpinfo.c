//PA #2

#include "types.h"
#include "stat.h"
#include "user.h"
#include "proc_type.h"

int main(int argc, char** argv) {
	struct pstat proc_state;
	
	if(getpinfo(&proc_state) == -1)
	{
		printf(1, "error: getpinfo: invalid pstat pointer\n");
	}
	
	printf(2, "used?\tnice\tpid\tticks\n");
	printf(2, "--------------------------\n");
	
	for (int i = 0; i < NPROC; i++) 
	{
		printf(2, "%s\t%d\t%d\t%d\n", 
		proc_state.inuse[i] ? "yes" : "no ",
		proc_state.nice[i],
		proc_state.pid[i],
		proc_state.ticks[i]);
	}
	
	exit();
}