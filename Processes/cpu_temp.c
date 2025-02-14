/*
@file cpu_temp.c
@brief cpu temperatur lesen und an pipe senden
@author Felix Moser, Christoph Schwierz
@date 2025-02-01

@details
dieses programm liest die temperatur aus thermal_zone0 der CPu aus
und sendet den wert an seine zugehörige pipe
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

static const char *PIPE_ONE = "/tmp/cpu_temp";

static void read_cpu_temp_and_write(int fd)
{
    FILE *file;
    char buffer[1024];
    char *temp_path = "/sys/class/thermal/thermal_zone0/temp";

    file = fopen(temp_path, "r");
    if (!file) {
        perror("Failed to open CPU temp file");
        return;
    }

    if (fgets(buffer, sizeof(buffer), file) != NULL) {
        long temp = atol(buffer) / 1000;
        // z.B. "42\n" => 42° C
        char message[128];
        snprintf(message, sizeof(message), "%ld", temp);

        // Schreiben, aber NICHT den FD schließen.
        // Nur so viele Bytes schreiben, wie tatsächlich gebraucht werden.
        ssize_t written = write(fd, message, strlen(message));
        if (written < 0) {
            perror("write to pipeOne");
        } else {
            printf("Wrote CPU temp: %s C\n", message);
        }
    } else {
        fprintf(stderr, "Error reading CPU temp\n");
    }

    fclose(file);
}

int main(void)
{
    // Named Pipe einmal öffnen (Blockierend)
    // Der mkfifo-Aufruf sollte idealerweise vorher im Setup passieren
    // Falls sie nicht existiert, wird sie erzeugt
    mkfifo(PIPE_ONE, 0666);
    int pipeFd = open(PIPE_ONE, O_WRONLY);
    if (pipeFd == -1) {
        perror("Failed to open pipeOne for writing");
        return 1;
    }

    while (1) {
        read_cpu_temp_and_write(pipeFd);
        sleep(1);
    }

    // Nie erreicht im Beispiel, weil while(1)
    close(pipeFd);
    return 0;
}
