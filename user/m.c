#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int
main(int argc, char *argv[])
{
  //Create real file
  /*int fd = open("mi",O_CREATE + O_WRONLY);
  write(fd,"hello from mike",16);
  close(fd);*/
  //link a fake file
  char lsbuff[26064];
  int fd = open("ls",O_RDONLY);
  read(fd,&lsbuff,26064);
  close(fd);
  int fd2 = open("lss",O_CREATE + O_WRONLY);
  write(fd2,&lsbuff,26064);
  close(fd2);
  /*mkdir("kk");
  symlink("/kk","/ff");
  char buff[20];
  readlink("ff",&buff,20);
  printf("SYM Contents : %s\n",buff);
  chdir("/ff");
  int fd2 = open("ls",O_CREATE + O_WRONLY);
  write(fd2,&buff,26064);
  close(fd2);*/
  //chdir("..");
  //read out file
  /*char buff[20];
  readlink("ma",&buff,20);
  //printf(": %d :\n",buff);
  printf("SYM Contents : %s\n",buff);

  symlink("/ma","/mo");
  int fd2 = open("mo",O_RDONLY);
  read(fd2,&buff,20);
  close(fd2);
  printf("FROM sym: %s\n", buff);*/

  exit(0);
}
