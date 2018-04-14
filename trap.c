#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

// PA #2
const int timeslices[4] = {1, 2, 4, 8};
char* state_code2str[] = {"UNUSED", "EMBRYO", "SLEEPING", "RUNNABLE", "RUNNING", "ZOMBIE"};
extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;
#include "proc_type.h"
extern queue mlfq[4];
extern int is_runnable[NPROC];
extern int test;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(proc->killed)
      exit();
    proc->tf = tf;
    syscall();
    if(proc->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpunum() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpunum(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
    if(proc == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpunum(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            proc->pid, proc->name, tf->trapno, tf->err, cpunum(), tf->eip,
            rcr2());
    proc->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  //cprintf("%d %d %d\n", proc, proc && proc->state == RUNNING, tf->trapno == T_IRQ0+IRQ_TIMER);
	if(proc && proc->state == RUNNING && tf->trapno == T_IRQ0+IRQ_TIMER)
	{
//  acquire(&ptable.lock);
//  
//  for (int i = 0; i < NPROC; i++){
//	  if (ptable.proc[i].state == UNUSED) continue;
//	  //if (ptable.proc[i].name[0] != 's') continue;
//	  
//	  cprintf(state_code2str[ptable.proc[i].state]);
//	  cprintf(" proc: %d id: %d name: %s mlfq0: %s mlfq1: %s mlfq2: %s mlfq3: %s is_runnable: %d", 
//	  proc, ptable.proc[i].pid,
//	  ptable.proc[i].name,
//	  empty(mlfq + 0) ? "empty":"filled",
//	  empty(mlfq + 1) ? "empty":"filled",
//	  empty(mlfq + 2) ? "empty":"filled",
//	  empty(mlfq + 3) ? "empty":"filled",
//	  is_runnable[i]);
//	  cprintf("\n");
//  }
//  release(&ptable.lock);
  
		
		proc->ticks++;
		proc->timeslice++;
//		cprintf("ticks: %d cur_tick: %d timeslice: %d\n", proc->ticks, proc->timeslice, timeslices[proc->niceness]);
		if (proc->timeslice >= timeslices[proc->niceness])
		{
			//proc->timeslice = 0;
//			cprintf("yield();\n");
			yield();
		}
		
//		yield();
		// shouldn't use yield() when scheduling
	}
  

  // Check if the process has been killed since we yielded
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit();
}
