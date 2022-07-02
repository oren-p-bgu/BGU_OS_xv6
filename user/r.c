#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int
main(int argc, char *argv[])
{
  int fd2 = open("mi",O_RDONLY);
  char buff[6];
  read(fd2,&buff,6);
  close(fd2);
  printf("file contents: %s", buff);
  exit(0);
}
