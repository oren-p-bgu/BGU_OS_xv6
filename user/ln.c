#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 3 && argc !=4 ){
    fprintf(2, "Usage: ln old new\n");
    fprintf(2, "or Usage: ln -s old new\n");
    exit(1);
  }
  if(strcmp(argv[1],"-s") == 0){
    if(symlink(argv[2], argv[3]) < 0)
      fprintf(2, "SYMlink %s %s: failed\n", argv[2], argv[3]);
  }else{
    if(link(argv[1], argv[2]) < 0)
      fprintf(2, "link %s %s: failed\n", argv[1], argv[2]);
  }
  exit(0);
}
