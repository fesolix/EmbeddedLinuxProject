#file process_monitoring.sh
#brief liste aller Prozesse die in ihre pipe writen
#author Felix Moser, Alan Muhemad
#date 2025-02-01

#!/bin/bash

#Prozesse starten
/usr/local/bin/cpu_temp &
/usr/local/bin/cpu_freq &

sleep 200

# Gleiche Argumente in die main übergeben
/usr/local/bin/main "$@"

# Damit das Skript nicht sofort beendet, bleibst du in einer Endlosschleife
while true
do
    sleep 3600
done
