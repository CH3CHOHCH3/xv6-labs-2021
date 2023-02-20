#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc, char *argv[]){
    char buf[512];
    char *p = buf;

    while(read(0, p, 1)){
        if(*p == '\n'){
            char *new_argv[MAXARG];
            for(int i = 0;i < argc - 1;++i){
                new_argv[i] = argv[i + 1];
            }

            int idx = argc - 1;
            for(char *i = buf, *j = buf;i <= p;++i){
                if(*i == ' ' || *i == '\n'){
                    char *arg = (char *)malloc(sizeof(buf));
                    memcpy(arg, j, i - j);
                    new_argv[idx++] = arg;
                    j = i + 1;
                }
            }
            if(fork()==0){
                exec(new_argv[0], new_argv);
            }
            else{
                wait(0);
            }
            p = buf;
        }
        else{
            p++;
        }
    }
    exit(0);
}