#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h> //for pipes
#include <fcntl.h>
#include <unistd.h>

int main(void) {
    // create and open pipes
    const char *pOne = "/tmp/pipeOne";
    mkfifo(pOne, 0666);
    int vOne = open(pOne, O_RDONLY | O_NONBLOCK);
    if (vOne == -1)
    {
        perror("Failed to open pipe one in main");
        return 1;
    }

    const char *pTwo = "/tmp/pipeTwo";
    mkfifo(pTwo, 0666);
    int vTwo = open(pTwo, O_RDONLY | O_NONBLOCK);
    if (vTwo == -1)
    {
        perror("Failed to open pipe two in main");
        return 1;
    }

    // read pipes
    char mOne[1024] = {}; // for now only sending char with the pipe
    read(vOne, mOne, sizeof(mOne));
    printf("value one received: %s\n", mOne);
    close(vOne);

    char mTwo[1024] = {};
    read(vTwo, mTwo, sizeof(mTwo));
    printf("value two received: %s\n", mTwo);
    close(vTwo);

    return 0;
}