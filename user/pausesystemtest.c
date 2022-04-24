#include "kernel/types.h"
#include "kernel/param.h"
#include "user.h"

void test_pause_system(int interval, int pause_seconds, int loop_size) {
    int n_forks = 2;
    for (int i = 0; i < n_forks; i++) {
    	fork();
    }
    for (int i = 0; i < loop_size; i++) {
        if (i % interval == 0) {
            printf("pause system %d/%d completed.\n", i, loop_size);
        }
        if (i == (int)(loop_size / 2)){
            pause_system(pause_seconds);
        }
    }
    printf("\n");
}

int
main(int argc, char *argv[])
{
    test_pause_system(10,10, 100);
    exit(0);
    return 0;
}