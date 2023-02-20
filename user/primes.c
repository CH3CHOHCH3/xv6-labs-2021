#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int prime;
int p[2][2];

int main(int argc, char *argv[]){
start:
    // 保存读父进程的管道
    p[0][0] = p[1][0];
    // 创建写子进程的管道
    pipe(p[1]);

    if (fork()) {
        close(p[1][0]);

        if (!p[0][0]) {
            // 第一个进程不从父进程读数字，直接把 2-35 写给子进程
            for(int i = 2; i <= 35; ++i){
                write(p[1][1], &i, sizeof(int));
            }
        } else {
            // 后续进程从父进程读到数字，写给子进程
            int temp;
            while(read(p[0][0], &temp, sizeof(int))){
                if(temp%prime){
                    write(p[1][1], &temp, sizeof(int));
                }
            }
            close(p[0][0]);
        }
        close(p[1][1]);
        wait(0);
        exit(0);
    } else {
        close(p[1][1]);
        if (read(p[1][0], &prime, sizeof(int))) {
            printf("prime %d\n", prime);
            // 从父进程读到数字后，才创建子进程
            goto start;
        } else {
            // 否则直接退出
            close(p[1][0]);
            exit(0);
        }
    }
}