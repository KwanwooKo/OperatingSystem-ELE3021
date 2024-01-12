# Project01-Design

## Design

### 구현 계획 및 자료구조

기본적으로 xv6는 ptable에 프로세스를 등록하고, ptable에 RUNNABLE한 프로세스를 순회하면서 어떤 프로세스를 스케줄링 할지 결정한다. 해당 코드는 proc.c 파일에 scheduler 함수에 구현되어 있다.

기본적으로 프로세스를 등록하고, 종료하는 일은 기존의 xv6가 다 처리해주기 때문에 해당 부분은 최대한 건드리지 않는 방향으로 진행하려 한다. 그래서 나는 scheduler 함수의 로직만을 수정하려 한다.

일단 MLFQ 방식은 queue가 3개 필요하고, L0과 L1 queue는 Round Robin 방식으로 진행되어야 하고 L2 queue는 priority가 높은(숫자적으로는 낮은) 프로세스부터 진행돼야 한다.

이를 위해 struct queue라는 구조체 type을 선언하고, 해당 구조체 타입을 크기가 3인 배열로 관리해서 프로젝트를 진행할 계획이다.

그 다음은 queue 자체의 구조인데, queue 내부 로직은 연결리스트를 통해 구현할 예정이다. 연결리스트를 사용하는 이유는 두가지가 있다

1. 포인터를 이용해 ptable의 process의 주소를 그대로 가져와서, physical하게는 정렬되어 있지 않지만, logical 하게 정렬된 상태를 구현하려 한다.
2. L2 queue에서 priority가 변경될 때마다 해당 프로세스의 위치를 조정해야하는데, 이 때 배열로 하기에는 시간복잡도를 많이 소모한다고 판단해서 연결리스트를 통해 구현하려 한다.

struct queue의 구조체는 해당 queue에 존재하는 첫번째 process(head)와 마지막 process(tail)을 저장하도록 한다.

그림을 그려보면 아래와 같은 그림이 나온다.

![Untitled](Project01-Design%20a30377a84fa043b3a2fa332461b1973f/Untitled.png)

이런식으로 각각의 queue에 맞게 순서를 관리하면 실제로 process는 ptable에 존재하지만, 해당 프로세스를 MLFQ의 순서에 맞게 실행할 수 있다.

그래서 queue에서 제공하는 기능은 Enqueue와 Dequeue 기능이고

Enqueue는 해당 queue의 tail에 추가되고,

Dequeue는 해당 queue의 head가 pop이 되도록 했다.

위의 그림에서 L2 queue가 순서대로 그려졌지만, 실제로 L2 queue는 priority에 의해서 순서가 결정되게 하려 한다.

이 자료구조의 시간복잡도를 생각해보면

L0 과 L1 queue의 Enqueue 및 Dequeue의 시간복잡도는 O(1)로 해결이 가능하고

L2 queue의 Enqueue는 적합한 위치를 찾아야 하므로 O(n)의 시간복잡도를 갖고, Dequeue는 동일하게 O(1)로 해결이 가능하다.

위의 구조를 구현하기 위해 struct proc에 변수를 추가했다.

추가한 변수로는 qlevel, priority, timequantum, qnext, schedlock이 있다.

여기서 기존의 명세를 제가 이해한 방식과 조교님이 제시하신 것과 조금 다른 부분이 있는 것 같아 보충 설명을 합니다.

저는 timequantum을 남은 ticks으로 사용했습니다. 즉 처음에 L0 queue에 들어가면 4로 초기화돼서 들어갑니다.

![스크린샷 2023-04-11 오후 6.31.54.png](Project01-Design%20a30377a84fa043b3a2fa332461b1973f/%25E1%2584%2589%25E1%2585%25B3%25E1%2584%258F%25E1%2585%25B3%25E1%2584%2585%25E1%2585%25B5%25E1%2586%25AB%25E1%2584%2589%25E1%2585%25A3%25E1%2586%25BA_2023-04-11_%25E1%2584%258B%25E1%2585%25A9%25E1%2584%2592%25E1%2585%25AE_6.31.54.png)

해당 변수들의 초기화는 allocproc에서 진행한다.

## Implement

1. queue의 자료형은 아래와 같다

```c
struct queue {
  struct proc *head;
  struct proc *tail;
};
```

1. queue의 기능을 지원하는 Enqueue와 Dequeue는 아래와 같다
    1. 이때 Enqueue는 L2 queue에 대해서 따로 처리를 해주었고, 해당 기능을 돕기위해 Enqueue2라는 helper function을 만들었다

