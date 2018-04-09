//PA #1

#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char** argv) {
	char *pid_str = argv[1];
	char *val_str = argv[2];
	int pid;
	int val;
	
	for (pid = 0; *pid_str != '\0'; pid_str++) {
		pid = 10 * pid + *pid_str - '0';
	}
	for (val = 0; *val_str != '\0'; val_str++) {
		val = 10 * val + *val_str - '0';
	}
	
	printf(2, setnice(pid, val) < 0 ? "failed\n" : "successful\n");
	
	exit();
}