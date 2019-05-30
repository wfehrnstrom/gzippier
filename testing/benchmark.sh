#!/bin/bash

# Usage: ./benchmark [<prog>...]

nbytes=1000000


printf "Input file size: $nbytes\n"
head -c $nbytes /dev/urandom >input

# Fill progs with all programs from the command-line arguments that can be
# located on this system.
progs=()
for prog in "$@"
do
    if [[ $(command -v $prog) ]]
    then
        progs+=($prog)
    fi
done

# Print the name of each program being tested as the header for each column.
print_header () {
    for prog in "${progs[@]}"
    do
        printf "\t\t$prog"
    done
    printf "\n"
}


compr_times=()
decompr_times=()

printf "\nCompressed file sizes:\n"
print_header

for i in `seq 1 9`
do
    printf "$i"
    for prog in "${progs[@]}"
    do
        compr_time=$((time $prog -$i input) 2>&1 | sed '2q;d' | cut -f 2)
        compr_size=$(wc -c input* | sed 's/^[ \t]*//g' | cut -d " " -f 1)
        printf "\t\t$compr_size"
        decompr_time=$((time $prog -$i -d input*) 2>&1 | sed '2q;d' | cut -f 2)

        compr_times+=($compr_time)
        decompr_times+=($decompr_time)
    done
    printf "\n"
done


printf "\nCompression times:\n"
print_header

n=0
for i in `seq 1 9`
do
    printf "$i\t"
    for prog in "${progs[@]}"
    do
        printf "\t${compr_times[$n]}"
        n=$((n+1))
    done
    printf "\n"
done


printf "\nDecompression times:\n"
print_header

n=0
for i in `seq 1 9`
do
    printf "$i\t"
    for prog in "${progs[@]}"
    do
        printf "\t${decompr_times[$n]}"
        n=$((n+1))
    done
    printf "\n"
done


rm -f input*
