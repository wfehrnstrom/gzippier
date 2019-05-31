#!/bin/sh

file_size=10000000 #default size



while getopts 's:' OPTION; do
  case "$OPTION" in
    s)
      file_size="$OPTARG"
      ;;
    ?)
      echo "script usage: $(basename $0) [-s number_of_bytes]" >&2
      exit 1
      ;;
  esac
done
shift "$(($OPTIND -1))"

wget http://ftp.gnu.org/gnu/gzip/gzip-1.10.tar.xz
tar xf gzip-1.10.tar.xz 
cd gzip-1.10
./configure
make all
mv gzip ../old_gzip
cd ..


echo "Generating input file..."
head -c $file_size /dev/urandom >input.txt

input=$(wc -c input.txt | sed 's/^[ \t]*//g' | cut -d " " -f 1)

printf "Input file size: $input\n"



gzip_com=()
gzip_dec=()

old_gzip_com=()
old_gzip_dec=()



printf "\nCompressed file size in bytes:\n"
printf "\tgzip\told_gzip\n"
rm *.lzma *.gz *bz2 2>/dev/null 
#cp original.txt input.txt
for i in `seq 1 9`;
do
 	
	com_time=$((time gzip -$i input.txt) 2>&1 | sed '2q;d' | cut -f 2)
 	compressed_size=$(wc -c input.txt.gz | sed 's/^[ \t]*//g' | cut -d " " -f 1)
 	dec_time=$((time gzip -$i -d input.txt.gz) 2>&1| sed '2q;d' | cut -f 2)
 	printf "$i\t$compressed_size"
 	gzip_com+=($com_time)
 	gzip_dec+=($dec_time)

 	com_time=$((time ./old_gzip -$i input.txt) 2>&1 | sed '2q;d' | cut -f 2)
 	compressed_size=$(wc -c input.txt.gz | sed 's/^[ \t]*//g' | cut -d " " -f 1)
 	dec_time=$((time ./old_gzip -$i -d input.txt.gz) 2>&1| sed '2q;d' | cut -f 2)
 	printf "\t$compressed_size\n"
	old_gzip_com+=($com_time)
 	old_gzip_dec+=($dec_time)



done  

printf "\nCompression time:\n"
printf "\tgzip\t\told_gzip\n"
for i in `seq 1 9`;
do
	j=($i)-1
	printf "$i\t${gzip_com[$j]}"
	printf "\t${old_gzip_com[$j]}\n"


done  

printf "\nDecompression time:\n"
printf "\tgzip\t\told_gzip\n"
for i in `seq 1 9`;
do
	j=($i)-1
	printf "$i\t${gzip_dec[$j]}"
	printf "\t${old_gzip_dec[$j]}\n"

done  

rm *.gz *bz2 2>/dev/null 
rm input.txt
rm -rf gzip-1.10* old_gzip


 #if we're doing this, then we'll always need a compressed size, dec_size... * 3.  great.







