// CAS Task 5

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
castest(void){
    fork();
    fork();
    fork();

    printf("-%d-\n",getpid() );
}

int
main(void)
{
    castest();
    exit(0);
    return 0;
}