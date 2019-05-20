#!/bin/sh

input=$(wc -c input.txt | sed 's/  */ /g' | cut -d " " -f 2)
title="\tgzip\tbzip2\tlzma\n"

printf "Input file size: $input\n"

printf "\nCompressed file size in bytes:\n"
printf "$title"
rm *.lzma *.gzip *bzip2 2>/dev/null 
cp original.txt input.txt

for i in `seq 1 9`;
 	do
 		cp original.txt input.txt
 		gzip -$i input.txt
 		compressed_size=$(wc -c input.txt.gz | sed 's/  */ /g' | cut -d " " -f 2)
 		printf "$i\t$compressed_size"
 		gzip -$i -d input.txt.gz

 		bzip2 -$i input.txt 
 		compressed_size=$(wc -c input.txt.bz2 | sed 's/  */ /g' | cut -d " " -f 2)
 		printf "\t$compressed_size"
 		bzip2 -$i -d input.txt.bz2

 		lzma -$i input.txt 
 		compressed_size=$(wc -c input.txt.lzma | sed 's/  */ /g' | cut -d " " -f 2)
 		printf "\t$compressed_size\n"
 		lzma -$i -d input.txt.lzma

    done    


printf "\nCompression time:\n"
printf "\tgzip\t\tbzip2\t\tlzma\n"
for i in `seq 1 9`;
 	do


		com_time=$((time gzip -$i input.txt) 2>&1 | sed '2q;d' | cut -f 2)
 		printf "$i\t$com_time"
 		gzip -$i -d input.txt.gz

 		com_time=$((time bzip2 -$i input.txt) 2>&1 | sed '2q;d' | cut -f 2)
 		printf "\t$com_time"
 		bzip2 -$i -d input.txt.bz2

 		com_time=$((time lzma -$i input.txt) 2>&1 | sed '2q;d' | cut -f 2)
 		printf "\t$com_time\n"
 		lzma -$i -d input.txt.lzma


 		#echo $i
    done  

printf "\nDecompression time:\n"
printf "\tgzip\t\tbzip2\t\tlzma\n"
for i in `seq 1 9`;
 	do

 		gzip -$i input.txt
 		dec_time=$((time gzip -$i -d input.txt.gz) 2>&1| sed '2q;d' | cut -f 2)
 		printf "$i\t$dec_time"

 		bzip2 -$i input.txt
 		dec_time=$((time bzip2 -$i -d input.txt.bz2) 2>&1| sed '2q;d' | cut -f 2)
 		printf "\t$dec_time"

 		lzma -$i input.txt
 		dec_time=$((time lzma -$i -d input.txt.lzma) 2>&1 | sed '2q;d' | cut -f 2)
 		printf "\t$dec_time\n"


 		#echo $i
    done  


rm *.lzma *.gzip *bzip2 2>/dev/null 





#gzip -d input.txt.gz   
#com_time=$((time gzip input.txt) 2>&1 | sed '2q;d' | cut -f 2)
#compressed_size=$(wc -c input.txt.gz | cut -d " " -f 6)
#dec_time=$((time gzip -d input.txt.gz) 2>&1| sed '2q;d' | cut -f 2)

#printf "compression time: \t$com_time\n"
#printf "compressed file size: \t$compressed_size\n"
#printf "decompression time: \t$dec_time\n"



#com_time=$((time gzip input.txt) 2>&1 | sed '2q;d' | cut -f 2)
#dec_time=$((time gzip -d input.txt.gz) 2>&1| sed '2q;d' | cut -f 2)
#printf "compression time: \t$com_time\n"
#printf "decompression time: \t$dec_time\n"

#i=5
#printf "and again:\n"
#com_time=$((time gzip -$i input.txt) 2>&1 | sed '2q;d' | cut -f 2)
#dec_time=$((time gzip -$i -d input.txt.gz) 2>&1| sed '2q;d' | cut -f 2)

#gzip -5d input.txt.gz
