#!/bin/bash

# This script assumes that the working directory is benchmarks, since programs
# are specified using relative paths.
#
# To include gzippier in the benchmarks, ensure that it has been built and
# resides in the src directory.
#
# To include gzip 1.10 in the benchmarks, download and build it by running
# get_gzip.sh.
#
# To include pigz and/or bzip2 in the benchmarks, ensure that the system can
# locate them somewhere in its PATH.

possible_progs=(../src/gzip ./gzip-1.10 pigz bzip2)
possible_pretty_progs=("gzippier" "gzip" "pigz" "bzip2")

progs=()
pretty_progs=()

for i in $(seq 0 "${#possible_progs[@]}")
do
    if [[ $(command -v "${possible_progs[$i]}") ]]
    then
        progs+=("${possible_progs[$i]}")
        pretty_progs+=("${possible_pretty_progs[$i]}")
    fi
done

get_file_size () {
    local file=$1
    local file_size=$(wc -c $file | sed 's/^[ \t]*//g' | cut -d ' ' -f 1)
    echo "$file_size"
}

print_with_padding () {
    local str=$1
    printf "$str"
    if [ ${#str} -lt 7 ]
    then
        printf "\t"
    fi
    printf "\t"
}

print_header () {
    local row_label=$1
    print_with_padding $row_label
    for prog in "${pretty_progs[@]}"
    do
        print_with_padding $prog
    done
    printf "\n"
}

benchmark () {
    local file=$1
    local num_threads=$2
    local compr_times=()
    local decompr_times=()

    echo "Compressed file sizes:"
    print_header level
    for level in {1..9}
    do
        print_with_padding $level
        for prog in "${progs[@]}"
        do
            jflag=
            # TODO: Uncomment this after merging with the parallelize branch.
#            if [ "$prog" == "../src/gzip" ]
#            then
#                jflag="-j $num_threads"
#            fi
            if [ "$prog" == "pigz" ]
            then
                jflag="-p $num_threads"
            fi

            time=$((time $prog $jflag -$level $file) 2>&1 | sed '2q;d' | \
                       cut -f 2)
            compr_times+=($time)

            file_size=$(get_file_size "$file*")
            print_with_padding $file_size

            time=$((time $prog -d $file*) 2>&1 | sed '2q;d' | cut -f 2)
            decompr_times+=($time)
        done
        printf "\n"
    done
    printf "\n"

    echo "Compression times:"
    print_header level
    i=0
    for level in {1..9}
    do
        print_with_padding $level
        for prog in "${progs[@]}"
        do
            print_with_padding "${compr_times[$i]}"
            i=$((i+1))
        done
        printf "\n"
    done
    printf "\n"

    echo "Decompression times:"
    print_header level
    i=0
    for level in {1..9}
    do
        print_with_padding $level
        for prog in "${progs[@]}"
        do
            print_with_padding "${decompr_times[$i]}"
            i=$((i+1))
        done
        printf "\n"
    done
}

if [ -z $1 ] || [ ! -z $3 ]
then
    echo "Usage: $0 <file> [<num_threads>]"
    exit 1
fi

file=$1
num_threads=1
if [ ! -z $2 ]
then
    num_threads=$2
fi

file_size=$(get_file_size $file)

echo "Running benchmarks on $file..."
echo "Uncompressed file size: $file_size"
echo "Number of threads for gzippier and pigz: $num_threads"
printf "\n"

benchmark $file $num_threads
