benchmarks-local: bench.sh $(top_srcdir)/src/gzip
	./run_benchmarks.sh

download-benchmarks: get.sh
	./get.sh https://raw.githubusercontent.com/TerrenceHo/gzip_benchmarks_repo/master/MAINTAINERS MAINTAINERS
	./get.sh https://raw.githubusercontent.com/TerrenceHo/gzip_benchmarks_repo/master/acl-resources.txt acl-resources.txt
	./get.sh https://raw.githubusercontent.com/TerrenceHo/gzip_benchmarks_repo/master/megabook megabook
