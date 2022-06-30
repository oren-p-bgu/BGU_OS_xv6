#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"



int
main(int argc, char *argv[])
{
  symlink("/ls","/how");
  //exec("/how",argv);
  exit(0);
}
