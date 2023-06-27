#include "types.h"
#include "stat.h"
#include "user.h"

struct args {
    int arg0;
    int arg1;
};


/*

struct args *args = (struct args *) arg;
int result = args->arg0 + args->arg1;
printf(2, "result: %d\n", result);
thread_exit(&result);
return 0;

*/

void*
thread_function(void* arg)
{
    // int* value = (int*) arg;
    // int result = *value + 10;
    // printf(2, "result: %d\n", result);
    // thread_exit(&result);
    // return 0;

    struct args *args = (struct args *) arg;
    int result = args->arg0 + args->arg1;
    printf(2, "result: %d\n", result);
    thread_exit(&result);
    return 0;
}

int
main()
{
    thread_t thread;
    // int arg = 5;
    int retval;
    struct args args;
    args.arg0 = 1;
    args.arg1 = 2;
    if (thread_create(&thread, thread_function, &args) != 0) {
        printf(2, "[ERROR] Thread create\n");
        exit();
    }

    if (thread_join(thread, (void**) &retval) != 0) {
        printf(2, "[ERROR] Thread join\n");
        exit();
    }

    printf(2, "Thread returned value: %d\n", *(int*)retval);

    exit();
}
