#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h> //for pipes
#include <fcntl.h>

void read_cpu_frequency()
{
    FILE *file;
    char buffer[1024];
    char *freq_path = "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq";

    file = fopen(freq_path, "r");
    if (file == NULL)
    {
        printf("Error opening file\n");
        return;
    }

    if (fgets(buffer, sizeof(buffer), file) != NULL)
    {
        float freq = atol(buffer) / 1000000.0f;
        char message[1024];
        snprintf(message, sizeof(message), "%f", freq);
        printf("CPU frequency: %f GHz\n", freq);

        //write pipe
        const char *pTwo = "/tmp/pipeTwo";
        int vTwo = open(pTwo, O_WRONLY);
        if (vTwo == -1)
        {
            perror("Failed to open pipe two in read_cpu_frequency");
            return;
        }
        write(vTwo, message, sizeof(message));
        close(vTwo);
    }
    else
    {
        printf("Error reading file\n");
    }
    fclose(file);
}

int main(void) {
    while (1) {
      read_cpu_frequency();
    }
    return 0;
}
