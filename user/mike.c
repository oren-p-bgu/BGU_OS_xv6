// mike tests

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  printf("I know my cpu is %d",get_cpu());
  printf("I will change it to 1",set_cpu(1));
  printf("Now my cpu is %d",get_cpu());
  exit(0);
}
