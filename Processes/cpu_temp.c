#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h> //for pipes
#include <fcntl.h>

void read_cpu_temp()
{
    FILE *file;
    char buffer[1024];
    char *temp_path = "/sys/class/thermal/thermal_zone0/temp";

    file = fopen(temp_path, "r");
    if (file == NULL)
    {
        printf("Error opening file\n");
        return;
    }

    if (fgets(buffer, sizeof(buffer), file) != NULL)
    {
        long temp = atol(buffer) / 1000;
        char message[1024];
        snprintf(message, sizeof(message), "%ld", temp);
        printf("CPU temperature: %ld C\n", temp); // todo: print message instead of temp?

        // write pipe
        const char *pOne = "/tmp/pipeOne";
        int vOne = open(pOne, O_WRONLY);
        if (vOne == -1)
        {
            perror("Failed to open pipe one in read_cpu_temp");
            return;
        }
        write(vOne, message, sizeof(message));
        close(vOne);
    }
    else
    {
        printf("Error reading file\n");
    }
    fclose(file);
}

int main(void) {
    while (1) {
      read_cpu_temp();
    }
    return 0;
}
