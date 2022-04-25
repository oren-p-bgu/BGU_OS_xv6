#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/types.h"
void example_pause_system(int interval, int pause_seconds, int loop_size) {
    int n_forks = 2;
    for (int i = 0; i < n_forks; i++) {
        fork();
    }
    for (int i = 0; i < (int)(loop_size); i++) {
        if (i % interval == 0) {
            printf("pause system %d/%d completed.\n", i, loop_size);
        }
        if (i == (int)(loop_size / 2)){
            pause_system((int)(pause_seconds));
        }
    }
    printf("\n");
}
void pause_system_dem(int interval, int pause_seconds, int loop_size) {
    int pid = getpid();
    for (int i = 0; i < loop_size; i++) {
        if (i % interval == 0 && pid == getpid()) {
            printf("pause system %d/%d completed.\n", i, loop_size);
        }
        if (i == loop_size / 2) {
            pause_system(pause_seconds);
        }
    }
    printf("\n");
}

void kill_system_dem(int interval, int loop_size) {
    int pid = getpid();
    for (int i = 0; i < loop_size; i++) {
        if (i % interval == 0 && pid == getpid()) {
            printf("kill system %d/%d completed.\n", i, loop_size);
        }
        if (i == loop_size / 2) {
            kill_system();
        }
    }
    printf("\n");
}

/*
void set_economic_mode_dem(int interval, int loop_size) {
    int pid = getpid();
    set_economic_mode(1);
    for (int i = 0; i < loop_size; i++) {
        if (i % interval == 0 && pid == getpid()) {
            printf("set economic mode %d/%d completed.\n", i, loop_size);
        }
        if (i == loop_size / 2) {
            set_economic_mode(0);
        }
    }
    printf("\n");
}*/

int
main(int argc, char *argv[])
{
    //set_economic_mode_dem(10, 100);
    print_stats();
    example_pause_system(10, 10, 100);
    kill_system_dem(10, 100);
    exit(0);
}