```c
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
```

1. allocproc함수에서 내가 추가한 변수들을 초기화 해주도록 한다.
    
    ![스크린샷 2023-04-11 오후 6.34.58.png](Project01-Design%20a30377a84fa043b3a2fa332461b1973f/%25E1%2584%2589%25E1%2585%25B3%25E1%2584%258F%25E1%2585%25B3%25E1%2584%2585%25E1%2585%25B5%25E1%2586%25AB%25E1%2584%2589%25E1%2585%25A3%25E1%2586%25BA_2023-04-11_%25E1%2584%258B%25E1%2585%25A9%25E1%2584%2592%25E1%2585%25AE_6.34.58.png)
    

1. 기존의 scheduler함수를 MLFQ에 맞게 수정하였다.
    1. 이 때 메크로를 이용해서 기존의 round robin으로 작동하는 scheduler함수는 작동하지 못하게 막아두었다.
    2. 기본적으로 L0과 L1 queue에서는 다음 level의 queue를 확인해야 하므로 qlevel을 증가시키고, L2 queue에서는 원래의 L0 queue로 돌아가기 위해 qlevel을 0으로 초기화 시킨다.
    3. schduler가 제대로 작동하는지 확인해보기 위해서 qlevel을 초기화 할때의 조건을 1보다 작은 경우로 해봤을 때, xv6가 정상적으로 작동하지 않는 것을 확인할 수 있다. 이를 통해 scheduler가 정상적으로 작동하고 있음을 판단하였다.

```c
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
    // L0, L1 queue
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
```

1. Priority boosting은 trap함수에서 ticks == 100 일때 실행한다.
    1. ptable에서 RUNNABLE인 상태의 process의 qlevel, priority, timequantum을 초기화 시킨다.
    2. 모든 프로세스를 연결을 다시 할 필요없이, L1의 tail에 L2의 head를 연결하고, L0의 tail에 L1의 head를 연결해서 L0 queue로 모든 프로세스를 재조정 한다.
    3. 이 때 만약 현재 cpu가 잡고있는 process가 scheduler를 lock하고 있다면 해당 lock을 풀어준다.
    4. priority boosting이 발생하면 모든 프로세스가 L0 queue로 재조정 되는데 이때 현재 실행되고 있는 프로세스는 L0 queue의 맨 앞으로 들어간다. 이렇게 되면 다음 tick에서도 현재 myproc()이 실행되므로 그냥 myproc()의 상태만 변경해주고 yield를 시키지 않았다. 이렇게하면 다음 틱에서 해당 프로세스를 다시 잡은 것과 같은 효과를 낼 수 있다.

```c
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
```

1. yield
    1. 기존의 yield는 시스템콜(int $64)로 등록되어있지 않아서 유저모드에서 사용할 수 없었는데, yield를 시스템콜로 등록한다.
    2. schduler에서 맨 앞의 process를 Dequeue하면서 프로세스를 진행하므로, 여기서 현재 프로세스를 Enqueue해주는 작업을 추가해야 한다.

```c
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
```

1. getLevel
    1. 현재 프로세스가 속한 큐의 레벨을 반환하도록 했다.

```c
int
getLevel(void)
{
  return myproc()->qlevel;
}
```

1. setPriority
    1. ptable에서 pid에 해당하는 프로세스를 찾고, 해당 프로세스의 priority를 변경한다
    2. 만약 해당 pid를 가진 프로세스가 없다면, 함수를 종료한다.
    3. 만약 L2 queue에 존재하는 프로세스를 변경한다면, queue 내부에서의 해당 프로세스의 위치가 변경되어야 하므로, 해당 process를 Dequeue하고 다시 Enqueue를 진행한다.
    4. Dequeue과정을 처리하기 위해 cutRelation이라는 helper function을 만들어서 진행했다.
    5. 해당 함수는 ptable의 값을 변경하기 때문에 반드시 lock을 호출해야 한다.

```c
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
  if (p->qlevel != 2)
    return;
  
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
```

