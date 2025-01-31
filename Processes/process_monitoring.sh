#!/bin/bash

/usr/local/bin/cpu_temp &
/usr/local/bin/cpu_freq &

while true
do
    sleep 3600
done
