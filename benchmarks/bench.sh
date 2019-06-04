#!/bin/sh

nbytes=10000000  # default file size
candidates=(../src/gzip bzip2 lzma) # do not put ./old_gzip here
sizes=(100 1024 102400 1048576) #100B 1KB 100KB 1MB
kernel_flag=0
size_flag=0
file=input


while getopts 'ks:' OPTION; do
    case "$OPTION" in
    k)
        kernel_flag=1
        ;;
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
    if [[ ! $(command -v curl) ]]
    then
      echo "curl is not installed on this machine. \
Please install curl before running these tests"
      exit 1
    fi
    curl http://ftp.gnu.org/gnu/gzip/gzip-1.10.tar.xz > gzip-1.10.tar.xz
    get_status=$?
    if [[ get_status -ne 0 ]]
    then
      echo "Something went wrong while downloading gzip to benchmark against. \
Aborting."
      exit 1
    fi
    echo "Downloading gnu gzip v1.1.0 to test against ..."
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
progs+=("../src/gzip -j 4")


size_conversion(){
	converted_size=$1
	if [ $(( $converted_size % 1048576 )) -eq 0 ] #check if in MB
	then
		converted_size=$((($converted_size)/1048576))
		converted_size+=MB
	elif [ $(( $converted_size % 1024 )) -eq 0 ]
	then
		converted_size=$((($converted_size)/1024))
		converted_size+=KB
	else
		converted_size+=B
	fi

	printf "$converted_size\n"
}

generate_random(){
	generate_size=0
	printf "\n"
        print_separator
        printf "Generating input file...\n"
	if [ $size_flag -eq 1 ]
	then
		generate_size=$nbytes
	else
		generate_size=$1
	fi

	printf "File size: "
	size_conversion $generate_size
	head -c $generate_size /dev/urandom > input

}

print_header () {
    for prog in "${progs[@]}"
    do
        printf "\t\t$prog"
    done
    printf "\n"
}

print_separator () {
    for i in {1..80}
    do
        printf "="
    done
    printf "\n"
}

# Run compression and decompression tests on the file at 'file'.
basic_eval() {
	compr_times=()
	decompr_times=()

	printf "\nCompressed file sizes:\n"
	print_header

	for i in `seq 1 9`
	do
	    printf "$i"
	    for prog in "${progs[@]}"
	    do
	        compr_time=$((time $prog -$i $file) 2>&1 | sed '2q;d' | cut -f 2)
	        compr_size=$(wc -c $file* | sed 's/^[ \t]*//g' | cut -d " " -f 1)
	        printf "\t\t$compr_size\t"
	        decompr_time=$((time $prog -$i -d $file*) 2>&1 | sed '2q;d' | cut -f 2)
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

rm input*


file=gzip_bin
printf "\n"
print_separator
cp old_gzip gzip_bin
gzip_bin_size=$(wc -c gzip_bin | sed 's/^[ \t]*//g' | cut -d " " -f 1)
printf "Testing on gzip 1.10 binary...\n"
printf "File size: $gzip_bin_size bytes\n"
basic_eval


if [ $kernel_flag -eq 1 ]
then
    file=ubuntu-16.04.iso
    wget http://mirror.math.princeton.edu/pub/ubuntu-iso/16.04/ubuntu-16.04.6-desktop-amd64.iso -O $file
    ubuntu_size=$(wc -c $file | sed 's/^[ \t]*//g' | cut -d " " -f 1)
    printf "\n"
    print_separator
    printf "Testing on .iso for Ubunty 16.04 LTS...\n"
    printf "File size: $ubuntu_size bytes\n"
    basic_eval
    rm $file
fi


rm -rf gzip-1.10* gzip_bin
