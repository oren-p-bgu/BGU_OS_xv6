#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

int
main(int argc, char *argv[])
{
  int fd = open("mi",O_CREATE + O_WRONLY);
  write(fd,"hello",6);
  close(fd);
  printf("%d\n",symlink("/mi","/ma"));

  char buff[10];
  readlink("ma",&buff,10);
  printf("first char %c", buff[0]);
  printf("file contents: %s", buff);
  printf("CHECK");
  exit(0);
}
