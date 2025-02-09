#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <zlib.h>

// static const char *PIPE_ONE = "/tmp/pipeOne";
// static const char *PIPE_TWO = "/tmp/pipeTwo";

static const char *CHAR_DEV = "/dev/packet_receiver";

static const char *SENDER_ID = "0x91";

char* build_crc_checksum(const char *payload) {
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, payload, strlen(payload));

    char* crc_checksum = ("0x%08lX", crc);

    return crc_checksum;
}

char* build_package(char const *payload) {
    char* crc_checksum = build_crc_checksum(payload);

    return "SENDER=%s %s %s", SENDER_ID, payload, crc_checksum;
}

void send_package(const char *package, const int fdCharDev) {
    char transmitBuf[256] = {0};
    snprintf(transmitBuf, sizeof(transmitBuf), package);

    const ssize_t written = write(fdCharDev, transmitBuf, strlen(transmitBuf));
    if (written < 0) {
        perror("write /dev/packet_receiver");
    } else {
        printf("Wrote %ld bytes to %s\n", written, CHAR_DEV);
    }
}

char* read_pipe(int fd, char const *pipeName, char *payload) {
    char buf[128] = {0};
    // =========== Lesevorgang Pipe ===========
    // read() kann blockieren, wenn noch keine Daten da sind,
    // da wir blockierendes I/O verwenden.
    // Wenn die Schreibseite offen bleibt, kommen periodisch neue Daten.
    const ssize_t bytesReadOne = read(fd, buf, sizeof(buf));
    if (bytesReadOne > 0) {
        // Gültige Daten -> ausgeben
        printf("Value: %s\n", buf);
    } else if (bytesReadOne == 0) {
        // EOF -> Schreibseite hat geschlossen
        // -> ggf. Pipe erneut öffnen
        printf("Pipe closed by writer. Reopening...\n");
        close(fd);

        // Neu öffnen
        fd = open(pipeName, O_RDONLY);
        if (fd == -1) {
            perror("Reopen pipe");
        }
    } else {
        // bytesReadOne < 0 => Fehler
        if (errno == EINTR) {
            // Signal unterbrochen, einfach weiter
            return;
        }
        perror("read pipe");
    }
}

// argc = Anzahl Pipes *argv[] Pipe Pfade
int main(const int argc, char *argv[]) {
    int fds[argc];

    for (int i = 0; i < argc; i++) {
        // mkfifo() nur ausführen, wenn die Pipes noch nicht existieren,
        // bzw. Fehler ignorieren, falls schon existiert:
        if (mkfifo(argv[i], 0666) < 0 && errno != EEXIST) {
            perror("mkfifo pipe");
            return 1;
        }

        // Pipes blockierend öffnen
        fds[i] = open(argv[i], O_RDONLY);
        if (fds[i] == -1) {
            perror("open pipeOne for reading");
            return 1;
        }
    }

    // Open the char device for writing
    const int fdCharDev = open(CHAR_DEV, O_WRONLY);
    if (fdCharDev == -1) {
        perror("open /dev/packet_receiver");
        for (int i = 0; i < argc; i++) {
            close(fds[i]);
        }
        return 1;
    }
    printf("Opened %s for writing.\n", CHAR_DEV);

    // Dauerhafte Lese-Schleife
    while (1) {
        // =========== Lesevorgang Pipes ===========
        for (int i = 0; i < argc; i++) {
            read_pipe(fds[i], argv[i]);
        }

        // =========== Zum cdev schreiben ===========
        send_package(package, fdCharDev);

        // Kurze Pause, damit CPU-Last nicht durch
        // Dauerschleife hochgetrieben wird
        usleep(200000); // 200 ms
    }
    // Sollte man jemals aus der while(1)-Schleife ausbrechen:
    for (int i = 0; i < argc; i++) {
        close(fds[i]);
    }

    return 0;
}
