#include<stdio.h>
#include<sys.h>
void main1(){
    int ws=2;
    int i,j,k,pid,ppid;
    if(fork()){
        //2#
        sleep(2);
        for(k=1;k<6;k++){
            printf("%d,%d; ",k,getppid(k));
        }
        printf("\n");
    }
    else{
        //3#
        if(fork()){
            sleep(1);
            if(fork()){
                //3#
                pid=getpid();
                ppid=getppid(pid);

                // for(k=0;k<ws;k++){
                //     i=wait(&j);
                //     printf("Process %d#:My child %d is finished with exit status %d\n",pid,i,j);
                // }
                printf("Process %d# finished: My father is %d\n",pid,ppid);
                exit(ppid);
            }
            else{
                //5#
                pid=getpid();
                ppid=getppid(pid);
                printf("Process %d# finished: My father is %d\n",pid,ppid);
                exit(ppid);
            }
        }
        else{
            //4#
            pid=getpid();
            ppid=getppid(pid);
            printf("Process %d# finished: My father is %d\n",pid,ppid);
            exit(ppid);
        }
    }
}