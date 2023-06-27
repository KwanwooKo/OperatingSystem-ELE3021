#include "types.h"
#include "stat.h"
#include "user.h"
// & 이 코드는 project에서 schedlock, schedunlock 및 priority boosting 이 정상적으로 작동하는지 확인하는 코드 입니다.
// ^ 테스트 방법
// ^ 우선 10억개의 print를 준비하고, priorityBoosting이 작동하는지 확인
// ^ 350 전후로 priorityBoosting이 발생하기 때문에 300 이전에서 schedulerUnlock을 호출
int
main(int argc, char *argv[])
{
    printf(1, "schedlock_test 시작\n");
    // schedlock 호출
    // __asm__("int $129");
    schedulerLock(2020060100);


    for (int i = 0; i < 1000000000; i++) {
        // printf(1, "current i: %d\n", i);
        // 350 전후로 priority boosting 이 발생해서 schedlock이 해제됨
        if (i == 200) {
            // printf(1, "schedlock 해제\n");
            // __asm__("int $130");
            // schedulerUnlock(2020060100);
        }
    }

    printf(1, "schedlock_test 끝\n\n");
    exit();
}
