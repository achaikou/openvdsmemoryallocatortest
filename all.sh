#!/bin/sh
n=8
min_iline=0
max_iline=6400

g++ request.cpp -O3 -DNDEBUG -I/open-vds/Dist/OpenVDS/include -L/open-vds/Dist/OpenVDS/lib  -lopenvds -ltbb -o request.out
# ./request.out "no_concurrency" $min_iline $max_iline
# ./request.out "one_handle_1_thread" $min_iline $max_iline
#./request.out "one_handle_n_threads" $min_iline $max_iline $n
#./request.out "many_handles_n_threads" $min_iline $max_iline $n
./request.out "all" $min_iline $max_iline $n


# now run same split into processes - it shows possibility of speedup

echo "Running as $n processes"
start_time=$(date +%s%N)


iline_step=$(( ($max_iline - $min_iline) / $n ))

for i in $(seq 0 $(($n - 1))); do
    start_iline=$(( $min_iline + ($i * $iline_step) ))
    end_iline=$(( $start_iline + $iline_step ))
    ./request.out "process" $start_iline $end_iline &
done

wait

end_time=$(date +%s%N)

runtime=$((( $end_time - $start_time)/1000000))

echo "Total time of multiprocess: $runtime ms"