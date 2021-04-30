#include "libuio.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>

volatile sig_atomic_t done = 0;

void sighandler(int sig)
{
    done = 1;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Invocation: ./uio.out <UIO Device Number>\n\n");
        return 0;
    }
    signal(SIGINT, &sighandler);
    int uio_id = atoi(argv[1]);
    printf("UIO ID: %d\n", uio_id);
    uio_dev dev[1];
    if (uio_init(dev, uio_id) < 0)
    {
        printf("Error initialization\n");
        return 0;
    }
    while(!done)
    {
        printf("[R]ead/[W]rite: ");
        char c = '\0';
        scanf(" %c", &c);
        printf("Enter address (hex): ");
        unsigned int offset = 0x0;
        scanf("%x", &offset);
        unsigned data = 0;
        if (c == 'r' || c == 'R')
        {
            uio_read(dev, offset, &data);
            printf("0x%x -> 0x%x\n", offset, data);
        }
        else if (c == 'w' || c == 'W')
        {
            printf("Enter data (hex): ");
            scanf("%x", &data);
            uio_write(dev, offset, data);
        }
    }
    uio_destroy(dev);
    return 0;
}