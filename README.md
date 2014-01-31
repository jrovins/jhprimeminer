jhPrimeminer
============

jhPrimeminer is a optimized pool miner for primecoin.

The purpose of this version is to get a working linux build of 
jhprimeminer that supports the new -xpm option, 
and the getblocktemplate protocol.
( See: http://www.peercointalk.org/index.php?topic=623.0 )

This will allow the setup of a private mining pool with the primecoin-qt
wallet as a server, and multiple linux workers attached for mining.
This part of it has been tested and works well. (is mining blocks!!!)

However, This merged version needs a lot more polishing,
I have not had good luck connecting this version to public mining pools,
so I suggest that you use other versions of jhprimeminer for public pool mining,
until these issues are fixed. 

Right now I consider this an alpha version, so use at your own risk.

History:
This was tandyuk's linux master branch from 10/13, merged with the latest from Ray De Bourbon 11/13.

Their branches are still in this repo for comparison.

tandyuk was based on  deschlers linux version.

Requirements
Openssl and libgmp.



Build instructions:

CentOS:

yum groupinstall "Development Tools"

yum install openssl openssl-devel openssh-clients gmp gmp-devel gmp-static git

git clone https://github.com/jrovins/jhPrimeminer.git

cd jhPrimeminer

make


Ubuntu:

apt-get install build-essential libssl-dev openssl git libgmp libgmp-dev

git clone https://github.com/jrovins/jhPrimeminer.git

cd jhPrimeminer

make


