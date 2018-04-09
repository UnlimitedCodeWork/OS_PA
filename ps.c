//PA #1

#include "types.h"
#include "stat.h"
#include "user.h"
#include "proc_type.h"

#define BUF_SIZE 34

void ps(int fd)
{
	struct ps_info info;
	ps_inside(fd, &info);
	
	printf(2, "pid      nice state      name\n");
	printf(2, "-----------------------------\n");
	for (int i = 0; i < info.arr_len; i++){
		char *name = info.arr[i].name;
		int niceness = info.arr[i].niceness;
		char *state = info.arr[i].state;
		int pid = info.arr[i].pid;
		
		char buf[BUF_SIZE];
		int digits;
		int i;
		
		memset(buf, ' ', BUF_SIZE);
		for (digits = (int)1e7, i = 0; digits >= 1; digits /= 10) {
			int div = pid / digits;
			int mod = pid % digits;
			if (div != 0) {
				buf[i++] = div + '0';
			}
			pid = mod;
		}
		if (niceness / 10 == 0) {
			buf[9] = niceness + '0';
		}
		else {
			buf[9] = niceness / 10 + '0';
			buf[10] = niceness % 10 + '0';
		}
		memmove(buf + 14, state, strlen(state));
		memmove(buf + 25, name, strlen(name));
		buf[BUF_SIZE - 1] = '\0';
		
		printf(2, "%s\n", buf);
	}
	
}

int main(int argc, char** argv) {
	char *pid_str = argv[1];
	int pid;
	
	for (pid = 0; *pid_str != '\0'; pid_str++) {
		pid = 10 * pid + *pid_str - '0';
	}
	ps(pid);
	
	exit();
}
