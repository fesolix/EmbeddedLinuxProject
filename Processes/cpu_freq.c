#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

static const char *PIPE_TWO = "/tmp/cpu_freq";

static void read_cpu_freq_and_write(int fd)
{
    FILE *file;
    char buffer[1024];
    char *freq_path = "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq";

    file = fopen(freq_path, "r");
    if (!file) {
        perror("Failed to open CPU freq file");
        return;
    }

    if (fgets(buffer, sizeof(buffer), file) != NULL) {
        // CPU frequency in kHz -> in GHz umrechnen
        float freq = atol(buffer) / 1000000.0f;
        char message[128];
        snprintf(message, sizeof(message), "%.3f", freq);

        ssize_t written = write(fd, message, strlen(message));
        if (written < 0) {
            perror("write to pipeTwo");
        } else {
            printf("Wrote CPU freq: %s GHz\n", message);
        }
    } else {
        fprintf(stderr, "Error reading CPU freq\n");
    }

    fclose(file);
}

int main(void)
{
    // Named Pipe einmal öffnen
    mkfifo(PIPE_TWO, 0666);
    int pipeFd = open(PIPE_TWO, O_WRONLY);
    if (pipeFd == -1) {
        perror("Failed to open pipeTwo for writing");
        return 1;
    }

    while (1) {
        read_cpu_freq_and_write(pipeFd);
        sleep(1);
    }

    close(pipeFd);
    return 0;
}
