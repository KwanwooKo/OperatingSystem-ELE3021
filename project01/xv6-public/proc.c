#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#define ROUND_ROBIN 0
#define MLFQ 1

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;
struct queue queues[3];         // L0, L1, L2 queue 관리

int nextpid = 1;
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

  // TODO: 내가 추가한 변수 초기화
  p->qlevel = 0;
  p->priority = 3;
  p->timequantum = 4;
  p->qnext = 0;
  p->schedLock = 0;

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
  // TODO: Enqueue 진행
  Enqueue(p, p->qlevel);
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

  // TODO: queue에다가 RUNNABLE한 process 넣기
  Enqueue(np, np->qlevel);
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
#if ROUND_ROBIN
void
scheduler(void)
{
  // 현재 scheduler 내용은 Round Robin 방식으로 진행
  // 이거를 Multilevel Feedback Queue로 변경
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  // scheduler는 끝나지 않음
  for(;;){
    // Enable interrupts on this processor.
    sti();  // set interrupt flag to 1

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE) {
        continue;
      }
      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      // p를 실행시키고 싶으면 user에 메모리 load 시켜야 돼 => 일단 잘 몰라도 RUNNING으로 바꾸기 전에 이거 쓰자
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      // scheduler로 다시 돌아오고 나서, kernel로 메모리 load 시킴 => 일단 잘 몰라도 swtch끝나고 이거 쓰자
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}
#endif

#if MLFQ
// TODO: L2 queue에 대한 처리를 따로 진행해줘야 해
// TODO: L0, L1 queue는 interrupt를 알려야 하는데
void
scheduler(void)
{
  int qlevel = 0;
  struct proc *cur;
  struct cpu *c = mycpu();
  c->proc = 0;
  for (;;) {
    sti();

    acquire(&ptable.lock);

    while ((cur = Dequeue(qlevel))) {
      // cpu에 현재 process를 할당하고 user mode로 넘어감
      c->proc = cur;
      switchuvm(cur);
      cur->state = RUNNING;

      // call swtch to start the current process
      swtch(&(c->scheduler), cur->context);
      switchkvm();

      // cpu가 작업을 끝냈으니 proc을 null로 세팅
      c->proc = 0;
    }
    qlevel = qlevel <= 1 ? qlevel + 1 : 0;

    release(&ptable.lock);
  }
  
}
#endif

// Enter scheduler.  
// Must hold only ptable.lock and have changed proc->state. 
// Saves and restores
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
  swtch(&p->context, mycpu()->scheduler); // 이거 assembly code 다
  mycpu()->intena = intena; // interrupt enable 인 거 같음
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  // cprintf("yield process\n");
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  Enqueue(myproc(), myproc()->qlevel);
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
    if(p->state == SLEEPING && p->chan == chan) {
      p->state = RUNNABLE;
      Enqueue(p, p->qlevel);
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
      if(p->state == SLEEPING) {
        p->state = RUNNABLE;
      }
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
    cprintf("%d ", p->qlevel);
    cprintf("\n");
  }
}

// 일반적인 상황, p가 queue 중간에 존재하는 경우
struct proc *
cutRelation(struct proc *prev, struct proc *p)
{
  struct queue q = queues[p->qlevel];

  // case 2: p가 queue의 tail에 존재하는 경우
  if (q.tail == p) {
    q.tail = prev;
    prev->qnext = 0;
  }
  // case 3: p가 queue 중간에 존재하는 경우
  else {
    prev->qnext = p->qnext;
    p->qnext = 0;
  }

  return p;
}

void
setPriority(int pid, int priority)
{
  struct proc *p;
  int findprocess = 0;
  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->pid != pid)
      continue;
    
    p->priority = priority;
    findprocess = 1;
    break;
  }
  if (findprocess == 0) {
    cprintf("there is no process which pid is %d\n", pid);
    release(&ptable.lock);
    return;
  }
  // qlevel이 2가 아니면 Dequeue, Enqueue 진행 안하고 종료
  if (p->qlevel != 2) {
    release(&ptable.lock);
    return;
  }

  // p가 RUNNABLE이 아니면 Enqueue 하지 말아야 함
  if (p->state != RUNNABLE) {
    release(&ptable.lock);
    return;
  }
  
  // 현재 proc 이 p
  struct queue q = queues[p->qlevel];
  struct proc *prev = q.head;

  // case 1: prev == q
  if (prev == p) {
    Enqueue(Dequeue(p->qlevel), p->qlevel);
    release(&ptable.lock);
    return;
  }

  // p 이전 process 찾기
  while (prev) {
    if (prev->qnext == p)
      break;
    prev = prev->qnext;
  }

  // cut relation
  p = cutRelation(prev, p);
  Enqueue(p, p->qlevel);
  release(&ptable.lock);
}

void
schedulerLock(int password)
{
  if (password != 2020060100) {
    cprintf("pid: %d\n", myproc()->pid);
    cprintf("time quantum: %d\n", myproc()->timequantum);
    cprintf("current queue level: %d\n", myproc()->qlevel);
    // 해당 프로세스 강제 종료
    exit();
  }
  cprintf("line 686\n");
  // 현재 프로세스에 lock을 걸기 => trap에서 lock이 걸려있으면 yield 시키지 않아
  // global ticks를 0으로 초기화
  if (myproc())
    myproc()->schedLock = 1;
  acquire(&tickslock);
  ticks = 0;
  release(&tickslock);
}

