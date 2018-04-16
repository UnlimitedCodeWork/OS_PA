#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char **argv){
        int i=0, sum=0;
        int child;

        //start
        printf(1, "Start\n");
        if((child = fork()) == 0) { /* child executes later because new process is added to the end of the queue. */
                printf(1, "2\n"); // child's current nice value is 0, while parent's is 1, 2, or 3.
                if(fork() == 0) { // creates grand child process
                        printf(1, "5\n");  
                        yield();
                } else { // child continues as its priority is higher than the parent. 
                        printf(1, "3\n");
                        if(fork() == 0){
                                printf(1, "6\n");
                        } else{
                                printf(1, "4\n"); 
                                wait();
                        }
                        wait();
                }
        }
        else {  /* parent executes first. */
                printf(1, "1\n");
                for(i=0;i<1000;i++) sum+= i; // parent process' nice level should be 3.
                wait();
                printf(1, "7\n");
        }
        //end
        exit();
}

