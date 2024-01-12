# Project02-wiki

## Design

1. int exec2(char *path, char **argv, int stacksize)
    
    해당 함수는 기존의 exec 함수가 page를 2개씩 할당 받던 것을, stacksize 개수만큼 할당 받도록 변경하는 함수이다.
    
    해당 함수는 기존의 exec 함수에서 대부분 가져왔는데, 
    
    ![스크린샷 2023-05-25 오후 9.00.41.png](Project02-wiki%209c1d7cf7e8f545cbb3d9ae4a962f44b8/%25E1%2584%2589%25E1%2585%25B3%25E1%2584%258F%25E1%2585%25B3%25E1%2584%2585%25E1%2585%25B5%25E1%2586%25AB%25E1%2584%2589%25E1%2585%25A3%25E1%2586%25BA_2023-05-25_%25E1%2584%258B%25E1%2585%25A9%25E1%2584%2592%25E1%2585%25AE_9.00.41.png)
    
    이 부분에서 2를 stacksize + 1로 변경하고, proc 구조체에 stacksize를 저장하는 변수를 추가해서, stacksize를 저장하도록 했다.
    
2. int setmemorylimit(int pid, int limit)
    
    이 함수에서는 ptable에서 pid에 맞는 프로세스를 발견하고 해당 프로세스의 szlimit(proc 구조체에서 추가) 값을 limit으로 설정해주었다.
    
    0인 경우에는 별도로 예외처리를 해주었다. 
    
    해당 pid의 프로세스를 발견하지 못하거나, 기존에 할당받은 sz가 limit보다 큰 경우에는 -1을 반환하도록 했고, 그 외의 경우에는 0을 반환하도록 했다.
    

### Pmanager

pmanager에서 기존의 sh코드의 runcmd함수가 BACK 명령에 대해 처리하는 것을 확인하는 것이 tip으로 주어졌는데, 이 부분은 잘 이해가 가지 않았습니다.

제가 구현한 방식은 user library에 있는 strchr, malloc과 free를 이용해 문자열을 할당하도록 하고 command line을 파싱해서 strcmp함수로 비교해서 처리했습니다.

이때 주의해야할 점은 sh코드와 다르게, pmanager는 wait으로 다른 프로세스들의 종료를 기다리지 않습니다.

다른 프로세스들이 실행되는 동안에도 pmanager 명령들은 수행되어야 하기 때문에 1tick씩 번갈아가면서 다른 프로세스들이 실행돼야 합니다.

1. list
    
    현재 실행 중인 프로세스를 출력하는데, 이 때 출력하는 state로는 RUNNING, RUNNABLE, SLEEPING을 출력하도록 했다. 
    
    이때 RUNNING외에도 RUNNABLE과 SLEEPING도 출력하는 이유는 pmanager에서 list명령을 호출할 때 CPU는 무조건 pmanager를 실행하기 때문에 list에 pmanager만 나온다.
    
    또한 thread를 갖고 있는 프로세스를 실행시키게 되면, main thread가 다른 스레드들이 종료되는 것을 기다리므로, 이 때 기다리는 프로세스가 누구인지 출력하기 위해서 SLEEPING도 출력하도록 했다.
    
    만약 SLEEPING을 출력하지 않는다면, 명세에서 thread를 출력하지 않는다 라고 했으므로 해당 프로세스의 정보는 출력되지 않는다.
    
    list 명령을 수행하기 위해 시스템콜로 printProcList와 printProc를 정의했다.
    
2. kill
    
    kill 시스템콜을 그대로 가져다 사용했습니다.
    
    pmanager 자신을 종료하는 것이 정상적인 행동은 아니지만, pmanager를 종료하게 될 경우 반환 값으로 성공여부를 파악해서 출력문을 작성할 경우 해당 출력문이 출력되지 않습니다.
    
    이에 따라 kill 시스템콜 내부에서 성공여부를 출력할 수 있도록 했습니다.
    
    하지만 이렇게 할경우 다른 경우에 kill을 호출할 때에도 성공여부를 출력하게 됩니다.
    
    또한 뒤의 thread를 갖고 있는 프로세스는 thread와 process의 pid가 모두 같기 때문에 해당 pid를 가진 모든 프로세스 및 스레드들의 kill flag를 true로 변경하고 모든 프로세스 및 스레드들을 trap에서 종료합니다.
    
