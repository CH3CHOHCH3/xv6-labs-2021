#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]){
    // 父进程->子进程
    int p1[2];
    // 子进程->父进程
    int p2[2];

    pipe(p1);
    pipe(p2);

    char t1, t2;
    // 即时关闭管道的无用读写端，是为了
    // 在管道的发送/接收端进程退出后能
    // 正常反映管道的状况，以便正常产生
    // 错误信号。
    if (fork()) {
        // 父进程关闭p1读端，p2写端
        close(p1[0]);
        close(p2[1]);
        write(p1[1], &t1, 1);
        read(p2[0], &t2, 1);
        printf("%d: received pong\n", getpid());
    } else {
        // 子进程关闭p1写端，p2读端
        close(p1[1]);
        close(p2[0]);
        read(p1[0], &t2, 1);
        printf("%d: received ping\n", getpid());
        write(p2[1], &t1, 1);
    }

    exit(0);
}