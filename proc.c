#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

//changes by kartik
/*struct qu{
  int strt;
  int end;
  struct proc **arrq;
};
struct qu allq[5];*/
void rrsched(struct cpu *c) __attribute__((noreturn));
void fcfssched(struct cpu *c) __attribute__((noreturn));
void pbsched(struct cpu *c) __attribute__((noreturn));
void mlfqsched(struct cpu *c) __attribute__((noreturn));
//changed ended

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  /*//chnges by kartik
  #ifndef QINIT
  #define QINIT
  for(int i=0;i<5;i++)
  {
    allq[i].strt=0;
    allq[i].end=0;
    allq[i].arrq=(struct proc **)malloc(10000*sizeof(struct proc *));
    for(int j=0;j<10000;j++)
    {
      allq[i].arrq[j]=(struct proc *)malloc(sizeof(struct proc));
      allq[i].arrq[j]=0;
    }
  }
  #endif
  //chnges done*/
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
  //changes by kartik
  p->rtime = 0;
  p->wtime=0;
  p->ctime = ticks;
  p->nrun = 0;
  p->priority = 60;
  p->wfrmlstrn=0;
  p->tick[0]=0;
  p->tick[1]=0;
  p->tick[2]=0;
  p->tick[3]=0;
  p->tick[4]=0;
  p->queue=0;
  p->etq=ticks;
  p->ticksinq=0;
  //done chnges
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
  /*//chnges by kartik
  allq[0].arrq[allq[0].end]=p;
  allq[0].end++;
  //chnges done*/
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
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
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
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  //changes by kartik
  curproc->etime = ticks;
  //changes done
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
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
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

        //changes by kartik
        p->rtime=0;
        p->ctime=0;
        p->etime=0;
        p->wtime=0;
        p->nrun=0;
        p->priority=60;
        p->wfrmlstrn=0;
        p->tick[0]=0;
        p->tick[1]=0;
        p->tick[2]=0;
        p->tick[3]=0;
        p->tick[4]=0;
        p->queue=0;
        p->ticksinq=0;
        p->etq=0;
        //done changes

        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//changes by kartik, implementing waitx,set_priority,pscall
int
waitx(int *wtime,int *rtime)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
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

        //changes by kartik
        *wtime = p->wtime;
        *rtime = p->rtime;
        p->rtime=0;
        p->ctime=0;
        p->etime=0;
        p->wtime=0;
        p->nrun=0;
        p->priority=60;
        p->wfrmlstrn=0;
        p->tick[0]=0;
        p->tick[1]=0;
        p->tick[2]=0;
        p->tick[3]=0;
        p->tick[4]=0;
        p->queue=0;
        p->ticksinq=0;
        p->etq=0;
        //done changes
        
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

int
set_priority(int new_priority,int pid){
  struct proc *p;
  int opr=101;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid)
    {
      opr=p->priority;
      p->priority=new_priority;
      break;
    }
  }
  release(&ptable.lock);
  if(opr==101)
  return -1;
  return opr;
}

void
pscall(void){
  struct proc *p;
  int piro,q,q0,q1,q2,q3,q4;
  cprintf("PID\tPriority\tState\tr_time\tw_time\tn_run\tcur_q\tq0\tq1\tq2\tq3\tq4\n");
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid!=0)
    {
      #ifdef PBS
      piro=p->priority;
      #else
      piro=-1;
      #endif
      #ifdef MLFQ
      q=p->queue;
      q0=p->tick[0];
      q1=p->tick[1];
      q2=p->tick[2];
      q3=p->tick[3];
      q4=p->tick[4];
      #else
      q=-1;
      q0=-1;
      q1=-1;
      q2=-1;
      q3=-1;
      q4=-1;
      #endif
      if(p->state==UNUSED)
        cprintf("%d\t%d\tunused  \t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",p->pid,piro,p->rtime,p->wfrmlstrn,p->nrun,q,q0,q1,q2,q3,q4);
      else if(p->state==EMBRYO)
        cprintf("%d\t%d\tembryo  \t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",p->pid,piro,p->rtime,p->wfrmlstrn,p->nrun,q,q0,q1,q2,q3,q4);
      else if(p->state==SLEEPING)
        cprintf("%d\t%d\tsleeping\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",p->pid,piro,p->rtime,p->wfrmlstrn,p->nrun,q,q0,q1,q2,q3,q4);
      else if(p->state==RUNNABLE)
        cprintf("%d\t%d\trunnable\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",p->pid,piro,p->rtime,p->wfrmlstrn,p->nrun,q,q0,q1,q2,q3,q4);
      else if(p->state==RUNNING)
        cprintf("%d\t%d\trunning \t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",p->pid,piro,p->rtime,p->wfrmlstrn,p->nrun,q,q0,q1,q2,q3,q4);
      else if(p->state==ZOMBIE)
        cprintf("%d\t%d\tzombie  \t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",p->pid,piro,p->rtime,p->wfrmlstrn,p->nrun,q,q0,q1,q2,q3,q4);         
    }
  }
  release(&ptable.lock);
}
//changes done



