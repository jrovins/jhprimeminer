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

This has been tested & working with ypool, needs the following options:
./jhprimeminer -o http://ypool.net:8080   -u <Username>.<Worker> -p <Password> -xpt

The Stat counters have been modified:
when private pool mining with the -xpm option, all chains found by the miner will be tallied.
seeing it this way is more helpful for fine tuning, without this change you will only see whole blocks found.
when mining on a public pool, all the chains accepted by the server will be tallied, just like it was before.

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


