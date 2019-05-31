#!/bin/sh

nbytes=10000000 #default size
candidates=(gzip bzip2 lzma ./oldgzip)


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
#shift "$(($OPTIND -1))"

wget http://ftp.gnu.org/gnu/gzip/gzip-1.10.tar.xz
tar xf gzip-1.10.tar.xz 
cd gzip-1.10
./configure
make all
mv gzip ../old_gzip
cd ..


echo "Generating input file..."
head -c $nbytes /dev/urandom >input

progs=()
for prog in "${candidates[@]}"
do
    if [[ $(command -v $prog) ]]
    then
        progs+=($prog)
    fi
done

progs+=(./old_gzip)


print_header () {
    for prog in "${progs[@]}"
    do
        printf "\t\t$prog"
    done
    printf "\n"
}



#input=$(wc -c input.txt | sed 's/^[ \t]*//g' | cut -d " " -f 1)

#printf "Input file size: $input\n"




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


rm -rf input* gzip-1.10* old_gzip

#rm -rf gzip-1.10* old_gzip


 #if we're doing this, then we'll always need a compressed size, dec_size... * 3.  great.