1. schedulerLock 및 schedulerUnlock 함수
    1. 우선 해당 시스템콜은 user에서 어떻게 사용할 지 몰라서 2가지 case로 준비했다.
    2. 첫번째로, 유저 코드에서 interrupt를 발생시켜서 사용하는 경우, trap함수 내부에서 case를 추가해서 해당 함수를 호출하도록 하였고
    3. 두번째로는, 유저 코드에서 직접적으로 해당함수를 호출시키는 경우가 있어서 해당 함수를 시스템콜(int $64)로 등록했다.
    4. 이렇게 관리할 경우, 유저 코드 내부에서 interrupt를 발생시켜서 처리를 하든, 직접 해당 함수를 호출해서 처리를 하든 모두 관리가 된다
    5. scheduler의 lock을 얻기위해 struct proc 구조체에 schedlock 변수를 추가했고, scheduler의 lock을 점유하면 해당 변수의 값이 1로, 점유하지 않는다면 0으로 바꾸도록 했다.
    6. 만약 schedlock을 점유하지 않은 채로 schedulerUnlock함수를 호출하는 경우, 해당 함수를 종료하도록 했다. 이 케이스가 발생할 수 있는 이유는 schedulerUnlock이 호출되기전에 priorityBoosting이 발생하면서 schedulerLock을 풀어줬기 때문이다.
    7. schedulerUnlock이 성공적으로 호출될 때, time quantum 초기화, priority 초기화, state를 RUNNABLE로 변경하고 L0 queue의 맨 앞으로 이동 시켰다.
    8. schedulerLock의 비밀번호가 틀려서 프로세스를 강제종료 시키는 경우, time quantum을 사용할 수 있는 남은 tick의 개념으로 사용했습니다.

```c
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
  // 현재 프로세스에 lock을 걸기 => trap에서 lock이 걸려있으면 yield 시키지 않아
  // global ticks를 0으로 초기화
  myproc()->schedLock = 1;
  ticks = 0;
}

void
schedulerUnlock(int password)
{
  if (password != 2020060100) {
    cprintf("pid: %d\n", myproc()->pid);
    cprintf("time quantum: %d\n", myproc()->timequantum);
    cprintf("current queue level: %d\n", myproc()->qlevel);
    // 해당 프로세스 강제 종료
    exit();
  }
  if (myproc()->schedLock == 0)
    return;
  /**
   * 해당 프로세스 => myproc()
   * 1. L0 queue에 넣기
   * 2. time quantum 초기화
   * 3. priority = 3
  */
  // ! Enqueue를 할 이유가 없음 => 현재 myproc이 mycpu에 의해 스케줄링 되고 있고, 해당 프로세스의 qlevel만 바꿔주면 무조건 맨 앞의 큐를 보장
  myproc()->schedLock = 0;
  myproc()->timequantum = 4;
  myproc()->priority = 3;
  myproc()->qlevel = 0;
  
}
```

1. trap에서 수정한 내용
    1. tvinit에서 int 129, 130에 대해서 DPL_USER권한을 주었다.
    2. IRQ_TIMER에서 ticks가 100이 될때, priorityBoosting을 호출
    3. 하단 if문에서 myproc()→RUNNING인 경우에서 조건을 추가하고, qlevel, priority 및 timequantum을 초기화 한다.
    4. priorityboosting이 발생하는 경우 원래는 L0 큐의 맨 앞에 현재 프로세스를 넣어야 하지만, 제 디자인에서 어차피 모든 프로세스가 L0 큐로 변경되고 현재의 myproc()이 다음틱에서 다시 myproc()이 될 예정이므로 해당 프로세스를 큐의 맨 앞에 넣지 않고 yield를 시키자 않았습니다.

```c
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
```

1. 기존에 있던 함수 중 Enqueue를 진행한 함수
    1. userinit, fork, yield, wakeup1
    2. fork, yield는 당연히 해당 프로세스를 다시 queue에 넣어야 하기 때문에 진행
    3. userinit과 wakeup1 함수는 처음으로 실행되는 프로세스가 queue에 진입을 해야 cpu가 해당 프로세스를 처리할 수 있기 때문에 Enqueue 작업을 해줘야 한다.

### Wrapper function

wrapper function에서는 기본적으로 오류가 발생하지 않으면 0을 반환하고 오류가 발생하면 -1을 반환하도록 했다.

단 sys_getLevel 함수는 해당 프로세스의 level을 반환하도록 했다.

### Result

