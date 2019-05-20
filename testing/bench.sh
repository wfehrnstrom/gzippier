#!/bin/sh


input=$(wc -c input.txt | sed 's/  */ /g' | cut -d " " -f 2)

printf "Input file size: $input\n"



gzip_com=()
gzip_dec=()
bzip2_com=()
bzip2_dec=()
lzma_com=()
lzma_dec=()



printf "\nCompressed file size in bytes:\n"
printf "\tgzip\tbzip2\tlzma\n"
rm *.lzma *.gzip *bzip2 2>/dev/null 
cp original.txt input.txt
for i in `seq 1 9`;
do
 	
	com_time=$((time gzip -$i input.txt) 2>&1 | sed '2q;d' | cut -f 2)
 	compressed_size=$(wc -c input.txt.gz | sed 's/  */ /g' | cut -d " " -f 2)
 	dec_time=$((time gzip -$i -d input.txt.gz) 2>&1| sed '2q;d' | cut -f 2)
 	printf "$i\t$compressed_size"
 	gzip_com+=($com_time)
 	gzip_dec+=($dec_time)

 	com_time=$((time bzip2 -$i input.txt) 2>&1 | sed '2q;d' | cut -f 2)
 	compressed_size=$(wc -c input.txt.bz2 | sed 's/  */ /g' | cut -d " " -f 2)
 	dec_time=$((time bzip2 -$i -d input.txt.bz2) 2>&1| sed '2q;d' | cut -f 2)
 	printf "\t$compressed_size"
 	bzip2_com+=($com_time)
 	bzip2_dec+=($dec_time)

 	com_time=$((time lzma -$i input.txt) 2>&1 | sed '2q;d' | cut -f 2)
 	compressed_size=$(wc -c input.txt.lzma | sed 's/  */ /g' | cut -d " " -f 2)
 	dec_time=$((time lzma -$i -d input.txt.lzma) 2>&1| sed '2q;d' | cut -f 2)
 	printf "\t$compressed_size\n"
 	lzma_com+=($com_time)
 	lzma_dec+=($dec_time)



done  

printf "\nCompression time:\n"
printf "\tgzip\tbzip2\tlzma\n"
for i in `seq 1 9`;
do
	j=($i)-1
	printf "$i\t${gzip_com[$j]}"
	printf "\t${bzip2_com[$j]}"
	printf "\t${lzma_com[$j]}\n"

done  

printf "\nDecompression time:\n"
printf "\tgzip\tbzip2\tlzma\n"
for i in `seq 1 9`;
do
	j=($i)-1
	printf "$i\t${gzip_dec[$j]}"
	printf "\t${bzip2_dec[$j]}"
	printf "\t${lzma_dec[$j]}\n"

done  

rm *.lzma *.gzip *bzip2 2>/dev/null 


 #if we're doing this, then we'll always need a compressed size, dec_size... * 3.  great.







