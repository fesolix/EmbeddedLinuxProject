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

// static const char *SENDER_ID = "0x91";

ssize_t read_pipe(int *fd, char *buf, size_t bufSize, const char *pipeName) {
    // Alles mit 0 füllen, um es später einfach als String verarbeiten zu können.
    memset(buf, 0, bufSize);

    // Blocking read
    ssize_t bytesRead = read(*fd, buf, bufSize - 1);
    if (bytesRead > 0) {
        // buf[bytesRead] = '\0'; // Schon durch memset garantiert
        return bytesRead;
    }
    if (bytesRead == 0) {
        // EOF -> Schreibseite geschlossen
        printf("Pipe '%s' closed by writer. Reopening...\n", pipeName);
        close(*fd);

        // Neu öffnen (blockierend)
        *fd = open(pipeName, O_RDONLY);
        if (*fd == -1) {
            perror("Reopen pipe");
        }
        // bytesRead = 0 bedeutet hier: wir haben keine Daten
        return 0;
    }

    // Fehlerfall (bytesRead < 0)
    perror("read pipe");
    return -1;
}

void build_package(char *package, size_t package_size, const char *payload, int value_id) {
    // Zwischenspeicher für die neue Komponente
    char tmp[128];
    // VALUE_ID=<id> VALUE=<payload>
    snprintf(tmp, sizeof(tmp), " VALUE_ID=%d VALUE=%s", value_id, payload);

    // Hänge tmp an package
    strncat(package, tmp, package_size - strlen(package) - 1);
}

void build_crc_checksum(char *package, size_t package_size) {
    // CRC berechnen
    uLong c = crc32(0L, Z_NULL, 0);
    c = crc32(c, (const Bytef*)package, strlen(package));

    char tmp[32];
    // z.B. " CRC=0x1A2B3C4D"
    snprintf(tmp, sizeof(tmp), " CRC=0x%08lX", (unsigned long)c);

    // ans package anhängen
    strncat(package, tmp, package_size - strlen(package) - 1);
}

void send_package(const char *package, const int fdCharDev) {
    char transmitBuf[256] = {0};

    // Kopiere Package in transmitBuf
    snprintf(transmitBuf, sizeof(transmitBuf), package);

    const ssize_t written = write(fdCharDev, transmitBuf, strlen(transmitBuf));
    if (written < 0) {
        perror("write /dev/packet_receiver");
    } else {
        printf("Wrote %ld bytes to %s\n", written, CHAR_DEV);
    }
}

// argc = Anzahl Pipes *argv[] Pipe Pfade
int main(const int argc, char *argv[]) {
    const int num_pipes = argc - 1;

    int *fds = calloc(num_pipes, sizeof(int));
    if (!fds) {
        perror("calloc fds");
        return 1;
    }

    for (int i = 0; i < num_pipes; i++) {
        const char *pipeName = argv[i + 1];
        // mkfifo() nur ausführen, wenn die Pipes noch nicht existieren,
        // bzw. Fehler ignorieren, falls schon existiert:
        if (mkfifo(pipeName, 0666) < 0 && errno != EEXIST) {
            perror("mkfifo pipe");
            free(fds);
            return 1;
        }

        // Pipes blockierend öffnen
        fds[i] = open(pipeName, O_RDONLY);
        if (fds[i] == -1) {
            perror("open pipe for reading");
            free(fds);
            return 1;
        }
    }

    // Open the char device for writing
    const int fdCharDev = open(CHAR_DEV, O_WRONLY);
    if (fdCharDev == -1) {
        perror("open /dev/packet_receiver");
        // Aufräumen
        for (int i = 0; i < num_pipes; i++) {
            close(fds[i]);
        }
        free(fds);
        return 1;
    }
    printf("Opened %s for writing.\n", CHAR_DEV);

    // Dauerhafte Lese-Schleife
    while (1) {
        char package[256];
        snprintf(package, sizeof(package), "SENDER=0x91");

        // Lese von jeder Pipe und ergänze das Package
        for (int i = 0; i < num_pipes; i++) {
            char pipeBuf[128];

            const ssize_t ret = read_pipe(&fds[i], pipeBuf, sizeof(pipeBuf), argv[i + 1]);
            if (ret > 0) {
                // ret Bytes gelesen in pipeBuf -> package anhängen
                build_package(package, sizeof(package), pipeBuf, i);
            } else if (ret == -1) {
                // Fehler beim Lesen
                fprintf(stderr, "Error reading from pipe\n");
                free(fds);
                return 1;
            }
        }

        // Jetzt an /dev/packet_receiver schicken
        if (strlen(package) > 0) {
            send_package(package, fdCharDev);
        }

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
