#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
    if (argc < 1) {
        printf(1, "add user student id!\n");
        exit();
    }

    char *buf = "2020060100";
    if (strcmp(argv[1], buf) != 0) {
        
    }
}