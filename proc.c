#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

// PA #1
#include "proc_type.h"

// PA #2
queue mlfq[4];
int is_runnable[NPROC];

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;
  p->niceness = 0;
  p->ticks = 0;
  p->timeslice = 0;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
  
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;

  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;
  proc->state = RUNNABLE;
  
  is_runnable[np - ptable.proc] = 1;
  enque(mlfq + 0, np - ptable.proc);
//  cprintf("fork(): %s, pid: %d, idx: %d runnable?: %d, nice: %d\n", 
//  proc->name, proc->pid, proc - ptable.proc, proc->state == RUNNABLE,
//  proc->niceness);
  
  sched();

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.

// PA #2
void init_queue(queue* q)
{
	q->front = 0;
	q->rear = 0;
}

void enque(queue* q, int pid)
{
	if (q->front == (q->rear + 1) % (NPROC + 1)) {
		return;
	}
	q->pids[(q->rear)++] = pid;
	q->rear %= (NPROC + 1);
}

int deque(queue* q)
{
	int ret;
	if (q->front == q->rear) {
		return -1;
	}
	ret = q->pids[(q->front)++];
	q->front %= (NPROC + 1);
	return ret;
}

int rear_deque(queue* q)
{
	int ret;
	if (q->front == q->rear) {
		return -1;
	}
	ret = q->pids[--(q->rear)];
	q->rear = (q->rear + NPROC + 1) % (NPROC + 1);
	return ret;
}

int deque_by_idx(queue* q, int idx)
{
	int ret;
	int index = -1;
	for (int i = q->front; i != q->rear; i = (i + 1)%(NPROC + 1)) {
		if (q->pids[i] == idx) {
			index = i;
			break;
		}
	}
	if (index == -1) {
		return -1;
	}
	ret = idx;
	
	for (int i = index; (i + 1)%(NPROC + 1) != q->rear; i = (i + 1)%(NPROC + 1)) {
		q->pids[i] = q->pids[(i + 1)%(NPROC + 1)];
	}
	q->rear = (q->rear - 1 + NPROC + 1) % (NPROC + 1);
	
	return ret;
}

int empty(queue* q)
{
	return q->front == q->rear;
}

int front(queue* q)
{
	return q->pids[q->front];
}

void print_queue(queue* q)
{
	for (int i = q->front; i != q->rear; i = (i + 1)%(NPROC + 1)){
		cprintf("%d=%s ", q->pids[i], ptable.proc[q->pids[i]].name);
	}
	cprintf("%s %d\n", empty(q) ? "empty":"filled", empty(q) ? 0 : front(q));
}

extern const int timeslices[4];

