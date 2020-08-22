#!/bin/bash

NUM_THREADS="1 2 4 8 16 32 64"
BUF_SIZE="100 1000 10000"

for num_thread in ${NUM_THREADS}
do 
    for buf_size in ${BUF_SIZE}
    do
        echo "-------*********----------"
        echo "mr-wordc perf-eval.txt with $num_thread threads and buffer of $buf_size bytes "
        for i in $(seq 1 5)
        do
            elapsed_time=` { /usr/bin/time -p ./mr-wordc input/mr-wordc/perf-eval.txt /dev/null $num_thread $buf_size; }  2>&1`
            elapsed_time_user=`echo ${elapsed_time} | cut -d' ' -f 4`
            elapsed_time_sys=`echo ${elapsed_time} | cut -d' ' -f 6`
            elapsed_time_total=$(echo "${elapsed_time_user} + ${elapsed_time_sys}" | bc)
            echo "${i}-th run's consumed time is $elapsed_time_total seconds"
        done
        echo "-------*********----------"
        echo ""
    done
done
