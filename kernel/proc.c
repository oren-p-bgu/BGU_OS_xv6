#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define ENDLINK -1
#define OUT_OF_LIST_LINK -2 //A proc that is not in a list shall have ni = -2
#define ASSIGNMENT4 1
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
//static int alt = 0;
extern uint64 cas(volatile void *addr, int expected, int newval);   // CAS Task 1

//MIKE: nice trick to get the index :)
int
indxOfProc(struct proc *p){
  return p - proc;
}
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

int removeSearching(struct proc *ps,struct proc *p){
  struct proc *pred,*curr;
  int indx = indxOfProc(p);
  int currindx;
  int found = 0;
  pred = ps;
  acquire(&pred->lock);
  if(pred->ni == ENDLINK){
    release(&pred->lock);
    return found;
  }
  curr = &proc[pred->ni];
  acquire(&curr->lock);
  for(;;){
    currindx = indxOfProc(curr);
    if(currindx == indx){
      pred->ni = curr->ni;
      found = 1;
      curr->ni = OUT_OF_LIST_LINK;
      break;
    }
    if(curr->ni == ENDLINK){
      break;
    }
    release(&pred->lock);
    pred = curr;
    curr = &proc[curr->ni];
    acquire(&curr->lock);
  }
  release(&curr->lock);
  release(&pred->lock);
  return found;
}
//MIKE : 
//This function get the first non dummy proc and returns it locked!.
/*
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
}*/




//p->lock must already be held!
//It wont be released by this func
void
pushProcAtStart(struct proc *ps, struct proc *p){
  if(p->ni != OUT_OF_LIST_LINK){
    panic("start: pushing an already pushed item to list!");
  }
  acquire(&ps->lock);
  p->ni = ps->ni;
  ps->ni = indxOfProc(p);
  release(&ps->lock);
}