3. execute
    
    path의 경로에 위치한 프로그램을 stacksize 개수만큼 스택용 페이지를 할당해서 실행합니다.
    
    이전에 작성하였던 exec2 함수를 이용해서 작성했습니다.
    
    이 때 그냥 exec2함수를 호출하게 되면 해당 context로 변경되고 더이상 sh이 실행되지 않으므로
    
    fork를 하고 자식 프로세스인 경우에 한해서만 exec2을 호출합니다.
    
4. memlim
    
    이전에 작성하였던 setmemorylimit 함수를 이용해서 작성했습니다.
    
    setmemorylimit에서 알아서 pid를 찾아서 할당하므로 해당 함수를 호출하는 것으로 처리했습니다.
    
5. exit
    
    exit 시스템콜을 호출해서 pmanager를 종료했습니다
    

### LWP

처음에 LWP를 구현할 때에 thread의 구조체를 만들어야 하는지 고려했습니다. 뒤의 tip을 봤을 때, LWP도 Process라는 hint를 확인하고 LWP도 proc 구조체를 이용해서 처리하면 되겠다고 생각했습니다.

그래서 proc 구조체 내부에서 process와 thread를 구분할 수 있는 변수를 추가해주고, process가 thread를 몇개 갖고 있는지 확인했습니다.

또한 thread_exit을 하면서 retval을 저장하고, thread_join에서 retval을 받아오기 위해 retval도 추가했습니다.

기존의 process는 parent를 갖는데, thread들의 parent를 main thread로 설정했습니다. 이렇게 처리한 이유는 자원회수를 하는 스레드가 별도로 필요한데 pthread관련 사용법에서 main함수에서 join으로 해당 스레드들의 자원을 회수하기 때문에 parent를 main thread로 설정했습니다.

1. thread_create
    
    우선 thread_create는 기존의 프로세스를 생성하는 과정, fork에서 프로세스를 복사하는 과정, exec에서 프로세스의 img를 변경하는 과정을 잘 섞어서 사용해야 한다고 생각했습니다.
    
    1. allocproc으로 스레드를 생성
    2. 기존의 fork코드는 pgdir을 copyuvm 함수를 통해 복사하지만, 스레드와 프로세스는 pgdir을 공유하므로, pgdir의 주소를 그대로 스레드의 pgdir에 저장하도록 하고 pid도 동일하게 설정했습니다.
    3. 스레드간에 서로 다른 스레드인 것을 구별해야 하므로 tid를 추가하고, tid는 기존의 프로세스의 pid를 관리하는 방식과 동일하게 nexttid를 전역변수로 선언해서 처리했습니다.
    4. 기존의 fork코드와 동일하게 스레드의 parent를 main thread로 설정하고 trapframe을 복사했습니다.
    5. exec 코드에서 start_routine과 arg를 저장하는 코드가 나와있는데, exec코드는 elf(executable and linkable format)으로 프로세스의 실행을 시작하는 주소를 저장하는데 스레드는 이 부분이 start_routine으로 생각했습니다.
    6. 그래서 trapframe의 eip레지스터를 start_routine으로 설정했습니다.
    7. 또한 기존의 exec코드는 argc와 argv를 구분해서 저장하는 것을 확인할 수 있습니다. 하지만 thread_create를 사용할 때 arg를 구조체를 이용해서 할당하는 것을 확인했습니다. 이렇게 되면 argc를 쓰지 않고, byte단위로 그대로 복사하고 사용자가 구분해서 사용한다고 생각했습니다.
    8. 그래서 ustack에서 argv를 저장하는 코드를 단순히 arg를 저장하게 했습니다.
    9. 그 외에 safestrcpy에서 name은 main thread와 동일하게 설정했습니다.
2. thread_exit
    
    thread_exit은 exit함수와 동일하게 동작한다고 생각해서 exit함수의 코드를 그대로 가져왔습니다.
    
    다만, retval을 저장해야 하므로 retval을 저장하는 코드만 별도로 추가했습니다.
    
3. thread_join
    
    thread_join은 wait함수와 동일하게 동작한다고 생각해서 wait함수의 코드를 그대로 가져왔습니다.
    
    다만 기존의 wait함수는 프로세스가 pgdir을 정리하는데
    
    해당 기능을 thread_join에서도 하게 되면 문제가 발생합니다. 하나의 스레드가 종료가 돼도 다른 스레드가 실행 중일 수 있기 때문에 pgdir을 정리하지 않고 모든 스레드가 다 종료될 때 wait함수에서 pgdir을 정리하도록 했습니다.
    

