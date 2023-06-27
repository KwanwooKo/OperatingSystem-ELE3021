#include "types.h"
#include "stat.h"
#include "user.h"
int a = 0;
void *
thread_func1(void *arg)
{
    printf(2, "Thread 1 is running\n");
    // while(1);
    printf(2, "Thread 1 zzz....\n");
    sleep(50000);
    printf(2, "Thread 1 wake up\n");
    a++;
    thread_exit(0);
    printf(2, "Thread exited\n");
    return 0;
}

void *
thread_func2(void *arg)
{
    printf(2, "Thread 2 is running\n");
    // while (1);
    a++;
    thread_exit(0);
    return 0;
}

int
main(int argc, char *argv[])
{
    thread_t tid1, tid2;
    int rc;
    
    rc = thread_create(&tid1, thread_func1, 0);
    // printf(2, "Thread 1 created\n");
    if (rc != 0) {
        printf(2, "failed to create thread1\n");
        exit();
    }

    rc = thread_create(&tid2, thread_func2, 0);
    // printf(2, "Thread 2 created\n");
    if (rc != 0) {
        printf(2, "failed to create thread2\n");
        exit();
    }
    rc = thread_join(tid1, 0);
    // printf(2, "Thread 1 join\n");
    if (rc != 0) {
        printf(2, "failed to join thread1\n");
        exit();
    }

    rc = thread_join(tid2, 0);
    // printf(2, "Thread 2 join\n");
    if (rc != 0) {
        printf(2, "failed to join thread2\n");
        exit();
    }

    printf(2, "All threads completed\n");


    exit();
}