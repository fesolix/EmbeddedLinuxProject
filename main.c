#include <stdio.h>
#include <stdlib.h>

void read_cpu_temp() {
    FILE *file;
    char buffer[1024];
    char *temp_path = "/sys/class/thermal/thermal_zone0/temp";

    file = fopen(temp_path, "r");
    if (file == NULL) {
        printf("Error opening file\n");
        return;
    }

    if (fgets(buffer, sizeof(buffer), file) != NULL) {
        long temp = atol(buffer) / 1000;
        printf("CPU temperature: %ld C\n", temp);
    } else {
        printf("Error reading file\n");
    }
    fclose(file);
}

void read_cpu_frequency() {
    FILE *file;
    char buffer[1024];
    char *freq_path = "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq";

    file = fopen(freq_path, "r");
    if (file == NULL) {
        printf("Error opening file\n");
        return;
    }

    if (fgets(buffer, sizeof(buffer), file) != NULL) {
        float freq = atol(buffer) / 1000000.0f;
        printf("CPU frequency: %f GHz\n", freq);
    } else {
        printf("Error reading file\n");
    }
    fclose(file);
}

int main(void) {
    read_cpu_temp();
    read_cpu_frequency();
    return 0;
}