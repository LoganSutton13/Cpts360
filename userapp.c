#include "userapp.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
// Additional headers as needed


// Register process with kernel module
void register_process(unsigned int pid)
{
    char cmd[256];
    sprintf(cmd, "echo \"%d\" > /proc/kmlab/status", pid);
    system(cmd);
}

int main(int argc, char* argv[])
{
    int __expire = 10;
    time_t start_time = time(NULL);

    if (argc == 2) {
        __expire = atoi(argv[1]);
    }

    register_process(getpid());

    // Terminate user application if the time has expired
    while (1) {
        if ((int)(time(NULL) - start_time) > __expire) {
            break;
        }
    }

	return 0;
}