### Specification-System call

1. fork
    
    스레드에서 fork가 호출될 때 fork가 정상적으로 실행돼야 한다.
    
    애초에 thread를 만들 때 process와 거의 유사하게 만들었으므로 정상적으로 작동한다.
    
2. exec
    
    처음에 exec을 따로 건드리지 않았는데, test code와 test 명세를 보고 수정해야 하는 것을 확인했다.
    
    처음에 수정하지 않아도 된다고 생각했던 이유는 어차피 exec을 실행하면 원래 context로 넘어오지 않기 때문에
    
    자원정리를 굳이 하지 않아도 될 것이라고 판단했으나
    
    스레드가 exec을 독립적으로 실행할 수 있으므로, 스레드들을 정리해야 하는 것을 확인했다.
    
    그래서 exec을 하기 전에 killthread라는 함수를 만들어, 스레드 자원들을 다 정리하도록 했다.
    
3. sbrk
    
    sbrk함수는 별도의 시스템콜이 없고, sys_sbrk함수로 처리한다.
    
    growproc함수를 이용해서 메모리를 할당하는데 이 때 lock을 걸어줘야 한다.
    
    또한 thread들은 부모(main thread)와 sz를 공유한다.
    
    그래서 growproc에서 thread인 경우에 부모 및 다른 thread와 sz값을 동일하게 설정한다.
    
4. kill
    
    기존의 kill함수는 process 하나에 kill flag를 세우고 이후에 trap함수에서 exit처리를 해줬는데,
    
    스레드가 도입되면서 스레드들까지 정리돼야 하므로 스레드들까지 kill flag를 true로 설정했다.
    
    어차피 이후에 exit에서 처리될 것이므로 kill에서는 동일한 pid를 가진 프로세스 및 모든 스레드들의 kill flag를 true로 설정했다.
    
5. sleep
    
    sleep함수는 myproc을 sleep상태로 변경하므로, 별도의 수정이 필요하지 않다.
    

## 실행 화면

### thread_test

![스크린샷 2023-05-28 오전 1.48.35.png](Project02-wiki%209c1d7cf7e8f545cbb3d9ae4a962f44b8/%25E1%2584%2589%25E1%2585%25B3%25E1%2584%258F%25E1%2585%25B3%25E1%2584%2585%25E1%2585%25B5%25E1%2586%25AB%25E1%2584%2589%25E1%2585%25A3%25E1%2586%25BA_2023-05-28_%25E1%2584%258B%25E1%2585%25A9%25E1%2584%258C%25E1%2585%25A5%25E1%2586%25AB_1.48.35.png)

### thread_exec

![스크린샷 2023-05-28 오전 1.49.07.png](Project02-wiki%209c1d7cf7e8f545cbb3d9ae4a962f44b8/%25E1%2584%2589%25E1%2585%25B3%25E1%2584%258F%25E1%2585%25B3%25E1%2584%2585%25E1%2585%25B5%25E1%2586%25AB%25E1%2584%2589%25E1%2585%25A3%25E1%2586%25BA_2023-05-28_%25E1%2584%258B%25E1%2585%25A9%25E1%2584%258C%25E1%2585%25A5%25E1%2586%25AB_1.49.07.png)

### thread_exit

![스크린샷 2023-05-28 오전 1.49.38.png](Project02-wiki%209c1d7cf7e8f545cbb3d9ae4a962f44b8/%25E1%2584%2589%25E1%2585%25B3%25E1%2584%258F%25E1%2585%25B3%25E1%2584%2585%25E1%2585%25B5%25E1%2586%25AB%25E1%2584%2589%25E1%2585%25A3%25E1%2586%25BA_2023-05-28_%25E1%2584%258B%25E1%2585%25A9%25E1%2584%258C%25E1%2585%25A5%25E1%2586%25AB_1.49.38.png)

뒤의 0으로 출력되는 부분은 첫번째로 실행된 스레드(0번 스레드)가 종료되는지 확인하기 위해 별도로 출력했습니다.

### thread_kill

![스크린샷 2023-05-28 오전 1.50.01.png](Project02-wiki%209c1d7cf7e8f545cbb3d9ae4a962f44b8/%25E1%2584%2589%25E1%2585%25B3%25E1%2584%258F%25E1%2585%25B3%25E1%2584%2585%25E1%2585%25B5%25E1%2586%25AB%25E1%2584%2589%25E1%2585%25A3%25E1%2586%25BA_2023-05-28_%25E1%2584%258B%25E1%2585%25A9%25E1%2584%258C%25E1%2585%25A5%25E1%2586%25AB_1.50.01.png)

