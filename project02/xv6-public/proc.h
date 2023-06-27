

// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int ncpu;

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;             // Program counter
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };


typedef struct thread thread;
// Per-process state
struct proc {
  uint sz;                            // Size of process memory (bytes)
  pde_t* pgdir;                       // Page table
  char *kstack;                       // Bottom of kernel stack for this process => 이전 쓰레드의 trapframe이 저장됨
  enum procstate state;               // Process state
  int pid;                            // Process ID
  int tid;                            // Thread ID
  struct proc *parent;                // Parent process
  struct trapframe *tf;               // Trap frame for current syscall
  struct context *context;            // swtch() here to run process
  void *chan;                         // If non-zero, sleeping on chan
  int killed;                         // If non-zero, have been killed
  struct file *ofile[NOFILE];         // Open files
  struct inode *cwd;                  // Current directory
  char name[16];                      // Process name (debugging)
  int stacksize;                      // process가 할당받은 stacksize(페이지의 개수)
  uint szlimit;                       // process의 최대 사이즈
  int threadnum;                      // 해당 프로세스에서 실행시킨 thread의 개수
  int isThread;                       // 이게 쓰레드인지 아닌지 구분
  int oldsz;
  pde_t* oldpgdir;                    // 처음 pgdir
  void *(*start_routine) (void *);    // routine 저장
  void *arg;                          // arg 저장
  void *retval;                       // retval 저장
};


// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
