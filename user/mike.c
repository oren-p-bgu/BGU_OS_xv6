// mike tests

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  for(int i = 0; i < 1; i++){
    int pid1 = fork();
    if(pid1 < 0){
      printf("%d: fork failed\n", i);
      exit(1);
    }
    if(pid1 == 0){
      sleep(10);
      while(1) {
        //printf("%d",getpid());
        getpid();
      }
      exit(0);
    }else{
      //sleep(1);
      if(kill(4) == -1){
        printf("Oh no..");
      }
    }
    /*kill(pid1);
    wait(&xst);
    if(xst != -1) {
       printf("%s: status should be -1\n", s);
       exit(1);
    }*/
  }
  exit(0);
}
