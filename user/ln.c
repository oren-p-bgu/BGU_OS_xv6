#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
int
main(int argc, char *argv[])
{
  if(argc != 3 && argc != 4){
    fprintf(2, "Usage: ln -s old new\n");
    fprintf(2, "or\n");
    fprintf(2, "Usage: ln old new\n");
    fprintf(2, "or%d\n" , argc);
    exit(1);
  }
  fprintf(2, "%s\n" , argv[1]);
  if(argc == 3){
    if(link(argv[1], argv[2]) < 0)
      fprintf(2, "link %s %s: failed\n", argv[1], argv[2]);
  }else if (strcmp(argv[1] ,"-s") == 0){
    if(symlink(argv[2], argv[3]) < 0)
      fprintf(2, "symlink %s %s: failed\n", argv[2], argv[3]);
  }
  exit(0);
}
