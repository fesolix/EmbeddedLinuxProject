#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
    while (1) {
        FILE *fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
        if (!fp) {
            perror("Fehler beim Öffnen der Frequenz-Datei");
            return 1;
        }
        
        int freq_kHz;
        fscanf(fp, "%d", &freq_kHz);
        fclose(fp);
        
        double freq_MHz = freq_kHz / 1000.0;
        printf("CPU-Frequenz: %.2f MHz\n", freq_MHz);
    }

    return 0;
}
