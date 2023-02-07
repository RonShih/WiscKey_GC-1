#!/bin/bash

Program="Beyond"
FILE="./FileServer.trace"
Percent=""

echo Start Simulation
echo $Program
echo $FILE
echo $Percent

$Program $FILE 1 65536 4096 16384 1536 4000 390 35 114 1 BEYONDADDR $Percent ART RW N 1 10000 50 512 512 500 1 1 1 5 8 64 0 10 0
