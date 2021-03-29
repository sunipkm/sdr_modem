#!/bin/bash
echo "960">/sys/class/gpio/export
echo "out">/sys/class/gpio/gpio960/direction
echo "1">/sys/class/gpio/gpio960/value
echo "0">/sys/class/gpio/gpio960/value
echo "960">/sys/class/gpio/unexport

