#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define ENDLINK -1
#define OUT_OF_LIST_LINK -2 //A proc that is not in a list shall have ni = -2

struct cpu cpus[NCPU];

struct proc proc[NPROC];
//Explanation: Each proc here is a dummy, indicated by cpu = -1. 
//IDEA #2: if you want to edit a list, you first lock the dummy.

//Each cpu has its own runnable table.
static struct proc ps_runnable[NCPU];

static struct proc ps_sleeping;
static struct proc ps_zombie;
static struct proc ps_unused;

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern uint64 cas(volatile void *addr, int expected, int newval);   // CAS Task 1

//DOC: this function returns the specific ps size 
int 
getPSsize(struct proc *ps){
  int size = 0;
  struct proc *first = ps;
  acquire(&first->lock);
  for(;;){
    if(ps->ni == ENDLINK){
      release(&first->lock);
      return size;
    }
    size++;
    ps = &proc[ps->ni];
  }
}

//MIKE : 
//This function get the first non dummy proc and returns it locked!.

struct proc*
popFirstProc(struct proc *ps){
  struct proc *chosen;
  acquire(&ps->lock);
  if(ps->ni == ENDLINK){
    printf("No available proccess in list.");
    return 0;
  }
  chosen = &proc[ps->ni];
  acquire(&chosen->lock);
  //DEBUG
  //printf("%d",chosen->pid);
  ps->ni = chosen->ni;
  chosen->ni = OUT_OF_LIST_LINK;
  release(&ps->lock);
  return chosen;
}


//MIKE: nice trick to get the index :)
int
indxOfProc(struct proc *p){
  return p - proc;
}

//p->lock must already be held!
//It wont be released by this func
void
pushProcAtEnd(struct proc *ps, struct proc *p){
  struct proc *last;
  if(p->ni != OUT_OF_LIST_LINK){
    panic("pushing an already pushed item to list!");
  }
  acquire(&ps->lock);
  for(;;){
    if(ps->ni == ENDLINK){// Found the last link, lets add the p here!
      ps->ni = indxOfProc(p);//TODO : replace this function with the acutal phrase
      p->ni = ENDLINK;
      release(&ps->lock);
      return;
    }else{
      last = ps;
      ps = &proc[ps->ni];
      //First lock the next one, then release the last.
      acquire(&ps->lock);
      release(&last->lock);
    }
  }
}

//p->lock must already be held!
//It wont be released by this func
//This function find the proc in the list and eliminates it, and fixes the list back 
void
removeProcFromList(struct proc *ps, struct proc *p){
  //first, lock its next inline. then find its pred and lock it too!
  struct proc *og = ps;
  struct proc *last;
  struct proc *next = proc;
  int currentIndexOfProc = indxOfProc(p);
  acquire(&ps->lock);

  if(p->ni != ENDLINK){
    next = &proc[p->ni];
    acquire(&next->lock);
  }

  for(;;){
    if(ps->ni == ENDLINK){
      //DEBUG
      if(og == &ps_sleeping){
        printf("sleepingbug");
      }
      if(og == &ps_unused){
        printf("unusedbug");
      }
      if(og == &ps_zombie){
        printf("zombiebug");
      }
      printf(" %d ",indxOfProc(p));
      panic("Could not find proc in list removeproc");
    }
    if(ps->ni == currentIndexOfProc){
      ps->ni = p->ni;//last will point to my next.
      release(&ps->lock);
      break;
    }
    last = ps;
    ps = &proc[ps->ni];
    //First lock the next one, then release the last.
    acquire(&ps->lock);
    release(&last->lock);
  }
  if(p->ni != ENDLINK){
    release(&next->lock);
  }
  p->ni = OUT_OF_LIST_LINK;//We just removed it, so we notify it about its removal 
 
}
extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl) {
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}
//TODO: each time a proc changes its state, I need to reconsider 
//using the list system
//MIKE : I guess we should add changes here if we want to init the proc

