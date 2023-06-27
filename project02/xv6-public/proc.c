#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#define true 1
#define false 0
#define NEWEXIT 1
#define EXIT 0

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
int nexttid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

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
  // ^ process의 첫번째 쓰레드 할당하기 전까지는 없는 취급
  p->tid = 0;
  p->stacksize = 0;
  p->isThread = false;
  // p->threadnum = 1;
  p->start_routine = 0;
  p->arg = 0;
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
  if (!holding(&ptable.lock))
    acquire(&ptable.lock);
  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  // thread 여러개 보유하는 경우
  if (curproc->threadnum != 0) {
    struct proc *p;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if (p->pid == curproc->pid)
        p->sz = curproc->sz;
    }
  }

  release(&ptable.lock);
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
  // ^ memory limit, stack size 복사
  np->stacksize = curproc->stacksize;
  np->szlimit = curproc->szlimit;


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

#if NEWEXIT
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


  killthread(curproc);

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->parent == curproc){
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }
  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;

  sched();
  panic("zombie exit");
}
#endif

#if EXIT
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
  sched();
  panic("zombie exit");
}
#endif


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
        p->szlimit = 0;
        p->stacksize = 0;
        p->threadnum = 0;
        p->isThread = false;
        p->start_routine = 0;
        p->arg = 0;
        p->retval = 0;
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
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
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

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
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
  int find = false;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid && p->killed == 0){
      find = true;
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
    }
  }
  if (find) {
    cprintf("[SUCCESS] kill process which pid is %d\n", pid);
    // cprintf("\n");
    release(&ptable.lock);
    return 0;
  }
  cprintf("[ERROR] could not kill the process\n");
  // cprintf("\n");
  release(&ptable.lock);
  return -1;
}

int
killthread(struct proc *curproc)
{
  struct proc *p;
  int pid = curproc->pid;
  int find = false;

  acquire(&ptable.lock);
  if (curproc->isThread)
    curproc->parent = curproc->parent->parent;
  curproc->isThread = false;
  curproc->threadnum = 0;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->pid == pid && p != curproc) {
      find = true;
      kfree(p->kstack);
      p->kstack = 0;

      p->pid = 0;
      p->tid = 0;
      p->parent = 0;
      p->name[0] = 0;
      p->killed = 0;
      p->state = UNUSED;
      p->isThread = false;
      p->threadnum = 0;
    } 
  }

  if (find) {
    release(&ptable.lock);
    return 0;
  }
  release(&ptable.lock);
  return -1;
}


int
killProc(struct proc *parent)
{
  struct proc *p;
  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->parent == parent && p->isThread == true) {
      cprintf("kill proc\n");
      p->killed = 1;
      if (p->state == SLEEPING) 
        p->state = RUNNABLE;
    }
  }
  release(&ptable.lock);
  return 0;
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


// ^ sz: 기존에 할당받은 프로세스의 크기
// ^ szlimit: 최대로 할당받을 수 있는 프로세스의 크기
int
setmemorylimit(int pid, int limit)
{
  struct proc *p;
  int findProc = false;
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->pid == pid && p->isThread == false) {
      findProc = true;
      break;
    }
  }
  // ! pid가 존재하지 않는 경우
  if (findProc == false) {
    cprintf("[ERROR] There is no process which pid is %d\n", pid);
    return -1;
  }

  // * 여기서부터는 process 발견한 상태
  if ((p->sz > limit || limit < 0) && limit != 0) {
    cprintf("[ERROR] Already allocated more than limit\n");
    return -1;
  }

  // * limit == 0인 경우 제한이 없음, 양수인 경우 limit으로 제한을 가짐
  // * 일단 0으로 해놓음
  p->szlimit = limit == 0 ? 0 : limit;

  return 0;
}

void
printProcList()
{
  cprintf("========================print process list========================\n");
  struct proc* p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if (p->state == RUNNING || p->state == RUNNABLE || p->state == SLEEPING) {
      // ^ 이름, pid, 스택용 페이지의 개수, 할당받은 메모리의 크기, 메모리의 최대 제한
      // ! Process가 실행한 Thread의 정보를 고려해서 출력???????????????
      // ^ Thread의 경우 출력하지 않음
      printProc(p);
    }
  }

  cprintf("\n");
}

void
printProc(struct proc* p)
{
  char *state;
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";

  if (p->isThread == false)
    cprintf("pid: %d\tname: %s\tstate: %s\tstacksize: %d\tallocated memory: %d\tmemory limit: %d\tthread: false\n\n", 
                        p->pid, p->name, state, p->stacksize, p->sz, p->szlimit);
  // if (p->isThread)
  //   cprintf("tid: %d\tstate: %s\tstacksize: %d\tallocated memory: %d\tmemory limit: %d\tthread: true\n\n", 
  //                       p->tid, state, p->stacksize, p->sz, p->szlimit);
  // else
  //   cprintf("pid: %d\tstate: %s\tstacksize: %d\tallocated memory: %d\tmemory limit: %d\tthread: false\n\n", 
  //                       p->pid, state, p->stacksize, p->sz, p->szlimit);
}



