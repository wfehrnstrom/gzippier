#!/bin/sh

nbytes=10000000  # default file size
candidates=(../src/gzip bzip2 lzma) # do not put ./old_gzip here
sizes=(10000 100 10)
size_flag=0

while getopts 's:' OPTION; do
    case "$OPTION" in
    s)
        nbytes="$OPTARG"
        ;;
    ?)
        echo "script usage: $(basename $0) [-s number_of_bytes]" >&2
        exit 1
        ;;
    esac
done

# Get and compile version 1.10 of gzip.
if [[ ! $(command -v ./old_gzip) ]]
then
    wget http://ftp.gnu.org/gnu/gzip/gzip-1.10.tar.xz
    tar xf gzip-1.10.tar.xz
    cd gzip-1.10
    ./configure
    make all
    mv gzip ../old_gzip
    cd ..
fi


progs=()
for prog in "${candidates[@]}"
do
    if [[ $(command -v $prog) ]]
    then
        progs+=($prog)
    fi
done
progs+=(./old_gzip)

generate_random(){
	generate_size=0
	printf "\nGenerating input file...\n"
	if [ $size_flag -eq 1 ]
	then
		generate_size=$nbytes
	else
		generate_size=$1
	fi

	printf "File size: $generate_size bytes\n"
	head -c $generate_size /dev/urandom > input

}

print_header () {
    for prog in "${progs[@]}"
    do
        printf "\t\t$prog"
    done
    printf "\n"
}


basic_eval(){
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
	        printf "\t\t$compr_size\t"
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
	        printf "\t${compr_times[$n]}\t"
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
	        printf "\t${decompr_times[$n]}\t"
	        n=$((n+1))
	    done
	    printf "\n"
	done
}

if [ $size_flag -eq 1 ]
then
	generate_random
	basic_eval
else
	for size in "${sizes[@]}"
	do
		generate_random $size
		basic_eval

	done
fi


rm -rf input* gzip-1.10*
