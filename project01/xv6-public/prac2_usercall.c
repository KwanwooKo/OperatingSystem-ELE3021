#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
    printf(1, "prac2_usercall 시작\n");
    // assembly code
    __asm__("int $128");
    // 이거 return 0가 아니라 exit() 이어야 하지 않나???
    return 0;
    // exit();
}