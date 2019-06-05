#!/bin/bash

if [[ ! $(command -v ./gzip-1.10) ]]
then
    if [[ ! $(command -v curl) ]]
    then
        echo "curl must be installed to download gzip 1.10."
        exit 1
    fi

    echo "Downloading GNU gzip 1.10..."
    curl http://ftp.gnu.org/gnu/gzip/gzip-1.10.tar.xz >gzip-1.10.tar.xz
    status=$?
    if [[ status -ne 0 ]]
    then
        echo "Failed to download gzip 1.10!"
        exit 1
    fi

    tar xf gzip-1.10.tar.xz
    cd gzip-1.10
    printf "\nBuilding gzip 1.10..."
    ./configure
    make all
    cd ..
    mv gzip-1.10/gzip gzip
    rm -rf gzip-1.10 gzip-1.10.tar.xz
    mv gzip gzip-1.10
fi
