#!/bin/sh

g++ request.cpp -I/open-vds/Dist/OpenVDS/include -L/open-vds/Dist/OpenVDS/lib  -lopenvds -ltbb -o single.out
./single.out

g++ request.cpp -I/open-vds/Dist/OpenVDS/include -L/open-vds/Dist/OpenVDS/lib  -lopenvds -ltbb -o multi.out
./multi.out 0 100 & \
./multi.out 100 200 & \
./multi.out 200 300 & \
./multi.out 300 400