// initialize the proc table at boot time.
void
procinit(void)
{
  struct proc *p;
  int cpu_indx = 0;
  char* dummystring = "runnable_dummyx";//do not touch this handsome string
  int nextProc = 0;//uses for the proc[] init UNUSED.

  //Added them dummies
  //first init the locks
  //each CPU runnable list is here :
  for(p = ps_runnable; p < &ps_runnable[NCPU]; p++) {
    dummystring[14] = '0' + (cpu_indx++);//this is only for formatting the number
    initlock(&p->lock, dummystring);
    p->ni = ENDLINK;
  }
  initlock(&ps_sleeping.lock, "sleeping_dummy");
  initlock(&ps_zombie.lock, "zombie_dummy");
  initlock(&ps_unused.lock, "unused_dummy");
  /*//Update the dummy (only debugging reasons) 
  ps_runnable->cpuToRunOn = -2;
  ps_sleeping->cpuToRunOn = -2;
  ps_zombie->cpuToRunOn = -2;
  ps_unused->cpuToRunOn = -2;*/

  //Init the lists.
  ps_sleeping.ni = ENDLINK;
  ps_zombie.ni = ENDLINK;
  ps_unused.ni = nextProc++;//special treatment

  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->kstack = KSTACK((int) (p - proc));
      p->ni = nextProc++;
  }
  //fix last proc in the list: 
  p = &proc[NPROC-1];
  p->ni = ENDLINK;
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}


//MIKE: this is safer (using push thingies)
int
getCPUid_MIKE()
{
  push_off();
  int id = r_tp();
  pop_off();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void) {
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void) {
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid() {
  int pid;
  
  // acquire(&pid_lock);
  // pid = nextpid;
  // nextpid = nextpid + 1;
  // release(&pid_lock);

  // CAS Task 4
  do{
      pid = nextpid;
  } while(cas(&nextpid, pid, pid+1));

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  p = popFirstProc(&ps_unused);
  if(p == 0){//No proc found
    return 0;
  }
  goto found;
  /*for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;*/

found:
  p->pid = allocpid();
  p->state = USED;
  p->lastCpuRan = getCPUid_MIKE() + 1;

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
  //MIKE: Cool idea! lets get this proc into the list!
  //TODO : implement inserting a proc into end of list (here its unused)
  //pushProcAtEnd(&ps_unused,)
  p->lastCpuRan = 0;
  pushProcAtEnd(&ps_unused,p);
}

// Create a user page table for a given process,
// with no user memory, but with trampoline pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe just below TRAMPOLINE, for trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// od -t xC initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy init's instructions
  // and data into it.
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  pushProcAtEnd(&ps_runnable[p->lastCpuRan - 1],p);
  //TODO : here you should assign it to a cpu

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();
  //int p_cpuid = cpuid();//current cpu
  //TODO : CHANGE THIS^ 
  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }
  
  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  pushProcAtEnd(&ps_runnable[p->lastCpuRan - 1],np);
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;
  //MIKE:
  pushProcAtEnd(&ps_zombie,p);

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      if(np->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&np->lock);

        havekids = 1;
        if(np->state == ZOMBIE){
          // Found one.
          pid = np->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                  sizeof(np->xstate)) < 0) {
            release(&np->lock);
            release(&wait_lock);
            return -1;
          }
          removeProcFromList(&ps_zombie,np);
          freeproc(np);
          release(&np->lock);
          release(&wait_lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || p->killed){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  int currentcpu = getCPUid_MIKE();
  struct proc *myps = &ps_runnable[currentcpu];
  //int cpu_id = 0;//FIX HERE
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();
    
    //check if list is empty
    //else: pick a process and lock it and fix the list and unlock the list
    //run the process 
    acquire(&myps->lock);
    if(myps->ni == ENDLINK){//No proccesses to run for me!
      release(&myps->lock);
      continue;
    }
    p = &proc[myps->ni];
    acquire(&p->lock);
    myps->ni = p->ni;
    p->ni = OUT_OF_LIST_LINK;
    release(&myps->lock);
    // Switch to chosen process.  It is the process's job
    // to release its lock and then reacquire it
    // before jumping back to us.
    p->state = RUNNING;
    c->proc = p;
    swtch(&c->context, &p->context);

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;
    release(&p->lock);

    /*for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        removeProcFromList(&ps_runnable[0],p);
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      release(&p->lock);
    }*/
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  pushProcAtEnd(&ps_runnable[p->lastCpuRan - 1],p);//TODO: to runnable
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;
  //PushProcAtStart:
  if(p->ni != OUT_OF_LIST_LINK){
    panic("pushing an already pushed item to list!");
  }
  acquire(&ps_sleeping.lock);
  p->ni = ps_sleeping.ni;
  ps_sleeping.ni = indxOfProc(p);
  release(&ps_sleeping.lock);

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;
  //CUSTOM CODE:
  struct proc *itr = &ps_sleeping;
  struct proc *last = itr;//for unlocking the last one after locking the current
  struct proc *cpulist_helper = ps_runnable;//do not worry, this is junk.
  int procindx = -1;
  acquire(&itr->lock);
  if(itr->ni == -1){
    release(&itr->lock);
    return;
  }
  for(;;){
    procindx = itr->ni;
    p = &proc[procindx];
    acquire(&p->lock);
    if(p->state == SLEEPING && p->chan == chan) {
      itr->ni = p->ni;//last will point to my next.
      p->ni = OUT_OF_LIST_LINK;
      p->state = RUNNABLE;
      //Special case of running on a list whilst running on a list!
      //we would put the item in the first place then!
      cpulist_helper = &ps_runnable[p->lastCpuRan - 1];
      //PushProcAtStart:
      if(p->ni != OUT_OF_LIST_LINK){
        panic("pushing an already pushed item to list!");
      }
      acquire(&cpulist_helper->lock);
      p->ni = cpulist_helper->ni;
      cpulist_helper->ni = procindx;
      release(&cpulist_helper->lock);
    }
    release(&p->lock);
    if(itr->ni == -1){
      release(&itr->lock);
      return;
    }
    last = itr;
    itr = &proc[itr->ni];
    //First lock the next one, then release the last.
    acquire(&itr->lock);
    release(&last->lock);
  }
  //OLD :
  /*for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        removeProcFromList(&ps_sleeping, p);
        p->state = RUNNABLE;
        pushProcAtEnd(&ps_runnable[p->lastCpuRan - 1],p);//FIX PLS
      }
      release(&p->lock);
    }
  }*/
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        removeProcFromList(&ps_sleeping, p);
        p->state = RUNNABLE;
        pushProcAtEnd(&ps_runnable[p->lastCpuRan - 1],p);
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}



// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n\n\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s cpu %d", p->pid, state, p->name, p->lastCpuRan-1);
    printf("\n");
  }
  printf("unused.ni = %d\n",ps_unused.ni);//maybe here also print the lists for sanity check
  printf("unused size = %d\n",getPSsize(&ps_unused));//maybe here also print the lists for sanity check
  printf("zombie size = %d\n",getPSsize(&ps_zombie));//maybe here also print the lists for sanity check
  printf("sleep size = %d\n",getPSsize(&ps_sleeping));//maybe here also print the lists for sanity check
  printf("runnable0 size = %d\n",getPSsize(&ps_runnable[0]));//maybe here also print the lists for sanity check
   printf("runnable1 size = %d\n",getPSsize(&ps_runnable[1]));//maybe here also print the lists for sanity check
}
int
set_cpu(int tocpu)
{
  struct proc *p = myproc();
  int newcpu = -1;
  acquire(&p->lock);
  p->lastCpuRan = tocpu+1;//fix the offset (see documentation in proc.h)
  release(&p->lock);
  yield();
  newcpu = getCPUid_MIKE();
  return newcpu == tocpu ? newcpu : -1;
}
int
get_cpu(void)
{
  printf("from proc : actual %d\nfrom proc: according to proc %d\n",getCPUid_MIKE(),myproc()->lastCpuRan-1);
  return myproc()->lastCpuRan-1;
}