### Trouble Shooting

1. LWP 구현과정
    
    우선 기본적으로 처음에 LWP를 어떻게 설계해야할지가 가장 어려웠습니다.
    
    처음에는 thread와 process가 별개라고 생각하고 별도의 구조체를 선언하고 이를 memmove를 이용해서 byte로 복사하려했는데 막상 작성하고 보니, 구조체 변수가 대부분 같고 process와 LWP가 비슷하다는 tip을 보고 LWP와 process를 서로 구분하는 것만 있으면 되겠다 라고 판단해서 isThread라는 변수를 추가해서 이를 구분하도록 했습니다. 
    
2. thread 관련 함수들
    
    처음에 우선적으로 봤던 건 hint 였는데 hint에서 fork, exec, exit, wait 함수를 분석해보라는 tip을 보고 이를 우선적으로 봤습니다. 이 때 생각했던건 thread_create에는 fork와 exec 코드가 그대로 들어가면 된다고 생각했고, thread_exit에는 exit함수가, thread_join에는 wait함수가 들어가면 된다고 판단했습니다. 그래서 코드를 일단 그대로 복사해왔습니다.
    
    다만 이해가 가지 않았던 부분은 parent(main thread)와 thread의 sz값이 같은 점인데 아마 이 부분은 process의 pgdir을 공유하면서 해당 지점부터 쌓아올리기 때문에 스레드들도 어느 지점부터 쌓는지 알아야 하기 때문에 이 값을 동일하게 맞춰줘야 된다고 생각하게 됐습니다. sz값을 같게 맞춰주지 않으면 sbrk test에서 remap 에러가 발생하고 제가 별도로 작성한 thread test들에서 재부팅이 발생하는 문제가 발생했습니다.
    
3. exit
    
    테스트 코드가 주어지면서 테스트 통과를 못하는 몇개의 테스트가 있었는데 exit이 그 중 하나였습니다. thread에서 exit을 호출하는 경우를 전혀 생각하지 않아서 이를 처리하는 것이 까다로웠습니다. 그 이유는 thread_exit을 하고 나면 해당 스레드의 자원을 회수하는 스레드 혹은 프로세스가 항상 있다고 생각했는데 스레드가 그냥 exit을 호출하게 되면 자원회수를 하는 스레드가 없어서 문제가 발생했습니다. 그래서 exit함수에서 다른 스레드들의 자원을 회수하도록 별도로 처리했고 이는 exec에도 구현되어 있습니다.
    
4. sbrk test
    
    sbrk test에서 remap 문제가 지속적으로 발생했는데 처음에 생각한 문제로는 스레드들이 독립적으로 실행이 되니 sbrk를 실행할 때 lock을 걸고 실행하게 하려 했습니다. 하지만 sysproc에 존재하는 sys_sbrk함수에 ptable.lock을 사용하기가 어려웠고 코드를 봤을 때 문제될만한 지점이 growproc이라고 생각했습니다.
    
    하지만 growproc에 lock을 걸어도 여전히 동일한 문제가 발생해서 다른 문제를 해결하려 했습니다. 이 때 이전의 thread 관련 함수들에서 sz값을 동일하게 맞춰주었던 것이 기억나서 growproc에서 thread를 보유한 경우 모든 스레드들의 sz 값을 증가한 값으로 맞춰줘서 문제가 해결됐습니다.
    
5. 기타 test code
    
    test code가 주어지기 전에 test code를 직접 작성해봤는데 따로 테스트 하는 것이 까다로웠습니다. 다만 스레드가 thread_create, thread_exec, thread_exit 함수들이 정상적으로 작동하는 것은 확인했습니다.
    
    다만 기존의 exec을 할 때 명세대로 수정을 했었는데 test code를 돌렸을 때 hello thread가 호출된 뒤에 sh로 돌아오지 않는 문제가 발생했습니다. 이 때 실수했던 것이 스레드들을 종료하고 나서 자원회수를 하는 main thread가 같이 종료돼서 자원회수를 해주는 스레드가 사라져 이를 별도로 처리해줬습니다. 또한 이 때 사용한 함수를 exit함수에서도 그대로 사용했습니다.
