#include "types.h"
#include "user.h"

int main(void) {
  int pid = fork();

  if (pid == 0) {
    // 자식 프로세스는 5번 반복하며 yield를 호출합니다.
    int i;
    for (i = 0; i < 5; i++) {
      printf(1, "Child process yielding\n");
      yield();
    }
    printf(1, "Child process done\n");
    exit();
  } else if (pid > 0) {
    int i;
    for (i = 0; i < 5; i++) {
        printf(1, "Parent process yielding\n");
        yield();
    }
    // 부모 프로세스는 자식 프로세스가 끝나기를 기다립니다.
    wait();
    printf(1, "Parent process done\n");
    exit();
  } else {
    printf(1, "Fork error\n");
    exit();
  }
}