[makexv6.sh](http://makexv6.sh) 파일을 작성하였고, 해당 파일을 실행시키면 Makefile을 통해 build가 된다.

[bootxv6.sh](http://bootxv6.sh) 파일을 작성하였고, build 이후에 해당 파일을 실행시키면 xv6가 부팅된다.

![스크린샷 2023-04-13 오후 2.12.35.png](Project01-Design%20a30377a84fa043b3a2fa332461b1973f/%25E1%2584%2589%25E1%2585%25B3%25E1%2584%258F%25E1%2585%25B3%25E1%2584%2585%25E1%2585%25B5%25E1%2586%25AB%25E1%2584%2589%25E1%2585%25A3%25E1%2586%25BA_2023-04-13_%25E1%2584%258B%25E1%2585%25A9%25E1%2584%2592%25E1%2585%25AE_2.12.35.png)

실행하면 위와 같은 화면이 나온다.

만든 함수들이 정상적으로 작동하는지 확인하기 위해 project_test, yield_test, schedlock_test 를 만들어봤다.

1. project_test
    1. MLFQ 스케줄러가 정상적으로 작동하는지 확인해보았다.
    2. 반복문을 돌면서(1000만) 강제로 프로세스를 종료시키도록 했는데, 이 때 i 값을 계속 수정해보면서 테스트를 진행해봤다.
    3. i == 10 일 때 프로세스 정보 
        
        ![스크린샷 2023-04-13 오후 2.19.47.png](Project01-Design%20a30377a84fa043b3a2fa332461b1973f/%25E1%2584%2589%25E1%2585%25B3%25E1%2584%258F%25E1%2585%25B3%25E1%2584%2585%25E1%2585%25B5%25E1%2586%25AB%25E1%2584%2589%25E1%2585%25A3%25E1%2586%25BA_2023-04-13_%25E1%2584%258B%25E1%2585%25A9%25E1%2584%2592%25E1%2585%25AE_2.19.47.png)
        
    4. i == 50 일 때 프로세스 정보
        
        ![스크린샷 2023-04-13 오후 2.21.04.png](Project01-Design%20a30377a84fa043b3a2fa332461b1973f/%25E1%2584%2589%25E1%2585%25B3%25E1%2584%258F%25E1%2585%25B3%25E1%2584%2585%25E1%2585%25B5%25E1%2586%25AB%25E1%2584%2589%25E1%2585%25A3%25E1%2586%25BA_2023-04-13_%25E1%2584%258B%25E1%2585%25A9%25E1%2584%2592%25E1%2585%25AE_2.21.04.png)
        
    
    이렇게 값을 확인하면서 time quantum과 queue level이 변경되는 것을 확인할 수 있다.
    
2. mlfq_test
    
    ![스크린샷 2023-04-22 오후 8.56.09.png](Project01-Design%20a30377a84fa043b3a2fa332461b1973f/%25E1%2584%2589%25E1%2585%25B3%25E1%2584%258F%25E1%2585%25B3%25E1%2584%2585%25E1%2585%25B5%25E1%2586%25AB%25E1%2584%2589%25E1%2585%25A3%25E1%2586%25BA_2023-04-22_%25E1%2584%258B%25E1%2585%25A9%25E1%2584%2592%25E1%2585%25AE_8.56.09.png)
    

피아자에서 제공된 테스트 코드를 실행시켰을 때 위와 같은 결과가 나온다.

해당 테스트는 당연히 오차가 존재할 수 있지만, 적어도 L2가 L1보다 많이 실행되고, L1이 L0보다 많이 실행되는 구조를 만족해야 한다.

### Makefile

gdb를 사용하기 위해 CFLAGS에서 -g 옵션을 추가해줬고, CPUS = 1로 고정했습니다.

### Trouble shooting

다른 함수들은 크게 어려움이 없었지만, scheduler 함수를 구현하고 나서 처음 실행을 했을 때 sh process가 실행되지 않아서 당황했다.

이를 처리하기 위해 init함수를 살펴봤고 call path를 따라가다보니 userinit 및 wakeup1 함수에서 해당 프로세스가 queue에 들어가야한다는 것을 알아서 해당 프로세스를 queue에 넣어주었더니 문제가 해결되었다.

또한 디버깅 하는 것이 생각보다 힘들었는데 우선 이 이유는 출력문으로 디버깅하기에는 속도가 너무 빨라서 이를 잡기가 까다로웠습니다. 그래서 테스트 코드로 작성해보려 했는데 테스트 코드도 작성하는 것이 어려워서 출력문을 넣고 qemu를 종료시키면서 테스트 해봤습니다. 그래서 이번에 xv6를 수정하면서 gdb의 필요성을 느껴서 gdb를 연결해서 메모리가 제가 생각하는 대로 움직이는지 확인해봤습니다.

다만 아쉬웠던건 gdb에 익숙하지 않아서 모든 경우를 확인하지 못한 것입니다. 그래서 개인적으로 나중에 실습수업 때 gdb를 이용한 디버깅 방법 같은 것들을 좀 더 자세히 배울 수 있으면 좋을 것 같습니다.
