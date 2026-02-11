#!/bin/bash

podman build -t cdev:fedora .

mkdir -p ../root/{bin,sbin,lib,lib64,include,share}
cp dev ..
chmod +x ../dev
