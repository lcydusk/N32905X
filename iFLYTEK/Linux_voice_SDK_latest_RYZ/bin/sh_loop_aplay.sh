#!/bin/sh

while true;
do
  ./aplay $1 &
  sleep 2
done
