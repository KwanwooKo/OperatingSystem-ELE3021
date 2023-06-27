#include "types.h"
#include "stat.h"
#include "user.h"

void
execute(char *path, int stacksize)
{
    if (fork() == 0) {
        exec2(path, (char*[]){path, 0}, stacksize);
        printf(2, "exec %s failed\n", path);
    }
}

void
memlim(int pid, int limit)
{
    // ^ setmemorylimit 함수 내부에서 오류 처리 다 해줌
    int success = setmemorylimit(pid, limit);
    if (success == 0) {
        if (limit == 0)
            printf(1, "[SUCCESS] set memory limit to INF\n");
        else
            printf(1, "[SUCCESS] set memory limit to %d\n", limit);
    }
}

void
killProc(int pid)
{
    int success = kill(pid);
    if (success == 0) {
        // printf(2, "[SUCCESS] kill process which pid is %d\n", pid);
        wait();
    }
    else {
    //   printf(2, "[ERROR] could not kill the process\n");
    }
}


char*
parsecommand(char *s, const char c)
{
    char* ret = (char *)malloc(sizeof(100));
    int i = 0;
    for (i = 0; i < strlen(s); i++) {
        if (s[i] == c)
            break;
        ret[i] = s[i];
    }

    ret[i] = '\0';
    return ret;
}

char*
getline(char *buf)
{
    gets(buf, 100);
    buf[strlen(buf) - 1] = '\0';
    return buf;
}



int 
main(int argc, char *argv[])
{
    char buf[100]; // 넉넉하게 100 정도로 잡음
    printf(2, "pmanager starts!\n");
    printf(2, "Enter the command\n\n");
    while (getline(buf) != 0) {
        char* cmd = parsecommand(buf, ' ');

        // list 출력
        if (strcmp(cmd, "list") == 0) {
            printProcList();
        }
        else if (strcmp(cmd, "kill") == 0) {
            char* findPlace = strchr(buf, ' ') + 1;
            char* cmd1 = parsecommand(findPlace, '\0');

            int pid = atoi(cmd1);

            killProc(pid);
            free(cmd1);
        }
        else if (strcmp(cmd, "execute") == 0) {
            char* findPlace = strchr(buf, ' ') + 1;
            char* cmd1 = parsecommand(findPlace, ' ');

            findPlace = strchr(findPlace, ' ') + 1;
            char* cmd2 = parsecommand(findPlace, '\0');

            int stacksize = atoi(cmd2);

            execute(cmd1, stacksize);

            free(cmd1);
            free(cmd2);
        }
        else if (strcmp(cmd, "memlim") == 0) {
            char* findPlace = strchr(buf, ' ') + 1;
            char* cmd1 = parsecommand(findPlace, '\0');

            findPlace = strchr(findPlace, ' ') + 1;
            char* cmd2 = parsecommand(findPlace, '\0');

            int pid = atoi(cmd1);
            int limit = atoi(cmd2);

            memlim(pid, limit);
        }
        else if (strcmp(cmd, "exit") == 0) {
            free(cmd);
            // ! 자식 프로세스 종료되는 거 기다림
            while (wait() != -1);
            exit();
        }
        else {
            printf(1, "There is no command %s\n", cmd);
        }
        free(cmd);
    }
}
