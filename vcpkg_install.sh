#!/bin/sh

if [ -d "vcpkg/" ]; then
  cd vcpkg/
  git pull
else
  git clone https://github.com/microsoft/vcpkg.git
  cd vcpkg/
fi
./bootstrap-vcpkg.sh -disableMetrics
./vcpkg x-set-installed "$@"
