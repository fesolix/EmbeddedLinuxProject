# Embedded Linux Projekt

# Dieses Program ist mit beliebig viele Prozessen verbunden, welche dessen Daten über die GPIO sendet

## Architektur
- Als Beispielprozesse werden die CPU Frequenz und Temperatur gemessen
- Die gemessenen daten werden über eine FIFO Pipe an einen Monitoring System (main.c im User-Space) gesendet.
- FIFO wird benutzt, weil man das Write- und Read End frei wählen kann und man mit mkfifo Pipes in einem Verzeichnis erstellen kann. Das erleichtert die Skalierbarkeit.
- Die Empfangene Daten werden vom Monitoring System in ein Package geschrieben.
- Mit der Vorgegebenen Struktur: SenderID, ValueID 1, Value 1, ValueID 2, ...
- Das Paket wird über einer CRC Checksum abgesichert und anschließend mit einem Character device interface an das Kernel gesendet.
- Das Kernel ist für das Erstellen des Character Devices zuständig
- Der Kernel Validiert das Paket mit der CRC.
- Der Kernel handlet das Timeslot management und Schreibt die daten an die GPIO
