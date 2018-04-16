#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char **argv){
        int child, parent=getpid();
printf(1,"nice:%d\n",getnice(parent));
        //start
        printf(1, "Start\n");
        if((child = fork()) == 0) { /* child executes later because new process is added to the end of the queue. */
                printf(1, "2\n");
                setnice(getpid(), 2);  // child's current nice value is set to 2, and this system call calls sched(). 

                if(fork() == 0) { // creates grand child process
                        printf(1, "3\n");  // the grand child process's priority is the highest, i.e., 0
                        yield();
                } else { // child's priority 2 is higher than the parent's 3. 
                        printf(1, "4\n");
                        wait();
                }
        }
        else {  /* parent executes first. */
                printf(1, "1\n");
                setnice(parent, 3);  // parent process' nice level should be 3.
                wait();
                printf(1, "5\n");
        }
        //end
        exit();
}

