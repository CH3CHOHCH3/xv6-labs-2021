#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    // p -> ch
    int p1[2];
    // ch -> p
    int p2[2];

    pipe(p1);
    pipe(p2);

    char send_msg[] = "X";
    char recv_msg[1]; 

    if(fork() == 0){
        read(p1[0], recv_msg, 1);
        printf("%d: received ping\n", getpid());
        write(p2[1], send_msg, 1);
    }else{
        write(p1[1], send_msg, 1);
        read(p2[0], recv_msg, 1);
        printf("%d: received pong\n", getpid());
    }
    exit(0);
}