//p->lock must already be held!
//It wont be released by this func
void
pushProcAtEnd(struct proc *ps, struct proc *p){
  static char *states[] = {
  [UNUSED]    "unused",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *last;
  if(p->ni != OUT_OF_LIST_LINK){
    printf("%s",states[p->state]);
    printf("to %s",states[ps->state]);
    printf("%d",p->ni);

    printf("%s",states[proc[p->ni].state]);
    panic("end: pushing an already pushed item to list!");
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
/*
//p->lock must already be held!
//It wont be released by this func
//This function find the proc in the list and eliminates it, and fixes the list back 
void
removeProcFromList(struct proc *ps, struct proc *p){
  //first, lock its next inline. then find its pred and lock it too!
  struct proc *og = ps;
  struct proc *last;
  int currentIndexOfProc = indxOfProc(p);
  acquire(&ps->lock);

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
  p->ni = OUT_OF_LIST_LINK;//We just removed it, so we notify it about its removal 
 
}*/
/*
void
removeProcFromList_safe(struct proc *ps, struct proc *p){
  //first, lock its next inline. then find its pred and lock it too!
  struct proc *last;
  struct proc *next = proc;
  int currentIndexOfProc = indxOfProc(p);
  acquire(&ps->lock);

  for(;;){
    if(ps->ni == ENDLINK){
      release(&ps->lock);
      return;//No match found
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
 
}*/

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
  struct cpu  *c;
  int i = 0;
  int cpu_indx = 0;
  char* dummystring = "runnable_dummyx";//do not touch this handsome string
  int nextProc = 0;//uses for the proc[] init UNUSED.

  //Added them dummies
  //first init the locks
  //each CPU runnable list is here :
  for(p = ps_runnable; p < &ps_runnable[NCPU]; p++,i++) {
    dummystring[14] = '0' + (cpu_indx++);//this is only for formatting the number
    initlock(&p->lock, dummystring);
    p->ni = ENDLINK;
    p->state = RUNNABLE;
    c = &cpus[i];
    if(cas(&c->counteryay, 0, 1)){
      panic("cas failed at proc init!!!");
    }
    c->counteryay = 5;
  }
  c = &cpus[0];
  int neee = 10;
  //int old = 10;
  do{
    neee -=1;
  }while(cas(&c->counteryay, neee,neee+8));
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
  ps_sleeping.state = SLEEPING;

  ps_zombie.ni = ENDLINK;
  ps_zombie.state = UNUSED;
  ps_unused.ni = nextProc++;//special treatment
  ps_unused.state = UNUSED;

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

  acquire(&ps_unused.lock);
  if(ps_unused.ni == ENDLINK){
    release(&ps_unused.lock);
    //debug:
    return 0;
  }
  p = &proc[ps_unused.ni];
  acquire(&p->lock);
  //DEBUG
  //printf("%d",chosen->pid);
  ps_unused.ni = p->ni;
  p->ni = OUT_OF_LIST_LINK;
  release(&ps_unused.lock);
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
  p->lastCpuRan = getCPUid_MIKE();

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

int remove_bypred(struct proc *ps,struct proc *p){
  struct proc *pred,*curr;
  int found = 0;
  int indx = indxOfProc(p);
  pred = ps;
  acquire(&pred->lock);
  if(pred->ni == ENDLINK){
    release(&pred->lock);
    return found;
  }
  if(pred->ni == indx){
    pred->ni = p->ni;
    found = 1;
    release(&pred->lock);
    return found;
  }
  curr = &proc[pred->ni];
  acquire(&curr->lock);
  for(;;){
    if(curr->ni == indx){
      curr->ni = p->ni;
      found = 1;
      break;
    }
    if(curr->ni == ENDLINK){
      break;
    }
    release(&pred->lock);
    pred = curr;
    curr = &proc[curr->ni];
    acquire(&curr->lock);
  }
  release(&curr->lock);
  release(&pred->lock);
  return found;
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
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;

  /*if(p->state == UNUSED){
    //make sure p is there else scream with agony
  }else{*/
    //MIKE: Cool idea! lets get this proc into the list!
    //TODO : implement inserting a proc into end of list (here its unused)
    //pushsProcAtEnd(&ps_unused,)

    if(remove_bypred(&ps_zombie,p)){
      p->ni = OUT_OF_LIST_LINK;
      //printf("\n\n\n+\n\n\n");
    }else if(remove_bypred(&ps_sleeping,p)){
      p->ni = OUT_OF_LIST_LINK;
      //printf("\n\n\n+\n\n\n");
    }else if(remove_bypred(&ps_unused,p)){
      p->ni = OUT_OF_LIST_LINK;
      //printf("\n\n\n+\n\n\n");
    }else{
      //printf("free:%s",states[p->state]);
      
    }
    p->pid = 0;
    p->lastCpuRan = 0;
    p->state = UNUSED;
    pushProcAtEnd(&ps_unused,p);
  //}
 
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
  uint64 numOfProc;
  int lastCpuRan;
  struct proc *p;

  p = allocproc();
  initproc = p;
  lastCpuRan = p->lastCpuRan;
  
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
  pushProcAtEnd(&ps_runnable[lastCpuRan],p);
  do{
      numOfProc = cpus[lastCpuRan].counteryay;
  } while(cas(&cpus[lastCpuRan].counteryay, numOfProc, numOfProc+1));
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
static int lastcpuuu = 0;
int 
getLowCpu(){
  uint64 val = -1;
  uint64 currentval = 0;
  int indx = 0;
  struct cpu *c;
  int i;

  for(i = 1; i < NCPU; i++){
    c = &cpus[NCPU];
    do{
      currentval = c->counteryay;
    }while(cas(&c->counteryay,currentval,currentval));
    if(currentval < val){
      indx = i;
      val = currentval;
    }
  }
  if(lastcpuuu != indx){
    //printf("<---%d--->",indx);
  }
  lastcpuuu = indx;
  return indx;
}
void 
printCpuVals(){
  struct cpu *c;
  int i;
  for(i = 0; i < NCPU; i++){
    c = &cpus[NCPU];
    printf("%d %d\n",i,c->counteryay);
  }
  printf("lastcpu : %d\n",lastcpuuu);
}
// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  uint64 numOfProc = 0;
  int lastCpuRan;
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
  #if ASSIGNMENT4 == 1
      lastCpuRan = getLowCpu();
      p->lastCpuRan = lastCpuRan;
  #else
  lastCpuRan = p->lastCpuRan;
  #endif
  pushProcAtStart(&ps_runnable[lastCpuRan],np);
  while(cas(&cpus[lastCpuRan].counteryay, numOfProc, numOfProc+1)){
      numOfProc ++;
  } 
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
  pushProcAtStart(&ps_zombie,p);
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
          if(remove_bypred(&ps_zombie,np))
          {
            np->ni = OUT_OF_LIST_LINK;
          }else{
            panic("A zombie ran away!");
          }
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
  struct proc *tosteal;
  int stolenindx = -1;
  int i;
  struct cpu *c = mycpu();
  int currentcpu = getCPUid_MIKE();
  struct proc *myps = &ps_runnable[currentcpu];
  //int numOfProc;
  //int cpu_id = 0;//FIX HERE
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();
    currentcpu = getCPUid_MIKE();
    myps = &ps_runnable[currentcpu];
    //check if list is empty
    //else: pick a process and lock it and fix the list and unlock the list
    //run the process 
    acquire(&myps->lock);
    if(myps->ni <= ENDLINK){//No proccesses to run for me!
      release(&myps->lock);
      //steal 
      for(i = 0,tosteal = ps_runnable;tosteal<&ps_runnable[NCPU];i++,tosteal++){
        if(i != currentcpu){
            acquire(&tosteal->lock);
            do{
              stolenindx = tosteal->ni;
              if(stolenindx == -1){
                break;//no steal!
              }
              p = &proc[stolenindx];
              acquire(&p->lock);   
            }while(cas(&tosteal->ni,stolenindx,p->ni));
            release(&tosteal->lock);
            if(stolenindx != -1){
              //printf("STEAL %d %d\n",i, currentcpu);   
              p->lastCpuRan = currentcpu;     
              break;
            }
        }
      }
      if(stolenindx == -1){
        continue;
      }
    }else{
      acquire(&(p = &proc[myps->ni])->lock);
      myps->ni = p->ni;
      release(&myps->lock);
    }
    p->ni = OUT_OF_LIST_LINK;
    // Switch to chosen process.  It is the process's job
    // to release its lock and then reacquire it
    // before jumping back to us.
    p->state = RUNNING;
    /*do{
      numOfProc = cpus[currentcpu].counteryay;
    } while(cas(&(cpus[currentcpu].counteryay), numOfProc, numOfProc-1));*/
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
  {
    panic("sched locks");
  }
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
  int lastCpuRan;
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  lastCpuRan = p->lastCpuRan;
  pushProcAtEnd(&ps_runnable[lastCpuRan],p);//TODO: to runnable
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

  acquire(&ps_sleeping.lock);
  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;
  //PushProcAtStart:
  if(p->ni != OUT_OF_LIST_LINK){
    panic("sleep: pushing an already pushed item to list!");
  }
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
  uint64 numOfProc = 0,lastCpuRan;
  struct proc *helper_cpuready;
  struct proc *pred,*curr;
  pred = &ps_sleeping;
  acquire(&pred->lock);
  if(pred->ni == ENDLINK){
    release(&pred->lock);
    return;
  }
  curr = &proc[pred->ni];
  acquire(&curr->lock);
  for(;;){
    if(curr->state != SLEEPING){
        panic("Awake proc in sleeping pile!");
    }
    if(curr->chan == chan){
      pred->ni = curr->ni;
      #if ASSIGNMENT4 == 1
      lastCpuRan = getLowCpu();
      curr->lastCpuRan = lastCpuRan;
      #else
      lastCpuRan = curr->lastCpuRan;
      #endif
      acquire(&(helper_cpuready = &ps_runnable[lastCpuRan])->lock);//very funny line, i like it!
      curr->ni = helper_cpuready->ni;
      helper_cpuready->ni = indxOfProc(curr);
      curr->state = RUNNABLE;
      release(&curr->lock);
      release(&helper_cpuready->lock);
      while(cas(&cpus[lastCpuRan].counteryay, numOfProc, numOfProc+1)){
          numOfProc ++;
      } 
      if(pred->ni == ENDLINK){//finished for today
        release(&pred->lock);
        return;
      }
      curr = &proc[pred->ni];
      acquire(&curr->lock);
    }else{
      if(curr->ni == ENDLINK){
        break;
      }
      release(&pred->lock);
      pred = curr;
      curr = &proc[curr->ni];
      acquire(&curr->lock);
    }
  }
  release(&curr->lock);
  release(&pred->lock);

  /*BAD CODE AHEAD: last = &ps_sleeping;
  p = &ps_sleeping;
  for(;;){
    if(p->ni == ENDLINK){
      release(&ps_sleeping.lock);
      return;
    }
    aqcuirep = &proc[p->ni];
    if(p->state != SLEEPING){
      panic("Awake proc in sleeping pile!");
    }
    if(p->chan == chan){
      //pop the proc from the list:
      last->ni = p->ni;
      //push the proc into ready list
      
      p = last;//fix the loop :)
    }
    last = p;
  }
  release(&ps_sleeping.lock);*/
  /*struct proc *p;
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
    acquire(&(p = &proc[itr->ni])->lock);
    procindx = itr->ni;
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
  }*/
  //OLD :
  /*for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        removeProcFromList(&ps_sleeping, p);
        p->state = RUNNABLE;
        pushPdrocAtEnd(&ps_runnable[p->lastCpuRan - 1],p);//FIX PLS
      }
      release(&p->lock);
    }
  }*/
}
//returns the proc locked
struct proc *removeSearching_bypid(struct proc *ps,int pid){
  struct proc *pred,*curr;
  pred = ps;
  acquire(&pred->lock);
  if(pred->ni == ENDLINK){
    release(&pred->lock);
    return 0;
  }
  curr = &proc[pred->ni];
  acquire(&curr->lock);
  for(;;){
    if(curr->pid == pid){
      pred->ni = curr->ni;
      release(&pred->lock);
      curr->ni = OUT_OF_LIST_LINK;
      return curr;
    }
    if(curr->ni == ENDLINK){
      break;
    }
    release(&pred->lock);
    pred = curr;
    curr = &proc[curr->ni];
    acquire(&curr->lock);
  }
  release(&curr->lock);
  release(&pred->lock);
  return 0;
}
//returns the proc locked
struct proc *search_bypid(struct proc *ps,int pid){
  struct proc *pred,*curr;
  pred = ps;
  acquire(&pred->lock);
  if(pred->ni == ENDLINK){
    release(&pred->lock);
    return 0;
  }
  curr = &proc[pred->ni];
  acquire(&curr->lock);
  for(;;){
    if(curr->pid == pid){
      release(&pred->lock);
      return curr;
    }
    if(curr->ni == ENDLINK){
      break;
    }
    release(&pred->lock);
    pred = curr;
    curr = &proc[curr->ni];
    acquire(&curr->lock);
  }
  release(&curr->lock);
  release(&pred->lock);
  return 0;
}
// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  int numOfProc,lastCpuRan;
  struct proc *runnablelist;
  struct proc *cpuhelper;
  struct proc *p;
  if((p = removeSearching_bypid(&ps_sleeping,pid)) != 0){
    p->killed = 1;
    p->state = RUNNABLE;
    lastCpuRan = p->lastCpuRan;
    cpuhelper = &ps_runnable[lastCpuRan];
    acquire(&cpuhelper->lock);
    p->ni = cpuhelper->ni;
    cpuhelper->ni = indxOfProc(p);
    release(&p->lock);
    release(&cpuhelper->lock);
    do{
        numOfProc = cpus[lastCpuRan].counteryay;
    } while(cas(&cpus[lastCpuRan].counteryay, numOfProc, numOfProc+1));
    return 0;
  }
  else{
    for(runnablelist = ps_runnable; runnablelist < &ps_runnable[NCPU]; runnablelist++){
      if((p = search_bypid(runnablelist,pid)) != 0){
        p->killed = 1;
        release(&p->lock);
        break;
      }
    }
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

  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s cpu %d", p->pid, state, p->name, p->lastCpuRan);
    printf("\n");
  }
  printf("cpu vals:\n");
  printCpuVals();
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
  p->lastCpuRan = tocpu;
  release(&p->lock);
  yield();
  newcpu = getCPUid_MIKE();
  return newcpu == tocpu ? newcpu : -1;
}
int
get_cpu(void)
{
  printf("from proc : actual %d\nfrom proc: according to proc %d\n",getCPUid_MIKE(),myproc()->lastCpuRan);
  return myproc()->lastCpuRan;
}
int cpu_process_count(int cpu_num){
  if(cpu_num >= NCPU){
    printf("cpu_process_count: Out of bounds");
    return -1;
  }
  return cpus[cpu_num].counteryay;
}