jhPrimeminer
============

jhPrimeminer is a optimized pool miner for primecoin.

This is my first attempt at merging rdebourbon's optimized build with deschler's linux build.

Requirements
Openssl and libgmp.

On centos I had issues, so compiled libgmp from source, and have to invoke the miner using
LD_LIBRARY_PATH=/usr/local/lib64 ./jhprimeminer .....


CentOS:

yum groupinstall "Development Tools"

yum install json-c json-c-devel libcurl libcurl-devel curl openssl openssl-devel openssh-clients gmp gmp-devel gmp-static git

git clone https://github.com/tandyuk/jhPrimeminer.git

cd jhPrimeminer

make



Ubuntu:

apt-get install build-essential libcurl4-openssl-dev libssl-dev openssl git libjson0 libjson0-dev

git clone https://github.com/tandyuk/jhPrimeminer.git

cd jhPrimeminer

make



If you found this helpful PLEASE support my work.

XPM: AYwmNUt6tjZJ1nPPUxNiLCgy1D591RoFn4

BTC: 1P6YrvFkwYGcw9sEFVQt32Cn7JJKx4pFG2
