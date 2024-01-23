#!/bin/sh

#if [ -d "build/" ]; then
#  cd build/
#fi

if [ -d "vcpkg/" ]; then
  cd vcpkg/
  git pull origin master
else
  git clone https://github.com/microsoft/vcpkg.git
  cd vcpkg/
fi
./bootstrap-vcpkg.sh -disableMetrics
echo "Updating vcpkg"
./vcpkg x-set-installed "$@" > vcpkg.log
#./vcpkg integrate install
