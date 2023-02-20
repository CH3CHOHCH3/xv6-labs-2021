#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]){
    if(argc != 2){
        fprintf(2, "error:sleep needs an argument as time.\n");
        exit(1);
    }

    int time = atoi(argv[1]);
    printf("(nothing happens for a little while)\n");
    sleep(time);
    exit(0);
}
