#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#define MAX_PROCESSES 10 
#define PIPE_DIR "/tmp/"
#define PROCESS_MONITORING_DIR "/Processes/process_monitoring.sh" //bitte überprüfen ob das die richtige DIR ist

typedef struct {
    char name[256];
    char pipe_path[256];
    int fd;
} ProcessPipe;

ProcessPipe processes[MAX_PROCESSES]; //bis auf 10 prozesse skalierbar
int process_count = 0;

void scan_processes() {
    FILE *file = fopen(PROCESS_MONITORING_DIR, "r");//file öffnen
    if (!file) {
        perror("Could not open process file");
        exit(EXIT_FAILURE); //break; oder return 1; ?
    }

    char line[512];
    while (fgets(line, sizeof(line), file) && process_count < MAX_PROCESSES) {
        if (strncmp(line, "/usr/local/bin/", 15) == 0) {//zeilenweise nach prozessen suchen
            char *newline = strchr(line, '\n');
            if (newline) {
                *newline = '\0';
            }
            //prozess in dateistruktur schreiben
            snprintf(processes[process_count].name, sizeof(processes[process_count].name), "%s", strrchr(line, '/') + 1);
            snprintf(processes[process_count].pipe_path, sizeof(processes[process_count].pipe_path), "%s%s", PIPE_DIR, processes[process_count].name);
            // mkfifo() nur ausführen, wenn die Pipes noch nicht existieren,
            // bzw. Fehler ignorieren, falls schon existiert:
            if (mkfifo(processes[process_count].pipe_path, 0666) < 0 && errno != EEXIST) {//mkfifo machen
                perror("mkfifo failed");
                exit(EXIT_FAILURE);
            }
            process_count++;
        }
    }
    fclose(file);
}

void open_pipes() {//pipe blockierend öffnen
    for (int i = 0; i < process_count; i++) {
        processes[i].fd = open(processes[i].pipe_path, O_RDONLY);
        if (processes[i].fd == -1) {
            perror("Error opening pipe");
            exit(EXIT_FAILURE);
        }
    }
}

void read_pipes() {
    while (1) {//alle pipes in dauerschlife nacheinander lesen
        for (int i = 0; i < process_count; i++) {
            char buffer[128] = {0};
            // read() kann blockieren, wenn noch keine Daten da sind,
            // da wir blockierendes I/O verwenden.
            // Wenn die Schreibseite offen bleibt, kommen periodisch neue Daten.
            ssize_t bytes_read = read(processes[i].fd, buffer, sizeof(buffer));
            if (bytes_read > 0) {
                // Gültige Daten -> ausgeben
                printf("%s: %s\n", processes[i].name, buffer);
            } else if (bytes_read == 0) {
                // EOF -> Schreibseite hat geschlossen
                // -> ggf. Pipe erneut öffnen
                printf("%s closed, reopening...\n", processes[i].name);
                close(processes[i].fd);
                processes[i].fd = open(processes[i].pipe_path, O_RDONLY);
                if (processes[i].fd == -1) {
                    perror("Reopen pipe");
                    exit(EXIT_FAILURE);
                }
            } else {
                // bytesReadOne < 0 => Fehler
                if (errno == EINTR) {
                    // Signal unterbrochen, einfach weiter
                    continue;
                }
                perror("read error");
                exit(EXIT_FAILURE);
            }
        }
        // Kurze Pause, damit CPU-Last nicht durch
        // Dauerschleife hochgetrieben wird
        usleep(200000);//20ms
    }
    // Sollte man jemals aus der while(1)-Schleife ausbrechen:
    close_pipes();
}

void close_pipes() {//braucht man das?
    for (int i = 0; i < process_count; i++) {
        close(processes[i].fd);
    }
    return;
}

int main() {
    scan_processes();
    open_pipes();
    read_pipes();
    return 0;
}


//alter code:
/*

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

static const char *PIPE_ONE = "/tmp/pipeOne";
static const char *PIPE_TWO = "/tmp/pipeTwo";

int main(void)
{
    // mkfifo() nur ausführen, wenn die Pipes noch nicht existieren,
    // bzw. Fehler ignorieren, falls schon existiert:
    if (mkfifo(PIPE_ONE, 0666) < 0 && errno != EEXIST) {
        perror("mkfifo pipeOne");
        return 1;
    }
    if (mkfifo(PIPE_TWO, 0666) < 0 && errno != EEXIST) {
        perror("mkfifo pipeTwo");
        return 1;
    }

    // Beide Pipes blockierend öffnen
    int fdOne = open(PIPE_ONE, O_RDONLY);
    if (fdOne == -1) {
        perror("open pipeOne for reading");
        return 1;
    }

    int fdTwo = open(PIPE_TWO, O_RDONLY);
    if (fdTwo == -1) {
        perror("open pipeTwo for reading");
        close(fdOne);
        return 1;
    }

    // Dauerhafte Lese-Schleife
    while (1) {
        char bufOne[128] = {0};
        char bufTwo[128] = {0};

        // =========== Lesevorgang PipeOne ===========
        // read() kann blockieren, wenn noch keine Daten da sind,
        // da wir blockierendes I/O verwenden.
        // Wenn die Schreibseite offen bleibt, kommen periodisch neue Daten.
        ssize_t bytesReadOne = read(fdOne, bufOne, sizeof(bufOne));
        if (bytesReadOne > 0) {
            // Gültige Daten -> ausgeben
            printf("PipeOne CPU Temp: %s °C\n", bufOne);
        } else if (bytesReadOne == 0) {
            // EOF -> Schreibseite hat geschlossen
            // -> ggf. Pipe erneut öffnen
            printf("PipeOne closed by writer. Reopening...\n");
            close(fdOne);

            // Neu öffnen
            fdOne = open(PIPE_ONE, O_RDONLY);
            if (fdOne == -1) {
                perror("Reopen pipeOne");
                break;  // oder return 1;
            }
        } else {
            // bytesReadOne < 0 => Fehler
            if (errno == EINTR) {
                // Signal unterbrochen, einfach weiter
                continue;
            }
            perror("read pipeOne");
            break; // oder return 1;
        }

        // =========== Lesevorgang PipeTwo ===========
        ssize_t bytesReadTwo = read(fdTwo, bufTwo, sizeof(bufTwo));
        if (bytesReadTwo > 0) {
            printf("PipeTwo CPU Freq: %s GHz\n", bufTwo);
        } else if (bytesReadTwo == 0) {
            printf("PipeTwo closed by writer. Reopening...\n");
            close(fdTwo);
            fdTwo = open(PIPE_TWO, O_RDONLY);
            if (fdTwo == -1) {
                perror("Reopen pipeTwo");
                break;
            }
        } else {
            if (errno == EINTR) {
                continue;
            }
            perror("read pipeTwo");
            break;
        }

        // Kurze Pause, damit CPU-Last nicht durch
        // Dauerschleife hochgetrieben wird
        usleep(200000); // 200 ms
    }

    // Sollte man jemals aus der while(1)-Schleife ausbrechen:
    close(fdOne);
    close(fdTwo);

    return 0;
}
*/