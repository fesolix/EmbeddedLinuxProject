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

int main(void) {
    read_cpu_temp();
    return 0;
}