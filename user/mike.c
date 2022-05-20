// mike tests

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  printf("mike: I know my cpu is %d\n",get_cpu());
  sleep(5);
  printf("mike: I will change it to 1\n",set_cpu(1));
  sleep(5);
  printf("mike: Now my cpu is %d\n",get_cpu());
  sleep(5);
  exit(0);
}
