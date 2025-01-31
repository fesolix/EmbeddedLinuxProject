#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
    while (1) {
        FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
        if (!fp) {
            perror("Fehler beim Öffnen der Temperatur-Datei");
            return 1;
        }
        
        int temp_mC;
        fscanf(fp, "%d", &temp_mC);
        fclose(fp);
        
        double temp_C = temp_mC / 1000.0;
        printf("CPU-Temperatur: %.2f°C\n", temp_C);
    }

    return 0;
}
