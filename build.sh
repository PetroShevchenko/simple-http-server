#!/bin/bash

export CC="/usr/bin/gcc-12"
export CXX="/usr/bin/g++-12"

# build.sh clean

if [[ $1 = "clean" ]] ; then
    rm -rf build
    echo "rm -rf build";
    echo "**** buld directory has been removed ****";
    exit 0
fi

# build.sh all
mkdir -p build
cd build
cmake ..
make 
