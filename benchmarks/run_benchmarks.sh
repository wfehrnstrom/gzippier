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
  ./get.sh "http://ftp.gnu.org/gnu/gzip/gzip-1.10.tar.xz" "gzip-1.10.tar.xz"
fi
echo "Testing Text file(s) ..."
print_separator
echo ""
file=original.txt
./benchmark_file.sh $file 1

# Now test files of random bytes of different sizes
sizes=(100 1024 102400 1048576)
echo "Testing random byte file(s) ..."
print_separator
echo ""
for size in "${sizes[@]}"
do
  generate_random $size
  ./benchmark_file.sh input 1
done

rm input

# http://ftp.gnu.org/gnu/gzip/gzip-1.10.tar.xz
# gzip-1.10.tar.xz
