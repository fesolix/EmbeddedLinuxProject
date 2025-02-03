#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h> //for pipes
#include <fcntl.h>
#include <unistd.h>

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

int main(void)
{
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

    read_cpu_temp();
    read_cpu_frequency();

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