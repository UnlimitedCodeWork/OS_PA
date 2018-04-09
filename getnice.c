//PA #1

#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char** argv) {
	char *pid_str = argv[1];
	int pid;
	
	for (pid = 0; *pid_str != '\0'; pid_str++) {
		pid = 10 * pid + *pid_str - '0';
	}
	
	printf(2, "niceness: %d\n", getnice(pid));
	
	exit();
}