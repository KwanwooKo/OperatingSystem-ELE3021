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

void
tvinit(void)
{
  int i;
  // 2주차: DPL에 해당하는 부분
  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
                                              // default: kernel mode(0으로 설정)
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);
  SETGATE(idt[PRAC2_INT], 1, SEG_KCODE<<3, vectors[PRAC2_INT], DPL_USER); // 해당 INT(128)에 대해서는 user 권한에 없었으니까, 이걸 줘야 가능하네
  SETGATE(idt[SCHEDLOCK], 1, SEG_KCODE<<3, vectors[SCHEDLOCK], DPL_USER);
  SETGATE(idt[SCHEDUNLOCK], 1, SEG_KCODE<<3, vectors[SCHEDUNLOCK], DPL_USER);
                                              // only int T_SYSCALL can be called from user-level

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
  int priorityBoosted = 0;
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed) {
      exit();
    }
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed) {
      exit();
    }
    return;
  }


  switch(tf->trapno){
  // 이 case가 TIMER INTERRUPT 발생
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      // TODO: Global tick이 100 ticks가 될 때마다 모든 프로세스들은 L0 큐로 재조정 됩니다.
      // TODO: queue 재조정 하는 함수 하나 만들어야 할듯
      // TODO: 모든 프로세스들의 priority 값은 3으로 재설정 됩니다.
      // TODO: 애초에 priority boosting을 하는 함수 하나를 만들면 되겠다
      // printQueue();
      if (ticks == 100) {
        priorityBoosting();
        priorityBoosted = 1;
      }
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
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;
  case PRAC2_INT:
    myproc()->tf = tf;
    mycall();
    exit();
    break;
  // schedulerLock, schedulerUnlock int case
  case SCHEDLOCK:
  // Lock 점유
    schedulerLock(2020060100);
    break;
  case SCHEDUNLOCK:
  // Lock 해제
    schedulerUnlock(2020060100);
    break;

  // PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  // TODO: timer interrupt 관련한 내용들은 여기서 진행(Enqueue, Dequeue 등)
  if(myproc() && myproc()->state == RUNNING
              && tf->trapno == T_IRQ0+IRQ_TIMER && myproc()->schedLock == 0) {
    // timequantum이 0이되면 qlevel 증가
    if (--myproc()->timequantum == 0) {
      myproc()->qlevel++;
      // 증가시켜서 3이 되면 다시 2로 재조정 + priority 감소
      if (myproc()->qlevel == 3) {
        myproc()->qlevel = 2;
        myproc()->priority = myproc()->priority == 0 ? 0 : myproc()->priority - 1;
      }
      myproc()->timequantum = 2 * myproc()->qlevel + 4;
    }
    if (priorityBoosted == 0)
      yield();
  }

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
