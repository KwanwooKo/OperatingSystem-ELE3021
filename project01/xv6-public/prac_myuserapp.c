#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
    printf(1, "prac_myuserapp 시작\n");
    // if (argc <= 1) {
    //     exit();
    // }

    // // 1주차: argv[1] 이 "user" 인 경우에만 밑에 코드 실행
    // if (strcmp(argv[1], "\"user\"") != 0) {
    //     exit();
    // }
    printf(1, "일단 yield 호출함\n");
    yield();
    // char *buf = "Hello xv6!";
    // int ret_val;
    // 1주차: system call 호출
    // ret_val = myfunction(buf);
    // printf(1, "Return value : 0x%x\n", ret_val);
    // 1주차: return 0 가 아닌, exit() 으로 프로세스 종료

    printf(1, "prac_myuserapp 종료!!!!!!!!!!!!!!!!!\n\n");
    exit();
}
