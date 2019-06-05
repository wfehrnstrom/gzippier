#!/bin/sh

# Our testing methodology is as follows: average over 10 iterations for each
# different file that we test. We test the following types of data
# 1. Random bytes
# 2. Text data
# 3. Video data
# 4. Concatenated files

print_separator () {
    for i in {1..80}
    do
        printf "="
    done
    printf "\n"
}

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
	generate_size=$1
	head -c $generate_size /dev/urandom > input
}


download=$1
if [[ download ]]
then
  echo "Downloading resources."
  if [[ ! $(command -v ./gzip-1.10) ]]
  then
    ./get.sh "http://ftp.gnu.org/gnu/gzip/gzip-1.10.tar.xz" "gzip-1.10.tar.xz"
  fi
fi
echo "Testing Text file(s) ..."
print_separator
echo ""
file=original.txt
./benchmark_file.sh $file 1 10

# if [[ ! `ls -l "http://mirror.math.princeton.edu/pub/ubuntu-iso/16.04/ubuntu-16.04.6-desktop-amd64.iso"` ]]
# then
#   curl "http://mirror.math.princeton.edu/pub/ubuntu-iso/16.04/ubuntu-16.04.6-desktop-amd64.iso" > ubuntu-16.04.6-desktop-amd64.iso
# fi

# ./benchmark_file.sh ubuntu-16.04.6-desktop-amd64.iso 4 1 1

# ./benchmark_file.sh jamesjoyceulysses.pdf 1 20
# echo "4 Threads..."
# print_separator
# ./benchmark_file.sh jamesjoyceulysses.pdf 4 20
#
# echo "1 Thread..."
# print_separator
# ./benchmark_file.sh all 1 1
# echo "2 Threads..."
# print_separator
# ./benchmark_file.sh all 2 1
# echo "4 Threads..."
# print_separator
# ./benchmark_file.sh all 4 1
# echo "8 Threads..."
# print_separator
# ./benchmark_file.sh all 8 1

./benchmark_file.sh acl-resources.txt 1 20 # 10KB
./benchmark_file.sh MAINTAINERS 1 5 #500KB
./benchmark_file.sh megabook 1 # 5 MB
./benchmark_file.sh megabook 5 # 5 MB, 5 threads


# rm input
# rm ubuntu-16.04.6-desktop-amd64.iso
