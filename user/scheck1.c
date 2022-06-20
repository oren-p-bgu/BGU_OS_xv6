#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"

void sanityCheckFirstPart(void)
{
    char *direct = malloc(1024 * 12);
    char *single = malloc(256 * 1024);
    char *d_indirect = malloc(10 * 1024 * 1024);
    for (int i = 0; i < 1024 * 12; i++)
    {
        direct[i] = 'a';
    }
    for (int i = 0; i < 1024 * 268; i++)
    {
        single[i] = 'a';
    }
    for (int i = 0; i < 1024 * 1024; i++)
    {
        d_indirect[i] = 'a';
    }

    int fd = open("test", O_CREATE | O_RDWR);

    if (write(fd, direct, 1024 * 12) != 1024 * 12)
    {
        printf("error: write to backup file failed\n");
        exit(0);
    }
    else
    {
        printf("Finished writing 12KB (direct)\n");
    }

    if (write(fd, single, 1024 * 256) != 1024 * 256)
    {
        printf("error: write to backup file failed\n");
        exit(0);
    }
    else
    {
        printf("Finished writing 268KB (single indirect)\n");
    }

    if (write(fd, d_indirect, 10 * 1024 * 1024) != 10 * 1024 * 1024)
    {
        printf("error: write to backup file failed\n");
        exit(0);
    }
    else
    {
        printf("Finished writing 10MB (double indirect)\n");
    }

    close(fd);

    free(direct);
    free(single);
    free(d_indirect);
}