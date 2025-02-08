#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

static const char *PIPE_ONE = "/tmp/pipeOne";
static const char *PIPE_TWO = "/tmp/pipeTwo";

static const char *CHAR_DEV = "/dev/packet_receiver";

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

    // Open the char device for writing
    int fdCharDev = open(CHAR_DEV, O_WRONLY);
    if (fdCharDev == -1) {
        perror("open /dev/packet_receiver");
        close(fdOne);
        close(fdTwo);
        return 1;
    }
    printf("Opened %s for writing.\n", CHAR_DEV);
    
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

        // ========= Write to char device =========
        // Build a packet string. Adapt SENDER / IDs / values to your needs.
        // Example: "SENDER=1 VID1=10 VAL1=<temp> VID2=11 VAL2=<freq>"
        char transmitBuf[256] = {0};
        snprintf(transmitBuf, sizeof(transmitBuf),
                 "SENDER=1 VID1=10 VAL1=%s VID2=11 VAL2=%s",
                 bufOne, bufTwo);

        ssize_t written = write(fdCharDev, transmitBuf, strlen(transmitBuf));
        if (written < 0) {
            perror("write /dev/packet_receiver");
        } else {
            printf("Wrote packet to %s: %s\n", CHAR_DEV, transmitBuf);
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