void
schedulerUnlock(int password)
{
  acquire(&ptable.lock);
  // 일단 lock을 잡고 있지 않는 process가 이걸 호출하면 문제돼
  if (password != 2020060100) {
    cprintf("pid: %d\n", myproc()->pid);
    cprintf("time quantum: %d\n", myproc()->timequantum);
    cprintf("current queue level: %d\n", myproc()->qlevel);
    release(&ptable.lock);
    // 해당 프로세스 강제 종료
    exit();
  }
  if (myproc()->schedLock == 0) {
    release(&ptable.lock);
    return;
  }
  /**
   * 해당 프로세스 => myproc()
   * 1. L0 queue에 넣기u
   * 2. time quantum 초기화
   * 3. priority = 3
  */

  // ! 여기서 schedlock 안 풀고 Enqueue 들어가서 풀어야 함 => 그래야 Enqueue할 때 맨 앞에 프로세스를 넣지
  if (myproc()) {
    myproc()->timequantum = 4;
    myproc()->priority = 3;
    myproc()->qlevel = 0;
    myproc()->state = RUNNABLE;
    Enqueue(myproc(), myproc()->qlevel);
    sched();
  }
  release(&ptable.lock);

}

int
getLevel(void)
{
  return myproc()->qlevel;
}

// L2 queue helper function
void
Enqueue2(struct proc *process)
{
  struct queue *q = &queues[2];
  struct proc *p = q->tail;
  if (p == 0) {
    q->head = process;
    q->tail = process;
  }
  else {
    struct proc *cur = q->head;
    struct proc *prev = 0;
    while (cur) {
      if (cur->priority > process->priority)
        break;
      prev = cur;
      cur = cur->qnext;
    }

    if (cur == 0) {
      prev->qnext = process;
      q->tail = process;
    }
    else if (prev == 0) {
      process->qnext = cur;
      q->head = process;
    }
    else {
      process->qnext = cur;
      prev->qnext = process;
    }

  }
}

void
Enqueue(struct proc *process, int type)
{
  struct queue *q = &queues[type];
  struct proc *p = q->tail;
  process->qnext = 0;
  // ! L0 queue의 맨 앞으로 이동 (schedulerUnlock 호출 후 해당 로직 실행)
  if (process->schedLock == 1) {
    process->schedLock = 0;
    // Enqueue 진행
    if (q->head == 0) {
      q->head = process;
      q->tail = process;
    }
    else {
      process->qnext = q->head;
      q->head = process;
    }
    return;
  }

  if (type == 2) {
    Enqueue2(process);
    return;
  }
  
  // tail == null
  if (p == 0) {
    q->head = process;
    q->tail = process;
  }
  else {
    p->qnext = process;
    q->tail = process;
  }
}

struct proc*
Dequeue(int type)
{
  struct queue *q = &queues[type];
  struct proc *p = q->head;
  if (p == 0) {
    return 0;
  }

  q->head = p->qnext;

  if (q->head == 0) {
    q->tail = 0;
  }
  // p 다음 프로세스 연결 끊기
  p->qnext = 0;
  return p;
}



int
IsEmpty(int type)
{
  struct queue *q = &queues[type];
  if (q->head == 0) {
    return 1;
  }
  return 0;
}

void
printProcess(struct proc *p)
{
  // qlevel, priority, timequantum, name
  // 1. name, 2. timequantum, 3. priority, 4. qlevel
  if (p->qnext != 0)
    cprintf("pid: %d\ttq: %d\tpriority: %d\tqlevel: %d -> ", p->pid, p->timequantum, p->priority, p->qlevel);
  else
    cprintf("pid: %d\ttq: %d\tpriority: %d\tqlevel: %d", p->pid, p->timequantum, p->priority, p->qlevel);
}


void
printQueue()
{
  acquire(&ptable.lock);
  cprintf("\n---------------------------Epoch %d---------------------------\n", ticks);
  if (myproc()) {
    cprintf("current process\t\tpid: %d tq: %d priority: %d qlevel: %d\n", myproc()->pid, myproc()->timequantum, myproc()->priority, myproc()->qlevel);
  }
  for (int i = 0; i < 3; i++) {
    int count = 0;
    struct queue *q = &queues[i];
    struct proc *p = q->head;
    cprintf("L%d: ", i);
    while (p) {
      printProcess(p);
      count++;
      p = p->qnext;
    }
    // cprintf("%d", count);
    cprintf("\n");
  }
  release(&ptable.lock);
}

// tickslock 점유된 채로 이 함수 호출
void
priorityBoosting()
{
  ticks = 0;
  struct proc *p;
  struct queue *L0 = &queues[0];
  struct queue *L1 = &queues[1];
  struct queue *L2 = &queues[2];
  acquire(&ptable.lock);

  // 모든 프로세스 초기화
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->state != RUNNABLE)
      continue;
    p->qlevel = 0;
    p->priority = 3;
    p->timequantum = 4;
  }

  // L2를 L1 뒤에 꼬리 연결
  if (L2->tail != 0) {
    if (L1->tail != 0) {
      L1->tail->qnext = L2->head;
      L1->tail = L2->tail;
    }
    else {
      L1->tail = L2->tail;
      L1->head = L2->head;
    }
    L2->head = 0;
    L2->tail = 0;
  }
  // L1을 L2 뒤에 꼬리 연결
  if (L1->tail != 0) {
    if (L0->tail != 0) {
      L0->tail->qnext = L1->head;
      L0->tail = L1->tail;
    }
    else {
      L0->tail = L1->tail;
      L0->head = L1->head;
    }
    L1->head = 0;
    L1->tail = 0;
  }
  if (myproc()) {
    myproc()->qlevel = 0;
    myproc()->timequantum = 4;
    myproc()->priority = 3;
    myproc()->schedLock = 0;
    // myproc()->state = RUNNABLE;
  
  }
  
  
  release(&ptable.lock);
}
