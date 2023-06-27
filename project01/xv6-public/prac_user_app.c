#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
    printf(1, "prac_user_app 시작\n");
    // __asm__("int $129");
    // schedulerLock(2020060101);
    // if (argc <= 1) {
    //     exit();
    // }
    // char *buf = "2020060100_12300_KwanwooKo";
    // char *buf = "2020060100";
    printf(1, "이 문장이랑 다음 문장이\n");
    yield();
    printf(1, "이거 연속적으로 실행되면 안돼\n");
    // 1주차: argv[1] 이 "user" 인 경우에만 밑에 코드 실행
    // if (strcmp(argv[1], buf) != 0) {
    //     exit();
    // }
    int i = 0;
    int count = 0;
    for (i = 0; i < 1000000000; i++) {
        count += 2;
    }
    
    // printf(1, "schedulerUnlock 호출 직전\n");
    // printf(1, "count: %d\n", count);
    // schedulerUnlock(2020060100);
    // printf(1, "schedulerUnlock 호출 함\n");
    
    // __asm__("int $129");

    // int ret_val;
    // 1주차: system call 호출
    // ret_val = myfunction(buf);
    // printf(1, "Return value : 0x%x\n", ret_val);
    // 1주차: return 0 가 아닌, exit() 으로 프로세스 종료
    printf(1, "prac_user_app 종료!!!!!!!!!!!!!!!!!\n\n");
    exit();
}
