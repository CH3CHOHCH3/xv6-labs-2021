#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int idx;
int cnt;
int p[11][2];
int numbers[34];

int main(){ 
    for(int i = 2;i <= 35;++i){
        numbers[cnt++]=i;
    }
    // 是否已经创建子进程；
    int have_child = 0;
    while(1){
        if(!have_child){
            printf("prime %d\n", numbers[0]);
            if(cnt <= 1){
                // 没有数字要传给下一个进程；
                exit(0);
            }
            pipe(p[idx]);
            if(fork() == 0){
                // 子进程从父进程读入数字；
                close(p[idx][1]);
                cnt = 0;
                while(read(p[idx][0], numbers + cnt, sizeof(int))){
                    cnt++;
                }
                close(p[idx][0]);
                idx++;
            }else{
                // 父进程向子进程写入数字;
                close(p[idx][0]);
                for(int i = 1;i < cnt;++i){
                    if(numbers[i] % numbers[0]){
                        write(p[idx][1], numbers + i, sizeof(int));
                    }
                }
                have_child = 1;
                close(p[idx][1]);
            }
        }else{
            wait(0);
            exit(0);
        }
    }
}