//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  //struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  //changes by kartik
  //if RR scheduler then call rrsched
  //if FCFS scheduler then call fcfssched
  //if PB scheduler then call pbsched
  //if MLFQ scheduler then call mlfqsched
  #ifdef RR
  rrsched(c);
  #endif
  #ifdef FCFS
  fcfssched(c);
  #endif
  #ifdef PBS
  pbsched(c);
  #endif
  #ifdef MLFQ
  mlfqsched(c);
  #endif
  /*for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }*/
}

void
rrsched(struct cpu *c){
  struct proc *p;
  for(;;){
    // Enable interrupts on this processor.
    sti();
    
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      p->nrun++;
      p->wfrmlstrn=0;

      swtch(&(c->scheduler), p->context);
      switchkvm();
  
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}
void
fcfssched(struct cpu *c){
  struct proc *p;
  struct proc *sp;
  long long int selpro = -1;
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    selpro = -1;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
      if(selpro==-1 || (selpro!=-1 && selpro > p->ctime)){
        selpro = p->ctime;
        sp=p;  
      }  
    }
    if(selpro!=-1)
    {
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      while(sp->state!=SLEEPING && sp->state!=ZOMBIE){
      c->proc = sp;
      switchuvm(sp);
      sp->state = RUNNING;
      p->nrun++;
      sp->wfrmlstrn=0;

      swtch(&(c->scheduler), sp->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      }
    }
    release(&ptable.lock);   
  }
}
void
pbsched(struct cpu* c){
  struct proc *p;
  struct proc *sp;
  int priori = 101;
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    priori = 101;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
      if(p->priority < priori)
      {
        sp=p;
        priori=p->priority;
      }
    }
    if(priori!=101)
    {
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = sp;
      switchuvm(sp);
      sp->state = RUNNING;
      sp->nrun++;
      sp->wfrmlstrn=0;
      
      swtch(&(c->scheduler), sp->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
  }  
}

void
mlfqsched(struct cpu* c){
  struct proc *p;
  struct proc *sp;
  int powt[]={1,2,4,8,16,32};
  int qfnd=5;
  int tmeq=-1; 
  for(;;){
    // Enable interrupts on this processor.
    sti();
    qfnd=5;
    tmeq=-1;
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      //p->wfrmlstrn=ticks-(p->etq)-(p->ticksinq);
      if(p->wfrmlstrn>=(powt[(p->queue)]*10) && p->queue!=0)
      {
        p->wfrmlstrn=0;
        //p->tick[p->queue]=p->tick[p->queue]+p->ticksinq;
        p->queue--;
        p->ticksinq=0;
        p->etq=ticks;
      }
      if(p->queue<4)
      {
        if(p->ticksinq>=powt[p->queue])
        {
          p->wfrmlstrn=0;
          //p->tick[p->queue]=p->tick[p->queue]+p->ticksinq;
          (p->queue)++;
          p->ticksinq=0;
          p->etq=ticks;
        }
      }
      if(p->state != RUNNABLE)
        continue;
      if(p->queue<qfnd)
      {
        qfnd=p->queue;
        tmeq=p->etq;
        sp=p;
      }
      else if(p->queue==qfnd)
      {
        if(p->etq<tmeq)
        {
          tmeq=p->etq;
          sp=p;
        }
      }
    }
    if(qfnd!=5)
    {
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      
      //implementing RR for queue 4
      if(qfnd==4)
      {
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
          if(p->state==RUNNABLE && p->queue==4)
          {
            sp=p;
            break;
          }
        }
      }
      c->proc = sp;
      switchuvm(sp);
      sp->state = RUNNING;
      (sp->nrun)++;
      sp->wfrmlstrn=0;
      //sp->etq=ticks;

      swtch(&(c->scheduler), sp->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0; 
    }
    release(&ptable.lock);
  }
}
//changes done

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
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
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
  struct proc *p = myproc();
  
  if(p == 0)
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
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

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
    {
      p->state = RUNNABLE;
      //changes by kartik 
      p->etq=ticks;
      //changes done
    }
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

//changes by kartik
void
uprtimewtime(void){
  struct proc *p;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->state==RUNNING)
    {
      (p->rtime)++;
      (p->ticksinq)++;
      (p->tick[p->queue])++;
    }
    else if(p->state==EMBRYO || p->state==UNUSED || p->state==RUNNABLE)
    {
      (p->wtime)++;
      (p->wfrmlstrn)++;
    }
  }
  release(&ptable.lock);
}