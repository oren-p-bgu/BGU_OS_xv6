#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/types.h"

int 
main(int argc, char *argv[])
{
	pause_system(4);
	
	exit(0);
}