int test = 0;
void print_mlfq(char *s)
{
	cprintf("%s:\n", s);
	for(int i = 0; i < 4; i++){
		if (! empty(mlfq + i)){
			print_queue(mlfq + i);
		}
	}
	cprintf("actual RUNNABLEs: ");
	for(int i = 0; i < NPROC; i++){
		if (ptable.proc[i].state == RUNNABLE) {
			cprintf("%d ", i);
		}
	}
	cprintf("\nis_runnable: ");
	for(int i = 0; i < NPROC; i++){
		if(is_runnable[i]){
			cprintf("%d ", i);
		}
	}
	cprintf("\n\n");
}
void
scheduler(void)
{
  struct proc *p;
  int first5 = 100;
	
	for(int i = 0; i < 4; i++){
		init_queue(mlfq + i);
	}
	
	acquire(&ptable.lock);
	for(int i = 0; i < NPROC; i++){
		if (ptable.proc[i].state == RUNNABLE) {
			is_runnable[i] = 1;
			enque(mlfq + 0, i);
		}
		else {
			is_runnable[i] = 0;
		}
	}
	release(&ptable.lock);
	
//	print_mlfq("start");
  for(;;){
	  if(first5) first5--;
    // Enable interrupts on this processor.
    sti();
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    
#ifdef _NEW_SCHED_
//if(first5) print_mlfq("before choose");
	  int runnable_exists = 0;
	  int runnable_idx;
    
    // update is_runnable
    // add new runnables to queue 0
    for (int i = 0; i < NPROC; i++){
	    // currently runnable
	    if (ptable.proc[i].state == RUNNABLE){
		    if (! is_runnable[i]) {
			    // new runnable
			    enque(mlfq + ptable.proc[i].niceness, i);
//			    if(first5) print_mlfq("new runnable");
		    }
		    // update cur is_runnable
		    is_runnable[i] = 1;
	    }
	    // not runnable
	    else {
		    is_runnable[i] = 0;
	    }
    }
    
    // choose a next p from 4 queues
    for(int i = 0; i < 4; i++){
	    if (! empty(mlfq + i)) {
		    runnable_exists = 1;
		    runnable_idx = front(mlfq + i);
		    break;
	    }
    }
    
    
    // if no runnable process, release lock and continue loop
    if (! runnable_exists) {
	    release(&ptable.lock);
	    continue;
    }
    
    // run it
  //  cprintf("switch start\n");
    p = ptable.proc + runnable_idx;
   // cprintf("p is runnable? %d\n", p->state == RUNNABLE);
   
    //deque from queue
    deque(mlfq + p->niceness);
    enque(mlfq + p->niceness, runnable_idx);
   
    switch_to(p);
    //cprintf("switch returned\n");
    
//    if(first5) cprintf("sleeping: %d\n", p->state==SLEEPING);
//    if(first5) print_mlfq("after deque");
    
    if (p->state == ZOMBIE || p->state == SLEEPING) {
		// does not enque
		rear_deque(mlfq + p->niceness);
		is_runnable[runnable_idx] = 0;
//    if(first5) print_mlfq("zombie or sleeping");
    }
//    else if (cur_niceness != p->niceness) {
//	    // setnice() called
//	    rear_deque(mlfq + cur_niceness);
//	    enque(mlfq + p->niceness, runnable_idx);
////	    cprintf("not yet implemented: setnice\n");
//    }
    else if (timeslices[p->niceness] <= p->timeslice) {
	    // time's up!
	    // enque to next queue(if any)
	    // change niceness accordingly
	    if (p->niceness == 3) {
//		    enque(mlfq + 3, runnable_idx);
	    }
	    else {
		    rear_deque(mlfq + p->niceness);
		    p->niceness++;
		    enque(mlfq + p->niceness, runnable_idx);
	    }
	    
//    print_mlfq("time's up");
    }
    else {
	    // yielded
	    // add to rear of cur queue
	    // does NOT change niceness
//	    enque(mlfq + p->niceness, runnable_idx);
	    
//    print_mlfq("yielded");
    }
	p->timeslice = 0;
#else
    
for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
		continue;
		
	switch_to(p);
}
#endif    
    release(&ptable.lock);
  }
}

void switch_to(struct proc* p)
{
	// h to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
	
	proc = p;	// pointed by gs:4 (from proc.h)
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING) {

	
    panic("sched running");
  }
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

// PA#1
void ps(int pid, struct ps_info *info_ptr)
{
	static char* state_code2str[] = {"UNUSED", "EMBRYO", "SLEEPING", "RUNNABLE", "RUNNING", "ZOMBIE"};
	//proc->pid, proc->niceness, state_code2str[proc->state], proc->name);
	struct proc *p;
	
  acquire(&ptable.lock);
  
  for(p = ptable.proc, info_ptr->arr_len = 0; p < &ptable.proc[NPROC]; p++){
	  if(pid == 0 || p->pid == pid){
	    if (p->state == UNUSED) {
		    continue;
	    }
	    strncpy((info_ptr->arr[info_ptr->arr_len].name), p->name, 16);
	    info_ptr->arr[info_ptr->arr_len].niceness = p->niceness;
	    info_ptr->arr[info_ptr->arr_len].pid = p->pid;
	    strncpy((info_ptr->arr[info_ptr->arr_len].state), state_code2str[p->state], 10);
	    (info_ptr->arr_len)++;
    }
  }
  release(&ptable.lock);
}

int getnice(int pid)
{
	struct proc *p;
	
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
	  if (p->state == UNUSED) {
		    continue;
	    }
    if(p->pid == pid){
	    release(&ptable.lock);
	    return p->niceness;
    }
  }
  release(&ptable.lock);
  return -1;
}

int setnice(int pid, int value)
{
	if (! (0 <= value && value <= 3))
	{
		return -1;
	}
	
	struct proc *p;
	
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
	  if (p->state == UNUSED) {
		    continue;
	    }
    if(p->pid == pid){
	    deque_by_idx(mlfq + p->niceness, p - ptable.proc);
	    enque(mlfq + value, p - ptable.proc);
	    p->niceness = value;
	    proc->state = RUNNABLE;
	    sched();
	    release(&ptable.lock);
	    return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// PA #2
int getpinfo(struct pstat* ptr)
{
	if (ptr == 0)
	{
		return -1;
	}
	
	acquire(&ptable.lock);
  
	for(int i = 0; i < NPROC; i++)
	{
		ptr->inuse[i] = ptable.proc[i].state != UNUSED;
		ptr->nice[i] = ptable.proc[i].niceness;
		ptr->pid[i] = ptable.proc[i].pid;
		ptr->ticks[i] = ptable.proc[i].ticks;
	}
	release(&ptable.lock);
  
	return 0;
}