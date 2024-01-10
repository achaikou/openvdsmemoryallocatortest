#!/bin/sh

g++ request.cpp -I/open-vds/Dist/OpenVDS/include -L/open-vds/Dist/OpenVDS/lib  -lopenvds -ltbb -o single.out
./single.out

start=`date +%s.%N`

g++ request.cpp -I/open-vds/Dist/OpenVDS/include -L/open-vds/Dist/OpenVDS/lib  -lopenvds -ltbb -o multi.out
./multi.out 0 100 & \
./multi.out 100 200 & \
./multi.out 200 300 & \
./multi.out 300 400

end=$(date +%s%N)
runtime=$((( $end - $start)/1000000))
echo "Total time of multiprocess: $runtime ms"
