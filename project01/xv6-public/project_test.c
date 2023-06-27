#include "types.h"
#include "stat.h"
#include "user.h"
// & 이 코드는 project에서 MLFQ가 정상적으로 작동하는지 확인하는 코드 입니다.
// ^ queue level이 정확하게 지켜지고 있는지 확인
// ^ schedulerUnlock 함수가 queue level, pid, time quantum을 출력하는 걸 이용해서
// ^ 해당 프로세스를 빠르게 종료하는 걸 통해 MLFQ가 정확하게 작동하는지 확인해봄
int
main(int argc, char *argv[])
{
    printf(1, "MLFQ_test 시작\n");

    // while(1);

    int count = 0;
    for (int i = 0; i < 100000000; i++) {
        count++;
        // printf(1, "current i: %d\n", i);
        // 350 전후로 priority boosting 이 발생해서 schedlock이 해제됨
    }

    printf(1, "MLFQ_test 끝\n\n");
    exit();
}
