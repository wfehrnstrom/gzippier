#!/bin/bash
url=$1
file=$2
if [[ ! $(command -v curl) ]]
then
    echo "curl must be installed to download gzip 1.10."
    exit 1
fi

echo "Downloading..."
curl $url > $file
status=$?
if [[ status -ne 0 ]]
then
    echo "Failed to download from $url!"
    exit 1
fi

if [[ $file = 'gzip-1.10.tar.xz' ]]
then
  tar xf $file
  cd gzip-1.10
  printf "\nBuilding gzip-1.10..."
  ./configure
  make all
  cd ..
  mv gzip-1.10/gzip gzip
  rm -rf gzip-1.10 gzip-1.10.tar.xz
  mv gzip gzip-1.10
fi
