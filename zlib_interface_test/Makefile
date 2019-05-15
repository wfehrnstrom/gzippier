all: test

zpipe: zpipe.c 
	gcc -lz zpipe.c -o zpipe

test: zpipe gzip-zpipe-test zpipe-zpipe-test zpipe-gzip-test

gzip-zpipe-test:
	gzip -k test.txt
	./zpipe -d < test.txt.gz > test.txt.c
	diff test.txt test.txt.c
	rm -f test.txt.gz test.txt.c

zpipe-zpipe-test:
	./zpipe < test.txt > test.txt.z
	./zpipe -d < test.txt.z > test.txt.c
	diff test.txt test.txt.c
	rm -f test.txt.z test.txt.c

zpipe-gzip-test:
	./zpipe < test.txt > test.txt.gz
	mv test.txt temp.txt
	gzip -d test.txt.gz  
	diff test.txt temp.txt
	mv temp.txt test.txt

clean:
	rm -f zpipe