// fork, exec 함수 참고
int
thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg)
{
  uint sp, ustack[3+MAXARG+1];
  struct proc* np;
  struct proc* curproc = myproc();

  // ^ thread 1개당 myproc()의 stack page 2개씩 쓰기 때문에
  if (curproc->sz + 2*PGSIZE < curproc->szlimit && curproc->szlimit != 0) {
    cprintf("Cannot create more threads\n");
    killProc(curproc);
    return -1;
  }
  
  if ((np = allocproc()) == 0) {
    cprintf("There is no more space\n");
    return -1;
  }


  // sz => 이거 설정을 도대체 어떻게 하냐?
  // 이거 sp를 curproc의 최상단에 위치시키면 되지 않나? => curproc->sz를 PGROUNDUP에서 그 지점부터 페이지 할당
  // 이렇게하면 해당 지점부터 페이지를 쌓아올리니까 allocuvm에서 stacksize가 부족하면 할당이 안되겠지
  // 스택페이지 1개 + 가드페이지 1개 씩 할당


  // acquire(&ptable.lock);
  if (growproc(2*PGSIZE) != 0) {
    return -1;
  }
  acquire(&ptable.lock);
  np->tf->eax = 0;

  for (int i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);
  

  // thread는 부모와 pgdir을 공유
  // state는 runnable로
  // pid는 처리 안하고, tid로 처리 => isThread를 통해서 뭘 쓸지 결정
  //    근데, pthread에서는 pid or tid를 0으로 처리하는 기법을 사용했는데, 이건 아마 exit 쪽이랑 관련있던거 같음
  // trapframe을 부모 프로세스에서 그대로 복사해오는데
  //    eip랑 esp만 별도로 처리해줌


  // curproc->sz = sz;

  np->sz = curproc->sz;
  np->pgdir = curproc->pgdir;

  np->state = RUNNABLE;
  np->pid = curproc->pid;
  np->tid = nexttid++;
  *thread = np->tid;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  np->isThread = true;
  curproc->threadnum++;
  np->threadnum = curproc->threadnum;

  np->start_routine = start_routine;
  np->arg = arg;
  np->retval = 0;



  sp = curproc->sz;
  ustack[0] = 0xffffffff;
  // ptr => 주소를 저장하나?
  ustack[1] = (uint) arg;
  sp -= 8;

  np->tf->eip = (uint)start_routine;
  np->tf->esp = sp;
  if (copyout(np->pgdir, sp, ustack, 8) < 0)
    return -1;

  // 일단 curproc 이름 그대로 가져와
  safestrcpy(np->name, curproc->name, sizeof(curproc->name));
  release(&ptable.lock);

  switchuvm(curproc);
  return 0;
}

// 1. join함수에서 wait으로 기다려줌
int
thread_join(thread_t thread, void **retval)
{
  struct proc *p;
  struct proc *curproc = myproc();
  int havekids;
  acquire(&ptable.lock);
  for (;;) {
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if (p->isThread == false || p->parent != curproc)
        continue;

      havekids = true;
      if (p->state == ZOMBIE && p->tid == thread) {
        kfree(p->kstack);
        p->kstack = 0;
        p->sz = 0;
        p->pid = 0;
        p->tid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        p->isThread = false;
        p->threadnum = 0;
        curproc->threadnum--;
        *retval = p->retval;
        release(&ptable.lock);
        return 0;
      }
    }

    if (!havekids || curproc->killed) {
      release(&ptable.lock);
      return -1;
    }

    sleep(curproc, &ptable.lock);
  }
}

void
thread_exit(void *retval)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if (curproc == initproc)
    panic("init exiting");
  
  // Close all open files
  for (fd = 0; fd < NOFILE; fd++) {
    if (curproc->ofile[fd]) {
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait()
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->parent == curproc) {
      p->parent = initproc;
      if (p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return
  curproc->state = ZOMBIE;
  curproc->retval = retval;

  sched();
  panic("zombie exit");
}


// 현재 스레드에서 exec을 호출,
// 기존 프로세스의 모든 스레드들을 정리
// 다른 스레드들을 종료
void
clearthreads() 
{
  struct proc *curproc = myproc();
  
  // cprintf("curproc->isThread: %d\n", curproc->isThread);
  killthread(curproc);
  // curproc->parent = initproc;
}



