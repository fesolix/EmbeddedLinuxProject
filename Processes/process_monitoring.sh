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
