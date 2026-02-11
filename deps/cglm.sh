cd /opt
wget https://github.com/recp/cglm/archive/refs/tags/v0.9.6.zip
unzip v0.9.6.zip
rm v0.9.6.zip
mv cglm-0.9.6 cglm
mkdir -p cglm/build
cd cglm/build
cmake .. -DCGLM_SHARED=ON
make -j`nproc`
make install

