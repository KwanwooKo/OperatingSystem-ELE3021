#include "types.h"
#include "defs.h"

// 1주차: 이거 자체가 system call
int
myfunction(char* str) 
{
    cprintf("%s\n", str);
    return 0xABCDABCD;
}

// 1주차: 이게 wrapper function
// 1주차: system call 앞에 sys_ 를 붙이면 된다
int
sys_myfunction(void) 
{
    char* str;
    // 1주차: 일단 이 함수가 뭔지 모르는게 맞음 => 다음 시간에 trap frame? 배우면 알 수 있음
    if (argstr(0, &str) < 0)
        return -1;
    return myfunction(str);
}