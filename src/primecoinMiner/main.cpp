#include"global.h"
#include "ticker.h"
//#include<intrin.h>
#include<ctime>
#include<map>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <termios.h>            //termios, TCSANOW, ECHO, ICANON
#include <unistd.h>     //STDIN_FILENO


//used for get_num_cpu
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

primeStats_t primeStats = {0};
commandlineInput_t commandlineInput = {0};
commandlineInput_t OldCommandlineInput = {0};
volatile int total_shares = 0;
volatile int valid_shares = 0;
unsigned int nMaxSieveSize;
unsigned int vPrimesSize;
float vPrimesMult = 184.5;
float vPrimesAvg = 8;
float vPrimesAdj = 1;
unsigned int nMaxPrimes;
bool nPrintDebugMessages;
unsigned long nOverrideTargetValue;
unsigned int nOverrideBTTargetValue;
unsigned int nTarget;
char* dt;
uint64 lastShareSubmit = getTimeMilliseconds(); // Lets pretend something was submitted at start - to not reset too soon!
unsigned int nRoundSievePercentage;

char* minerVersionString = "jhPrimeminer X1 (AeroCloud)";

bool error(const char *format, ...)
{
	puts(format);
	return false;
}


bool hex2bin(unsigned char *p, const char *hexstr, size_t len)
{
	bool ret = false;

	while (*hexstr && len) {
		char hex_byte[4];
		unsigned int v;

		if (!hexstr[1]) {
			//printf("hex2bin str truncated");
			std::cout << "hex2bin str truncated" << std::endl;
			return ret;
		}

		memset(hex_byte, 0, 4);
		hex_byte[0] = hexstr[0];
		hex_byte[1] = hexstr[1];

		if (sscanf(hex_byte, "%x", &v) != 1) {
			//printf( "hex2bin sscanf '%s' failed", hex_byte);
			std::cout << "hex2bin sscanf '" << hex_byte << "' failed" << std::endl;
			return ret;
		}

		*p = (unsigned char) v;

		p++;
		hexstr += 2;
		len--;
	}

	if (len == 0 && *hexstr == 0)
		ret = true;
	return ret;
}



uint32 _swapEndianessU32(uint32 v)
{
	return ((v>>24)&0xFF)|((v>>8)&0xFF00)|((v<<8)&0xFF0000)|((v<<24)&0xFF000000);
}

uint32 _getHexDigitValue(uint8 c)
{
	if( c >= '0' && c <= '9' )
		return c-'0';
	else if( c >= 'a' && c <= 'f' )
		return c-'a'+10;
	else if( c >= 'A' && c <= 'F' )
		return c-'A'+10;
	return 0;
}

/*
 * Parses a hex string
 * Length should be a multiple of 2
 */
void yPoolWorkMgr_parseHexString(char* hexString, uint32 length, uint8* output)
{
	uint32 lengthBytes = length / 2;
	for(uint32 i=0; i<lengthBytes; i++)
	{
		// high digit
		uint32 d1 = _getHexDigitValue(hexString[i*2+0]);
		// low digit
		uint32 d2 = _getHexDigitValue(hexString[i*2+1]);
		// build byte
		output[i] = (uint8)((d1<<4)|(d2));	
	}
}

/*
 * Parses a hex string and converts it to LittleEndian (or just opposite endianness)
 * Length should be a multiple of 2
 */
void yPoolWorkMgr_parseHexStringLE(char* hexString, uint32 length, uint8* output)
{
	uint32 lengthBytes = length / 2;
	for(uint32 i=0; i<lengthBytes; i++)
	{
		// high digit
		uint32 d1 = _getHexDigitValue(hexString[i*2+0]);
		// low digit
		uint32 d2 = _getHexDigitValue(hexString[i*2+1]);
		// build byte
		output[lengthBytes-i-1] = (uint8)((d1<<4)|(d2));	
	}
}


void primecoinBlock_generateHeaderHash(primecoinBlock_t* primecoinBlock, uint8 hashOutput[32])
{
	uint8 blockHashDataInput[512];
	memcpy(blockHashDataInput, primecoinBlock, 80);
	sha256_context ctx;
	sha256_starts(&ctx);
	sha256_update(&ctx, (uint8*)blockHashDataInput, 80);
	sha256_finish(&ctx, hashOutput);
	sha256_starts(&ctx); // is this line needed?
	sha256_update(&ctx, hashOutput, 32);
	sha256_finish(&ctx, hashOutput);
}

void primecoinBlock_generateBlockHash(primecoinBlock_t* primecoinBlock, uint8 hashOutput[32])
{
	uint8 blockHashDataInput[512];
	memcpy(blockHashDataInput, primecoinBlock, 80);
	uint32 writeIndex = 80;
	sint32 lengthBN = 0;
	CBigNum bnPrimeChainMultiplier;
	bnPrimeChainMultiplier.SetHex(primecoinBlock->mpzPrimeChainMultiplier.get_str(16));
	std::vector<unsigned char> bnSerializeData = bnPrimeChainMultiplier.getvch();
	lengthBN = bnSerializeData.size();
	*(uint8*)(blockHashDataInput+writeIndex) = (uint8)lengthBN;
	writeIndex += 1;
	memcpy(blockHashDataInput+writeIndex, &bnSerializeData[0], lengthBN);
	writeIndex += lengthBN;
	sha256_context ctx;
	sha256_starts(&ctx);
	sha256_update(&ctx, (uint8*)blockHashDataInput, writeIndex);
	sha256_finish(&ctx, hashOutput);
	sha256_starts(&ctx); // is this line needed?
	sha256_update(&ctx, hashOutput, 32);
	sha256_finish(&ctx, hashOutput);
}


#define MINER_PROTOCOL_GETWORK		(1)
#define MINER_PROTOCOL_STRATUM		(2)
#define MINER_PROTOCOL_XPUSHTHROUGH	(3)


jsonRequestTarget_t jsonRequestTarget = {0}; // rpc login data
jsonRequestTarget_t jsonLocalPrimeCoin; // rpc login data
bool useLocalPrimecoindForLongpoll;



workData_t workData;


/*
 * Pushes the found block data to the server for giving us the $$$
 * Uses getwork to push the block
 * Returns true on success
 * Note that the primecoin data can be larger due to the multiplier at the end, so we use 256 bytes per default
 */
bool jhMiner_pushShare_primecoin(primecoinBlock_t* primecoinBlock)
{
	// printf("Queue share\n");
		xptShareToSubmit_t* xptShareToSubmit = (xptShareToSubmit_t*)malloc(sizeof(xptShareToSubmit_t));
		memset(xptShareToSubmit, 0x00, sizeof(xptShareToSubmit_t));
		memcpy(xptShareToSubmit->merkleRoot, primecoinBlock->merkleRoot, 32);
		memcpy(xptShareToSubmit->prevBlockHash, primecoinBlock->prevBlockHash, 32);
		xptShareToSubmit->version = primecoinBlock->version;
		xptShareToSubmit->nBits = primecoinBlock->nBits;
		xptShareToSubmit->nonce = primecoinBlock->nonce;
		xptShareToSubmit->nTime = primecoinBlock->timestamp;
		// set multiplier
		CBigNum bnPrimeChainMultiplier;
		bnPrimeChainMultiplier.SetHex(primecoinBlock->mpzPrimeChainMultiplier.get_str(16));
		std::vector<unsigned char> bnSerializeData = bnPrimeChainMultiplier.getvch();
		sint32 lengthBN = bnSerializeData.size();
		memcpy(xptShareToSubmit->chainMultiplier, &bnSerializeData[0], lengthBN);
		xptShareToSubmit->chainMultiplierSize = lengthBN;
		// todo: Set stuff like sieve size
		if( workData.xptClient && !workData.xptClient->disconnected){
			xptClient_foundShare(workData.xptClient, xptShareToSubmit);
			lastShareSubmit = getTimeMilliseconds();
			return true;
		}
		else
		{
			//printf("Share submission failed. The client is not connected to the pool.\n");
			std::cout << "Share submission failed. The client is not connected to the pool." << std::endl;
			return false;
		}
}

static double DIFFEXACTONE = 26959946667150639794667015087019630673637144422540572481103610249215.0;
static const uint64_t diffone = 0xFFFF000000000000ull;

#ifdef _WIN32
static double target_diff(const unsigned char *target)
{
	double targ = 0;
	signed int i;

	for (i = 31; i >= 0; --i)
		targ = (targ * 0x100) + target[i];

	return DIFFEXACTONE / (targ ? targ: 1);
}
#endif

//static double DIFFEXACTONE = 26959946667150639794667015087019630673637144422540572481103610249215.0;
//static const uint64_t diffone = 0xFFFF000000000000ull;

double target_diff(const uint32_t  *target)
{
	double targ = 0;
	signed int i;

	for (i = 0; i < 8; i++)
		targ = (targ * 0x100) + target[7 - i];

	return DIFFEXACTONE / ((double)targ ?  targ : 1);
}


std::string HexBits(unsigned int nBits)
{
    union {
        int32_t nBits;
        char cBits[4];
    } uBits;
    uBits.nBits = htonl((int32_t)nBits);
    return HexStr(BEGIN(uBits.cBits), END(uBits.cBits));
}

#ifdef _WIN32
static bool IsXptClientConnected()
{
	__try
	{
		if (workData.xptClient == NULL || workData.xptClient->disconnected)
			return false;
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}

	return true;
}
#endif


int getNumThreads(void) {
  // based on code from ceretullis on SO
  uint32_t numcpu = 1; // in case we fall through;
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
  int mib[4];
  size_t len = sizeof(numcpu); 

  /* set the mib for hw.ncpu */
  mib[0] = CTL_HW;
#ifdef HW_AVAILCPU
  mib[1] = HW_AVAILCPU;  // alternatively, try HW_NCPU;
#else
  mib[1] = HW_NCPU;
#endif
  /* get the number of CPUs from the system */
sysctl(mib, 2, &numcpu, &len, NULL, 0);

    if( numcpu < 1 )
    {
      numcpu = 1;
    }

#elif defined(__linux__) || defined(sun) || defined(__APPLE__)
  numcpu = static_cast<uint32_t>(sysconf(_SC_NPROCESSORS_ONLN));
#elif defined(_SYSTYPE_SVR4)
  numcpu = sysconf( _SC_NPROC_ONLN );
#elif defined(hpux)
  numcpu = mpctl(MPC_GETNUMSPUS, NULL, NULL);
#elif defined(_WIN32)
  SYSTEM_INFO sysinfo;
  GetSystemInfo( &sysinfo );
  numcpu = sysinfo.dwNumberOfProcessors;
#endif
  
  return numcpu;
}


uint32 jhMiner_getCurrentWorkBlockHeight(sint32 threadIndex)
{
	return ((serverData_t*)workData.workEntry[threadIndex].serverData)->blockHeight;
}

/*
 * Worker thread mainloop for xpt() mode
 */
#ifdef _WIN32
int jhMiner_workerThread_xpt(int threadIndex)
{
#else
void *jhMiner_workerThread_xpt(void *arg){
uint32_t threadIndex = static_cast<uint32_t>((uintptr_t)arg);
#endif

	CSieveOfEratosthenes* psieve = NULL;
	while( true )
	{
		uint8 localBlockData[128];
		// copy block data from global workData
		uint32 workDataHash = 0;
		uint8 serverData[32];
		while( workData.workEntry[threadIndex].dataIsValid == false ) Sleep(50);
#ifdef _WIN32
		EnterCriticalSection(&workData.cs);
#else
    pthread_mutex_lock(&workData.cs);
#endif
		memcpy(localBlockData, workData.workEntry[threadIndex].data, 128);
		memcpy(serverData, workData.workEntry[threadIndex].serverData, 32);
		workDataHash = workData.workEntry[threadIndex].dataHash;
#ifdef _WIN32
		LeaveCriticalSection(&workData.cs);
#else
    pthread_mutex_unlock(&workData.cs);
#endif
		// convert raw data into primecoin block
		primecoinBlock_t primecoinBlock = {0};
		memcpy(&primecoinBlock, localBlockData, 80);
		// we abuse the timestamp field to generate an unique hash for each worker thread...
		primecoinBlock.timestamp += threadIndex;
		primecoinBlock.threadIndex = threadIndex;
		primecoinBlock.xptMode = (workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH);
		// ypool uses a special encrypted serverData value to speedup identification of merkleroot and share data
		memcpy(&primecoinBlock.serverData, serverData, 32);
		// start mining
		//uint32 time1 = GetTickCount();
      	BitcoinMiner(&primecoinBlock, psieve, threadIndex);
		//printf("Mining stopped after %dms\n", GetTickCount()-time1);
		primecoinBlock.mpzPrimeChainMultiplier = 0;
	}
	if( psieve )
	{
		delete psieve;
		psieve = NULL;
	}
	return 0;
}



void jhMiner_printHelp()
{
	using namespace std;
	cout << "Usage: jhPrimeminer.exe [options]" << endl;
	cout << "Core Options:" << endl;
	cout << "  -conf <path>                   The config file to use. (default = ./jhprimeminer.conf)" << endl;
	cout << "                                 If writeable, and -key is provided, this may be updated remotely" << endl;
	cout << "                                 If readable, config file will be read, and any commandline options AFTER -conf override the config file" << endl;
	cout << "                                 If provided, this MUST be the first commandline option given" << endl;
	cout << "  -key <string>                  API key to use when connecting to a central server for stats and remote config" << endl;
	cout << "                                 Automatically sets central server url to http://xpm.tandyuk.com:80 if -cs is not provided" << endl;	
	cout << "  -cs <url>                      URL of central server to use for JSON-RPC stats and remote config" << endl;
	cout << "  -csenabled [true|false|1|0]    Whether to enable central server mode. This is automatically enabled if -key or -cs is provided" << endl;
	cout << "                                 or is defined in the config file. (default = false)" << endl;
	cout << "  -sslnoverify [true|false|1|0]  Disable strict checking of SSL certificates. (default = false)" << endl;
	cout << "                                 Enable this option if your central server has a self signed SSL certificate" << endl;
	cout << "Display Options" << endl;
	cout << "  -quiet                         Enable Quiet mode. Client will only print 1 line per share found" << endl;
	cout << "  -silent                        Enable Silent mode. No output to console." << endl;
	cout << "                                 In addition if central server is disabled, silent mode will disable tracking of most stats." << endl;
	cout << "Pool Options:" << endl;
	cout << "  -o, -O <url>                   The miner will connect to this url" << endl;
	cout << "                                 You can specifiy an port after the url using -o url:port" << endl;
	cout << "  -u <string>                    The username (workername) used for login" << endl;
	cout << "  -p <string>                    The password used for login" << endl;
	cout << "Performance Options:" << endl;
	cout << "  -t <num>                       The number of threads for mining (default = detected cpu cores)" << endl;
	cout << "                                 For most efficient mining, set to number of CPU cores" << endl;
	cout << "  -s <num>                       Set MaxSieveSize range from 200000 - 10000000" << endl;
	cout << "                                 Default is 1500000." << endl;
	cout << "  -d <num>                       Set SievePercentage - range from 1 - 100" << endl;
	cout << "                                 Default is 15 and it's not recommended to use lower values than 8." << endl;
	cout << "                                 It limits how many base primes are used to filter out candidate multipliers in the sieve." << endl;
	cout << "  -r <num>                       Set RoundSievePercentage - range from 3 - 97" << endl;
	cout << "                                 The parameter determines how much time is spent running the sieve." << endl;
	cout << "                                 By default 80% of time is spent in the sieve and 20% is spent on checking the candidates produced by the sieve" << endl;
	cout << "  -primes <num>                  Sets how many prime factors are used to filter the sieve" << endl;
	cout << "                                 Default is MaxSieveSize. Valid range: 300 - 200000000" << endl;
	cout << "  -tune [true|false|1|0]         Enable Auto Tuning" << endl;
	cout << "  -ns <num>                      Null Share Timeout (Default: 0)" << endl;
	cout << "                                 After this many minutes with 0 shares, miner will exit. 0 to disable" << endl;
	cout << "Example usage:" << endl;
#ifndef _WIN32
	cout << "  jhprimeminer -o http://poolurl.com:10034 -u workername -p workerpass" << endl;
	cout << "  jhprimeminer -key abcd1234 -cs https://serverurl:8080 -sslnoverify true" << endl;
	cout << "  jhprimeminer -conf /path/to/config.file -quiet" << endl;
	cout << "  jhprimeminer        (will read config from ./jhprimeminer.conf)" << endl;
#else
	cout << "  jhPrimeminer.exe -o http://poolurl.com:8332 -u workername.1 -p workerpass -t 4" << endl;
	cout << "  jhprimeminer.exe -key abcd1234 -cs https://serverurl:8080 -sslnoverify true" << endl;
	cout << "  jhprimeminer.exe -conf /path/to/config.file -quiet" << endl;
	cout << "  jhprimeminer.exe        (will read config from ./jhprimeminer.conf)" << endl;
puts("Press any key to continue...");
	getchar();
#endif
}

void jhMiner_parseCommandline(int argc, char **argv)
{
	using namespace std;
	sint32 cIdx = 1;
	bool defaultconfig = true;
	while ( cIdx < std::min(argc,2) ){
		char* argument = argv[cIdx];
		cIdx++;
		if( memcmp(argument, "-conf", 6)==0){
			if( cIdx >= argc )
			{
				cout << "Missing filename after -conf option" << endl;
				exit(0);
			}
			commandlineInput.configfile = fStrDup(argv[cIdx], 64);
			cIdx++;
			defaultconfig = false;
		}
	}


	
		//attempt to load from config file
		std::ifstream configfile;
		configfile.open(commandlineInput.configfile);
		//test config file for readability	
		if(configfile.is_open()){
			//load config from file	
			std::string config(std::istreambuf_iterator<char>(configfile.rdbuf()), std::istreambuf_iterator<char>());
			if(!loadConfigJSON(config,false)){
				std::cout << "Failed to parse config file: " << commandlineInput.configfile << std::endl;
				exit(0);
			}
		}else{
			if(!defaultconfig){
				cout << "Failed to read config file: " << commandlineInput.configfile << endl;
				exit(0);
			}
		}


	

	//parse rest if the commandline options
	cIdx = 1;
	while( cIdx < argc )
	{
		char* argument = argv[cIdx];
		cIdx++;
		if( memcmp(argument, "-o", 3)==0 || memcmp(argument, "-O", 3)==0 )
		{
			// -o
			if( cIdx >= argc )
			{
				cout << "Missing URL after -o option" << endl;
				exit(0);
			}
			if( strstr(argv[cIdx], "http://") )
				commandlineInput.host = fStrDup(strstr(argv[cIdx], "http://")+7);
			else
				commandlineInput.host = fStrDup(argv[cIdx]);
			char* portStr = strstr(commandlineInput.host, ":");
			if( portStr )
			{
				*portStr = '\0';
				commandlineInput.port = atoi(portStr+1);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-conf", 6)==0){
			//already loaded conf
			if(cIdx > 2){
				cout << "-conf parameter MUST be the first option." << endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-u", 3)==0 )
		{
			// -u
			if( cIdx >= argc )
			{
				cout << "Missing username/workername after -u option" << endl;
				exit(0);
			}
			commandlineInput.workername = fStrDup(argv[cIdx], 64);
			cIdx++;
		}
		else if( memcmp(argument, "-p", 3)==0 )
		{
			// -p
			if( cIdx >= argc )
			{
				cout << "Missing password after -p option" << endl;
				exit(0);
			}
			commandlineInput.workerpass = fStrDup(argv[cIdx], 64);
			cIdx++;
		}
		else if( memcmp(argument, "-t", 3)==0 )
		{
			// -t
			if( cIdx >= argc )
			{
				cout << "Missing thread number after -t option" << endl;
				exit(0);
			}
			commandlineInput.numThreads = atoi(argv[cIdx]);
			if( commandlineInput.numThreads < 1 || commandlineInput.numThreads > 128 )
			{
				cout << "-t parameter out of range" << endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-s", 3)==0 )
		{
			// -s
			if( cIdx >= argc )
			{
				cout << "Missing number after -s option" << endl;
				exit(0);
			}
			commandlineInput.sieveSize = atoi(argv[cIdx]);
         if(commandlineInput.sieveSize < 1024000) { commandlineInput.sieveSize=1024000; }
		 if(commandlineInput.sieveSize > 10240000) { commandlineInput.sieveSize=10240000; }
			cIdx++;
		}
		else if( memcmp(argument, "-d", 3)==0 )
		{
			// -s
			if( cIdx >= argc )
			{
				cout << "Missing number after -d option" << endl;
				exit(0);
			}
			commandlineInput.sievePercentage = atoi(argv[cIdx]);
			if( commandlineInput.sievePercentage < 1 || commandlineInput.sievePercentage > 100 )
			{
				cout << "-d parameter out of range, must be between 1 - 100" << endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-r", 3)==0 )
		{
			// -s
			if( cIdx >= argc )
			{
				cout << "Missing number after -r option" << endl;
				exit(0);
			}
			commandlineInput.roundSievePercentage = atoi(argv[cIdx]);
			if( commandlineInput.roundSievePercentage < 3 || commandlineInput.roundSievePercentage > 97 )
			{
				cout << "-r parameter out of range, must be between 3 - 97" << endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-primes", 8)==0 )
		{
			// -primes
			if( cIdx >= argc )
			{
				cout << "Missing number after -primes option" << endl;
				exit(0);
			}
			commandlineInput.sievePrimeLimit = atoi(argv[cIdx]);
			if( commandlineInput.sievePrimeLimit < 300 || commandlineInput.sievePrimeLimit > 200000000 )
			{
				cout << "-primes parameter out of range, must be between 300 - 200000000" << endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-c", 3)==0 )
		{
			// -c
			if( cIdx >= argc )
			{
				cout << "Missing number after -c option" << endl;
				exit(0);
			}
			commandlineInput.L1CacheElements = atoi(argv[cIdx]);
			if( commandlineInput.L1CacheElements < 300 || commandlineInput.L1CacheElements > 200000000  || commandlineInput.L1CacheElements % 32 != 0) 
			{
				cout << "-c parameter out of range, must be between 64000 - 2000000 and multiply of 32" << endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-m", 3)==0 )
		{
			// -primes
			if( cIdx >= argc )
			{
				cout << "Missing number after -m option" << endl;
				exit(0);
			}
			commandlineInput.primorialMultiplier = atoi(argv[cIdx]);
			if( commandlineInput.primorialMultiplier < 5 || commandlineInput.primorialMultiplier > 1009) 
			{
				cout << "-m parameter out of range, must be between 5 - 1009 and should be a prime number" << endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-tune", 6)==0 )
		{
         // -tune
			if( cIdx >= argc )
			{
            cout << "Missing flag after -tune option" << endl;
				exit(0);
			}
			if (memcmp(argument, "true", 5) == 0 ||  memcmp(argument, "1", 2) == 0)
				commandlineInput.enableCacheTunning = true;

			cIdx++;
		}
      else if( memcmp(argument, "-target", 8)==0 )
      {
         // -target
         if( cIdx >= argc )
         {
            cout << "Missing number after -target option" << endl;
            exit(0);
         }
         commandlineInput.targetOverride = atoi(argv[cIdx]);
         if(commandlineInput.targetOverride < 8) { commandlineInput.targetOverride = 8; }
		 if(commandlineInput.targetOverride > 100) { commandlineInput.targetOverride = 100; }
         cIdx++;
      }
      else if( memcmp(argument, "-bttarget", 10)==0 )
      {
         // -bttarget
         if( cIdx >= argc )
         {
            printf("Missing number after -bttarget option\n");
            exit(0);
         }
         commandlineInput.targetBTOverride = atoi(argv[cIdx]);
		 if(commandlineInput.targetBTOverride < 8) { commandlineInput.targetBTOverride = 8; }
		 if(commandlineInput.targetBTOverride > 100) { commandlineInput.targetBTOverride = 100; }
         cIdx++;
      }
      else if( memcmp(argument, "-primorial", 11)==0 )
      {
         // -primorial
         if( cIdx >= argc )
         {
            cout << "Missing number after -primorial option" << endl;
            exit(0);
         }
         commandlineInput.initialPrimorial = atoi(argv[cIdx]);
         if( commandlineInput.initialPrimorial < 11 || commandlineInput.initialPrimorial > 1000 )
         {
            cout << "-primorial parameter out of range, must be between 11 - 1000" << endl;
            exit(0);
         }
         cIdx++;
      }
	  else if( memcmp(argument, "-cs", 4)==0 )
		{
			// -cs
			if( cIdx >= argc )
			{
				cout << "Missing central server address after -cs option" << endl;
				exit(0);
			}


			if( strstr(argv[cIdx], "http://") )
				commandlineInput.centralServer = fStrDup(strstr(argv[cIdx], "http://")+7);
			else
				commandlineInput.centralServer = fStrDup(argv[cIdx]);
			const char* portStr = strstr(commandlineInput.centralServer, ":");
			if( portStr )
			{
			//	*portStr = '\0';
	//			commandlineInput.centralServerPort = atoi(portStr+1);
				commandlineInput.centralServerPort = atoi(portStr);
			}
			commandlineInput.csEnabled = true;
			cIdx++;
	  }
       else if( memcmp(argument, "-key", 5)==0 )
		{
			// -key
			if( cIdx >= argc )
			{
				cout << "Missing API key after -key option" << endl;
				exit(0);
			}
			commandlineInput.csApiKey = fStrDup(argv[cIdx], 128);
			commandlineInput.csEnabled = true;
			cIdx++;
      }
	  else if( memcmp(argument, "-debug", 6)==0 )
      {
         // -debug
         if( cIdx >= argc )
         {
            cout << "Missing flag after -debug option" << endl;
            exit(0);
         }
         if (memcmp(argument, "true", 5) == 0 ||  memcmp(argument, "1", 2) == 0)
            commandlineInput.printDebug = true;
         cIdx++;
      }
	  else if( memcmp(argument, "-sslnoverify", 13)==0 )
      {
		bool arg = false;
        // -sslnoverify doesnt need an argument.
        if( !(cIdx >= argc) && !memcmp(argument, "-", 1)){
			arg = true;
		}
		if(arg){
			if(memcmp(argument, "true", 5) == 0 ||  memcmp(argument, "1", 2) == 0){
				 commandlineInput.weakSSL = true;
				 cIdx++;
			 }
			else if(memcmp(argument, "false", 6) == 0 || memcmp(argument, "0", 2) == 0){
				commandlineInput.weakSSL = false;
				cIdx++;
			}else{
				cout << "Usage: -sslnoverify [true|false|1|0]" << endl;
				exit(0);
			}
		}else{
			commandlineInput.weakSSL = true;
		}
      }
		else if( memcmp(argument, "-se", 4)==0 )
		{
			// -target
			if( cIdx >= argc )
			{
				cout << "Missing number after -se option" << endl;
				exit(0);
			}
			commandlineInput.sieveExtensions = atoi(argv[cIdx]);
			if( commandlineInput.sieveExtensions <= 1 || commandlineInput.sieveExtensions > 15 )
			{
				cout << "-se parameter out of range, must be between 0 - 15" << endl;
				exit(0);
			}
			cIdx++;
		}
	  		else if( memcmp(argument, "-ns", 4)==0 )
		{
			// -target
			if( cIdx >= argc )
			{
				cout << "Missing number after -ns option" << endl;
				exit(0);
			}
			commandlineInput.sieveExtensions = atoi(argv[cIdx]);
			cIdx++;
		}

		else if( memcmp(argument, "-quiet", 7)==0 )
      {
		bool arg = false;
        // -sslnoverify doesnt need an argument.
        if( !(cIdx >= argc) && !memcmp(argument, "-", 1)){
			arg = true;
		}
		if(arg){
			if(memcmp(argument, "true", 5) == 0 ||  memcmp(argument, "1", 2) == 0){
				 commandlineInput.quiet = true;
				 cIdx++;
			 }
			else if(memcmp(argument, "false", 6) == 0 || memcmp(argument, "0", 2) == 0){
				commandlineInput.quiet = false;
				cIdx++;
			}else{
				cout << "Usage: -sslnoverify [true|false|1|0]" << endl;
				exit(0);
			}
		}else{
			commandlineInput.quiet = true;
		}
      }
	  else if( memcmp(argument, "-silent", 8)==0 )
      {
		bool arg = false;
        // -sslnoverify doesnt need an argument.
        if( !(cIdx >= argc) && !memcmp(argument, "-", 1)){
			arg = true;
		}
		if(arg){
			if(memcmp(argument, "true", 5) == 0 ||  memcmp(argument, "1", 2) == 0){
				 commandlineInput.silent = true;
				 cIdx++;
			 }
			else if(memcmp(argument, "false", 6) == 0 || memcmp(argument, "0", 2) == 0){
				commandlineInput.silent = false;
				cIdx++;
			}else{
				cout << "Usage: -sslnoverify [true|false|1|0]" << endl;
				exit(0);
			}
		}else{
			commandlineInput.silent = true;
		}
      }

		else if( memcmp(argument, "-help", 6)==0 || memcmp(argument, "--help", 7)==0 )
		{
			jhMiner_printHelp();
			exit(0);
		}
		else
		{
			cout << "'" << argument << "' is an unknown option." << endl;
			#ifdef _WIN32
				cout << "Type jhPrimeminer.exe -help for more info" << endl;
			#else
				cout << "Type jhPrimeminer -help for more info" << endl; 
			#endif
			exit(-1);
		}
	}
	if( argc <= 1 )
	{
		jhMiner_printHelp();
		exit(0);
	}
}



#ifdef _WIN32
typedef std::pair <DWORD, HANDLE> thMapKeyVal;
uint64 * threadHearthBeat;

static void watchdog_thread(std::map<DWORD, HANDLE> threadMap)
#else
static void *watchdog_thread(void *)
#endif
{
	uint32 maxTimeBetweenShareSubmit = commandlineInput.nullShareTimeout * 60 * 1000;		// Nice if it was a cmd line option, so it can be ajusted!
#ifdef _WIN32
	   	uint32 maxIdelTime = 10 * 1000;
		std::map <DWORD, HANDLE> :: const_iterator thMap_Iter;
#endif
	   while(true){
		   if (lastShareSubmit+maxTimeBetweenShareSubmit < getTimeMilliseconds()){
				// Something must be wrong, no accepted shares for a long time
				if(!commandlineInput.silent){
					std::cout << "Error - Watchdog - No accepted shares for too long!" << std::endl;
				}
#ifdef _WIN32
				if (!IsXptClientConnected())
					continue;
				uint64 currentTick = getTimeMilliseconds();
				for (int i = 0; i < threadMap.size(); i++){
					DWORD heartBeatTick = threadHearthBeat[i];
					if (currentTick - heartBeatTick > maxIdelTime){
						//restart the thread
						if(!commandlineInput.silent){
							std::cout << "Restarting thread " << i << std::endl;
						}
						//__try
						//{
							//HANDLE h = threadMap.at(i);
							thMap_Iter = threadMap.find(i);
							if (thMap_Iter != threadMap.end()){
								HANDLE h = thMap_Iter->second;
								TerminateThread( h, 0);
								Sleep(1000);
								CloseHandle(h);
								Sleep(1000);
								threadHearthBeat[i] = getTimeMilliseconds();
								threadMap.erase(thMap_Iter);
								h = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)jhMiner_workerThread_xpt, (LPVOID)i, 0, 0);
								SetThreadPriority(h, THREAD_PRIORITY_BELOW_NORMAL);
								threadMap.insert(thMapKeyVal(i,h));
							}
						/*}
						__except(EXCEPTION_EXECUTE_HANDLER)
						{
						}*/
					}
				}
#else
				//on linux just exit
				exit(-2000);
#endif
				Sleep( 10*1000);
			}
	}
}

void PrintCurrentSettings()
{
	using namespace std;
	unsigned long uptime = (getTimeMilliseconds() - primeStats.startTime);

	unsigned int days = uptime / (24 * 60 * 60 * 1000);
    uptime %= (24 * 60 * 60 * 1000);
    unsigned int hours = uptime / (60 * 60 * 1000);
    uptime %= (60 * 60 * 1000);
    unsigned int minutes = uptime / (60 * 1000);
    uptime %= (60 * 1000);
    unsigned int seconds = uptime / (1000);

	cout << endl << "--------------------------------------------------------------------------------"<< endl;
	cout << "Worker name (-u): " << commandlineInput.workername << endl;
if(commandlineInput.csEnabled)
	cout << "Central Server (-cs): " << commandlineInput.centralServer << endl;
	cout << "Number of mining threads (-t): " << commandlineInput.numThreads << endl;
	cout << "Sieve Size (-s): " << nMaxSieveSize << endl;
	cout << "Prime Limit (-primes): " << commandlineInput.sievePrimeLimit << endl;
	cout << "Primorial Multiplier (-m): " << primeStats.nPrimorialMultiplier << endl;
	cout << "L1CacheElements (-c): " << primeStats.nL1CacheElements << endl;
	cout << "Chain Length Target (-target): " << nOverrideTargetValue << endl;
	cout << "BiTwin Length Target (-bttarget): " << nOverrideBTTargetValue << endl;
	cout << "Sieve Extensions (-se): " << nSieveExtensions << endl;
	cout << "Total Runtime: " << days << " Days, " << hours << " Hours, " << minutes << " minutes, " << seconds << " seconds" << endl;
	cout << "Total Share Value submitted to the Pool: " << primeStats.fTotalSubmittedShareValue << endl;	
	cout << "--------------------------------------------------------------------------------" << endl;
}

bool appQuitSignal = false;

#ifdef _WIN32
static void input_thread(){
#else
void *input_thread(void *){
static struct termios oldt, newt;
    /*tcgetattr gets the parameters of the current terminal
    STDIN_FILENO will tell tcgetattr that it should write the settings
    of stdin to oldt*/
    tcgetattr( STDIN_FILENO, &oldt);
    /*now the settings will be copied*/
    newt = oldt;

    /*ICANON normally takes care that one line at a time will be processed
    that means it will return if it sees a "\n" or an EOF or an EOL*/
    newt.c_lflag &= ~(ICANON);          

    /*Those new settings will be set to STDIN
    TCSANOW tells tcsetattr to change attributes immediately. */
    tcsetattr( STDIN_FILENO, TCSANOW, &newt);


#endif

	while (true) {
		int input = getchar();
		switch (input) {
		case 'q': case 'Q': case 3: //case 27:
			appQuitSignal = true;
			Sleep(3200);
			std::exit(0);
#ifdef _WIN32
			return;
#else
			/*restore the old settings*/
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
	return 0;
#endif
			break;
		case 'h': case 'H':
			// explicit cast to ref removes g++ warning but might be dumb, dunno
			if (!PrimeTableGetPreviousPrime((unsigned int &) primeStats.nPrimorialMultiplier))
				error("PrimecoinMiner() : primorial decrement overflow");	
			std::cout << "Primorial Multiplier: " << primeStats.nPrimorialMultiplier << std::endl;
			break;
		case 'y': case 'Y':
			// explicit cast to ref removes g++ warning but might be dumb, dunno
			if (!PrimeTableGetNextPrime((unsigned int &)  primeStats.nPrimorialMultiplier))
				error("PrimecoinMiner() : primorial increment overflow");
			std::cout << "Primorial Multiplier: " << primeStats.nPrimorialMultiplier << std::endl;
			break;
		case 's': case 'S':			
			PrintCurrentSettings();
			break;
		case 'u': case 'U':
			if (nMaxSieveSize < 10000000)
				nMaxSieveSize += 100000;
			std::cout << "Sieve size: " << nMaxSieveSize << std::endl;
			break;
		case 'j': case 'J':
			if (nMaxSieveSize > 100000)
				nMaxSieveSize -= 100000;
			std::cout << "Sieve size: " << nMaxSieveSize << std::endl;
			break;
		case 't': case 'T':
			if( nRoundSievePercentage < 98)
				nRoundSievePercentage++;
			std::cout << "Round Sieve Percentage: " << nRoundSievePercentage << "%" << std::endl;
			break;
		case 'g': case 'G':
			if( nRoundSievePercentage > 2)
				nRoundSievePercentage--;
			std::cout << "Round Sieve Percentage: " << nRoundSievePercentage << "%" << std::endl;
			break;
		case 'w': case 'W':
			std::cout << "Writing Current Config To: " << commandlineInput.configfile;
			if(saveConfigJSON()){
				std::cout << ": Success" << std::endl;
			}else{
				std::cout << ": Failed" <<std::endl;
			}
			break;

		}
	}
#ifdef _WIN32
	return;
#else
	/*restore the old settings*/
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
    return 0;
#endif
}



/*
 * Mainloop when using xpt mode
 */
int jhMiner_main_xptMode()
{
	#ifdef _WIN32
	// start the Auto Tuning thread
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)input_thread, NULL, 0, 0);


   std::map<DWORD, HANDLE> threadMap;
   threadHearthBeat = (uint64 *)malloc( commandlineInput.numThreads * sizeof(uint64));
	// start threads
	for(sint32 threadIdx=0; threadIdx<commandlineInput.numThreads; threadIdx++)
	{
		HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)jhMiner_workerThread_xpt, (LPVOID)threadIdx, 0, 0);
		SetThreadPriority(hThread, THREAD_PRIORITY_BELOW_NORMAL);
      threadMap.insert(thMapKeyVal(threadIdx,hThread));
      threadHearthBeat[threadIdx] = GetTickCount();
	}
 CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)watchdog_thread, (LPVOID)&threadMap, 0, 0);

#else
	uint32_t totalThreads = commandlineInput.numThreads;
		if(commandlineInput.nullShareTimeout>0){
			totalThreads = commandlineInput.numThreads + 2;
		}else{
			totalThreads = commandlineInput.numThreads + 1;
		}

  pthread_t threads[totalThreads];
  // start the Auto Tuning thread
  
  pthread_create(&threads[commandlineInput.numThreads], NULL, input_thread, NULL);
if(commandlineInput.nullShareTimeout>0){
	pthread_create(&threads[commandlineInput.numThreads+1], NULL, watchdog_thread, NULL);
}
	pthread_attr_t threadAttr;
  pthread_attr_init(&threadAttr);
  // Set the stack size of the thread
  pthread_attr_setstacksize(&threadAttr, 120*1024);
  // free resources of thread upon return
  pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED);
  
  // start threads
	for(uint32 threadIdx=0; threadIdx<commandlineInput.numThreads; threadIdx++)
  {
	pthread_create(&threads[threadIdx], &threadAttr, jhMiner_workerThread_xpt, (void *)threadIdx);
  }
  pthread_attr_destroy(&threadAttr);
#endif
	// main thread, don't query work, just wait and process
	sint32 loopCounter = 0;
	uint32 xptWorkIdentifier = 0xFFFFFFFF;
//	uint64 time_multiAdjust = getTimeMilliseconds();
   //unsigned long lastFiveChainCount = 0;
   //unsigned long lastFourChainCount = 0;
	while( true )
	{
		if (appQuitSignal)
         return 0;
		// calculate stats every second tick
		if( loopCounter&1 )
		{
			double totalRunTime = (double)(getTimeMilliseconds() - primeStats.startTime);
			double statsPassedTime = (double)(getTimeMilliseconds() - primeStats.primeLastUpdate);
			if( statsPassedTime < 1.0 )
				statsPassedTime = 1.0; // avoid division by zero
			double primesPerSecond = (double)primeStats.primeChainsFound / (statsPassedTime / 1000.0);
			primeStats.primeChainsFound = 0;
         		float avgCandidatesPerRound = (double)primeStats.nCandidateCount / primeStats.nSieveRounds;
       			float sievesPerSecond = (double)primeStats.nSieveRounds / (statsPassedTime / 1000.0);
			primeStats.primeLastUpdate = getTimeMilliseconds();
         		primeStats.nCandidateCount = 0;
         		primeStats.nSieveRounds = 0;
         primeStats.primeChainsFound = 0;
			uint32 bestDifficulty = primeStats.bestPrimeChainDifficulty;
			primeStats.bestPrimeChainDifficulty = 0;
			float primeDifficulty = GetChainDifficulty(bestDifficulty);
			if( workData.workEntry[0].dataIsValid )
			{
            statsPassedTime = (double)(getTimeMilliseconds() - primeStats.blockStartTime);
            if( statsPassedTime < 1.0 )
               statsPassedTime = 1.0; // avoid division by zero
				primeStats.bestPrimeChainDifficultySinceLaunch = std::max<double>((double)primeStats.bestPrimeChainDifficultySinceLaunch, primeDifficulty);
				//double sharesPerHour = ((double)valid_shares / totalRunTime) * 3600000.0;
				float shareValuePerHour = primeStats.fShareValue / totalRunTime * 3600000.0;
     		if(!commandlineInput.silent && !commandlineInput.quiet){
				std::cout << "Val/h: " << shareValuePerHour << " - PPS: " << (sint32)primesPerSecond << " - SPS: " << sievesPerSecond << " - ACC: " << (sint32)avgCandidatesPerRound << std::endl;
				std::cout << " Chain/Hr:  ";
				for(int i=6; i<=std::max(6,(int)primeStats.bestPrimeChainDifficultySinceLaunch); i++){
	            		   std::cout << i << ": " <<  std::setprecision(2) << (((double)primeStats.chainCounter[0][i] / statsPassedTime) * 3600000.0) << " ";
				}
				std::cout << std::setprecision(8);
				std::cout << std::endl;
			}
			}
		}
		// wait and check some stats
		uint64 time_updateWork = getTimeMilliseconds();
		while( true )
		{
			uint64 tickCount = getTimeMilliseconds();
			uint64 passedTime = tickCount - time_updateWork;


			if( passedTime >= 4000 )
				break;
			xptClient_process(workData.xptClient);
			char* disconnectReason = false;
			if( workData.xptClient == NULL || xptClient_isDisconnected(workData.xptClient, &disconnectReason) )
			{
				// disconnected, mark all data entries as invalid
				for(uint32 i=0; i<128; i++)
					workData.workEntry[i].dataIsValid = false;
     		if(!commandlineInput.silent && !commandlineInput.quiet){
				std::cout << "xpt: Disconnected, auto reconnect in 30 seconds"<<std::endl;
			}
				if( workData.xptClient && disconnectReason )
			     	if(!commandlineInput.silent && !commandlineInput.quiet){
						std::cout << "xpt: Disconnect reason: " << disconnectReason << std::endl;
					}
				Sleep(30*1000);
				if( workData.xptClient )
					xptClient_free(workData.xptClient);
				xptWorkIdentifier = 0xFFFFFFFF;
				while( true )
				{
					workData.xptClient = xptClient_connect(&jsonRequestTarget, commandlineInput.numThreads);
					if( workData.xptClient )
						break;
				}
			}
			// has the block data changed?
			if( workData.xptClient && xptWorkIdentifier != workData.xptClient->workDataCounter )
			{
				// printf("New work\n");
				xptWorkIdentifier = workData.xptClient->workDataCounter;
				for(uint32 i=0; i<workData.xptClient->payloadNum; i++)
				{
					uint8 blockData[256];
					memset(blockData, 0x00, sizeof(blockData));
					*(uint32*)(blockData+0) = workData.xptClient->blockWorkInfo.version;
					memcpy(blockData+4, workData.xptClient->blockWorkInfo.prevBlock, 32);
					memcpy(blockData+36, workData.xptClient->workData[i].merkleRoot, 32);
					*(uint32*)(blockData+68) = workData.xptClient->blockWorkInfo.nTime;
					*(uint32*)(blockData+72) = workData.xptClient->blockWorkInfo.nBits;
					*(uint32*)(blockData+76) = 0; // nonce
					memcpy(workData.workEntry[i].data, blockData, 80);
					((serverData_t*)workData.workEntry[i].serverData)->blockHeight = workData.xptClient->blockWorkInfo.height;
					((serverData_t*)workData.workEntry[i].serverData)->nBitsForShare = workData.xptClient->blockWorkInfo.nBitsShare;

					// is the data really valid?
					if( workData.xptClient->blockWorkInfo.nTime > 0 )
						workData.workEntry[i].dataIsValid = true;
					else
						workData.workEntry[i].dataIsValid = false;
				}
				if (workData.xptClient->blockWorkInfo.height > 0)
				{
//					double totalRunTime = (double)(getTimeMilliseconds() - primeStats.startTime);  unused?
					double statsPassedTime = (double)(getTimeMilliseconds() - primeStats.primeLastUpdate);
               if( statsPassedTime < 1.0 ) statsPassedTime = 1.0; // avoid division by zero
					double poolDiff = GetPrimeDifficulty( workData.xptClient->blockWorkInfo.nBitsShare);
					double blockDiff = GetPrimeDifficulty( workData.xptClient->blockWorkInfo.nBits);
					if(!commandlineInput.silent && commandlineInput.quiet){
						std::cout << "New Block: " << workData.xptClient->blockWorkInfo.height << " - Diff: " << blockDiff << " / " << poolDiff << std::endl;
					}
					
					if(!commandlineInput.silent && !commandlineInput.quiet){
						std::cout << std::endl << "--------------------------------------------------------------------------------" << std::endl;
						std::cout << "New Block: " << workData.xptClient->blockWorkInfo.height << " - Diff: " << blockDiff << " / " << poolDiff << std::endl;
						std::cout << "Valid/Total shares: [ " << valid_shares << " / " << total_shares << " ]  -  Max diff: " << primeStats.bestPrimeChainDifficultySinceLaunch << std::endl;
               statsPassedTime = (double)(getTimeMilliseconds() - primeStats.blockStartTime);

               if( statsPassedTime < 1.0 ) statsPassedTime = 1.0; // avoid division by zero
               for (int i = 6; i <= std::max(6,(int)primeStats.bestPrimeChainDifficultySinceLaunch); i++)
               {
                  double sharePerHour = ((double)primeStats.chainCounter[0][i] / statsPassedTime) * 3600000.0;
                  std::cout << i << "ch/h: " << sharePerHour << " - " << primeStats.chainCounter[0][i] << " [ " << primeStats.chainCounter[1][i] << " / " << primeStats.chainCounter[2][i] << " / " << primeStats.chainCounter[3][i] << " ]" << std::endl;
               }
               std::cout << "Share Value submitted - Last Block/Total: " << primeStats.fBlockShareValue << " / " << primeStats.fTotalSubmittedShareValue << std::endl;
               std::cout << "Current Primorial Value: " << primeStats.nPrimorialMultiplier << std::endl;
               std::cout << "--------------------------------------------------------------------------------" << std::endl;
			}
			if(commandlineInput.csEnabled)
			   csNotifyStats();
					primeStats.fBlockShareValue = 0;
						multiplierSet.clear();
				}
			}
			Sleep(10);
		}
		loopCounter++;
	}

	return 0;
}

int main(int argc, char **argv)
{
	// setup some default values
	commandlineInput.port = 10034;
	commandlineInput.numThreads = std::max(getNumThreads(), 1);
	commandlineInput.sieveSize = 1000000; // default maxSieveSize
	commandlineInput.sievePercentage = 10; // default 
	commandlineInput.roundSievePercentage = 70; // default 
	commandlineInput.enableCacheTunning = false;
	commandlineInput.L1CacheElements = 256000;
	commandlineInput.primorialMultiplier = 0; // for default 0 we will switch auto tune on
	commandlineInput.targetOverride = 9;
	commandlineInput.targetBTOverride = 10;
    commandlineInput.initialPrimorial = 41;
	commandlineInput.printDebug = 0;
	commandlineInput.centralServer = "xpm.tandyuk.com";
	commandlineInput.centralServerPort = 80;
	commandlineInput.csEnabled = false;
	commandlineInput.sieveExtensions = 7;
	commandlineInput.maxPrimes = 16400;
	commandlineInput.weakSSL = false;
	commandlineInput.csUUID = NULL;
	commandlineInput.sievePrimeLimit = 0;
	commandlineInput.configfile = "jhprimeminer.conf";
	commandlineInput.quiet = false;
	commandlineInput.silent = false;
	commandlineInput.nullShareTimeout = 0; //disabled by default
	commandlineInput.csStaticUUID = false;

	std::cout << std::fixed << std::showpoint << std::setprecision(8);


	// parse command lines and config file
	jhMiner_parseCommandline(argc, argv);

	if(commandlineInput.csEnabled)
		csNotifySettings(false);


	// Sets max sieve size
   nMaxSieveSize = ceil(commandlineInput.sieveSize/1024000)*1024000;
   nSieveExtensions = commandlineInput.sieveExtensions;

   commandlineInput.targetBTOverride = ceil(commandlineInput.targetBTOverride/2)*2;
   vPrimesAvg = ((commandlineInput.targetOverride+commandlineInput.targetBTOverride)/2);
   if (vPrimesAvg!=10) { vPrimesAdj = pow(1.3,((10-vPrimesAvg)*2)); } else { vPrimesAdj = 1; }
   vPrimesMult = 41.0 * (1+(1.0*2 ));
   nMaxPrimes = vPrimesMult * commandlineInput.initialPrimorial * vPrimesAdj;
	nOverrideTargetValue = commandlineInput.targetOverride;
	nOverrideBTTargetValue = commandlineInput.targetBTOverride;

	if (commandlineInput.sievePrimeLimit == 0) //default before parsing 
		commandlineInput.sievePrimeLimit = commandlineInput.sieveSize;  //default is sieveSize 
	primeStats.nL1CacheElements = commandlineInput.L1CacheElements;



	if( commandlineInput.host == NULL){
		if( commandlineInput.csApiKey == NULL){
			std::cout << "Missing required -o or -key option" << std::endl;
		}else{
			std::cout << "Missing required -o option, and failed to connect to central server" << std::endl;
		}
		exit(-1);	
	}

	//CRYPTO_set_mem_ex_functions(mallocEx, reallocEx, freeEx);
	if(!commandlineInput.silent && !commandlineInput.quiet){	
	std::cout << 
	" ============================================================================ " << std::endl <<
	"|  jhPrimeMiner - mod by rdebourbon -v3.3beta                     |" << std::endl <<
	"|     optimised from hg5fm (mumus) v7.1 build + HP10 updates      |" << std::endl <<
	"|     jsonrpc stats and remote config added by tandyuk            |" << std::endl <<
	"|  author: JH (http://ypool.net)                                  |" << std::endl <<
	"|  contributors: x3maniac                                         |" << std::endl <<
	"|  Credits: Sunny King for the original Primecoin client&miner    |" << std::endl <<
	"|  Credits: mikaelh for the performance optimizations             |" << std::endl <<
	"|  Credits: erkmos for the original linux port                    |" << std::endl <<
	"|  Credits: tandyuk for the linux build of rdebourbons mod        |" << std::endl <<
	"|                                                                 |" << std::endl <<
	"|  Donations (XPM):                                               |" << std::endl <<
	"|    JH: AQjz9cAUZfjFgHXd8aTiWaKKbb3LoCVm2J                       |" << std::endl <<
	"|    rdebourbon: AUwKMCYCacE6Jq1rsLcSEHSNiohHVVSiWv               |" << std::endl <<
	"|    tandyuk: AYwmNUt6tjZJ1nPPUxNiLCgy1D591RoFn4                  |" << std::endl <<
	" ============================================================================ " << std::endl <<
	"Launching miner..." << std::endl;
	}
	// set priority lower so the user still can do other things
#ifdef _WIN32
	SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
#endif
	// init memory speedup (if not already done in preMain)
	//mallocSpeedupInit();
	if( pctx == NULL )
		pctx = BN_CTX_new();
	// init prime table
	GeneratePrimeTable(commandlineInput.sievePrimeLimit);
	// init winsock
#ifdef WIN32
	WSADATA wsa;
	WSAStartup(MAKEWORD(2,2),&wsa);
	// init critical section
	InitializeCriticalSection(&workData.cs);
#else
  pthread_mutex_init(&workData.cs, NULL);
#endif
	// connect to host
#ifdef _WIN32
	hostent* hostInfo = gethostbyname(commandlineInput.host);
	if( hostInfo == NULL )
	{
		printf("Cannot resolve '%s'. Is it a valid URL?\n", commandlineInput.host);
		exit(-1);
	}
	void** ipListPtr = (void**)hostInfo->h_addr_list;
	uint32 ip = 0xFFFFFFFF;
	if( ipListPtr[0] )
	{
		ip = *(uint32*)ipListPtr[0];
	}
	char ipText[32];
	esprintf(ipText, "%d.%d.%d.%d", ((ip>>0)&0xFF), ((ip>>8)&0xFF), ((ip>>16)&0xFF), ((ip>>24)&0xFF));
#else
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  getaddrinfo(commandlineInput.host, 0, &hints, &res);
  char ipText[32];
  inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr, ipText, INET_ADDRSTRLEN);
#endif
  
	// setup RPC connection data (todo: Read from command line)
	jsonRequestTarget.ip = ipText;
	jsonRequestTarget.port = commandlineInput.port;
	jsonRequestTarget.authUser = (char *)commandlineInput.workername;
	jsonRequestTarget.authPass = (char *)commandlineInput.workerpass;
	if(!commandlineInput.silent){	
		std::cout << "Connecting to '" << commandlineInput.host << "'" << std::endl;
	}



	if(commandlineInput.csEnabled && !commandlineInput.silent){	
		std::cout << "Using '" << commandlineInput.centralServer << "' for stats output and remote config" << std::endl;
	}

	// init stats
   primeStats.primeLastUpdate = primeStats.blockStartTime = primeStats.startTime = getTimeMilliseconds();
	primeStats.shareFound = false;
	primeStats.shareRejected = false;
	primeStats.primeChainsFound = 0;
	primeStats.foundShareCount = 0;
   for(uint32 i = 0; i < sizeof(primeStats.chainCounter[0])/sizeof(uint32);  i++)
   {
      primeStats.chainCounter[0][i] = 0;
      primeStats.chainCounter[1][i] = 0;
      primeStats.chainCounter[2][i] = 0;
      primeStats.chainCounter[3][i] = 0;
   }
	primeStats.fShareValue = 0;
	primeStats.fBlockShareValue = 0;
	primeStats.fTotalSubmittedShareValue = 0;
   primeStats.nPrimorialMultiplier = commandlineInput.initialPrimorial;
	primeStats.nWaveTime = 0;
	primeStats.nWaveRound = 0;

	// setup thread count and print info
	if(!commandlineInput.silent && !commandlineInput.quiet){

	std::cout << "Using " << commandlineInput.numThreads << " threads" << std::endl;
	std::cout << "Username: " << jsonRequestTarget.authUser << std::endl;
	std::cout << "Password: " << jsonRequestTarget.authPass << std::endl;
	}
	// decide protocol
	// port 10034 indicates xpt protocol (in future we will also add a -o URL prefix)
	workData.protocolMode = MINER_PROTOCOL_XPUSHTHROUGH;
	if(!commandlineInput.silent && !commandlineInput.quiet)
		std::cout << "Using x.pushthrough protocol" << std::endl;
	

		// initial query new work / create new connection
		workData.xptClient = NULL;
		// x.pushthrough initial connect & login sequence
		while( true )
		{
			// repeat connect & login until it is successful (with 30 seconds delay)
			while ( true )
			{
				workData.xptClient = xptClient_connect(&jsonRequestTarget, commandlineInput.numThreads);
				if( workData.xptClient != NULL )
					break;
					if(!commandlineInput.silent){	
						std::cout << "Failed to connect, retry in 30 seconds" << std::endl;
					}
				Sleep(1000*30);
			}
			// make sure we are successfully authenticated
			while( xptClient_isDisconnected(workData.xptClient, NULL) == false && xptClient_isAuthenticated(workData.xptClient) == false )
			{
				xptClient_process(workData.xptClient);
				Sleep(1);
			}
			char* disconnectReason = NULL;
			// everything went alright?
			if( xptClient_isDisconnected(workData.xptClient, &disconnectReason) == true )
			{
				xptClient_free(workData.xptClient);
				workData.xptClient = NULL;
				break;
			}
			if( xptClient_isAuthenticated(workData.xptClient) == true )
			{
				break;
			}
			if( disconnectReason ){
				if(!commandlineInput.silent){	
					std::cout << "xpt error: " << disconnectReason << std::endl;
				}
			}
			// delete client
			xptClient_free(workData.xptClient);
			// try again in 30 seconds
			if(!commandlineInput.silent && !commandlineInput.quiet){	
				std::cout << "x.pushthrough authentication sequence failed, retry in 30 seconds" << std::endl;
			}
		Sleep(30*1000);
	}
	


	if(!commandlineInput.silent && !commandlineInput.quiet){
		std::cout << "\nVal/h = 'Share Value per Hour', PPS = 'Primes per Second'," <<std::endl <<
		"SPS = 'Sieves per Second', ACC = 'Avg. Candidate Count / Sieve' " << std::endl <<
		"===============================================================" << std::endl <<
		"Keyboard shortcuts:" << std::endl <<
		"   <Ctrl-C>, <Q>     - Quit" << std::endl <<
		"   <Y> - Increment Primorial Multiplier" << std::endl <<
		"   <H> - Decrement Primorial Multiplier" << std::endl <<
		"   <U> - Increment Sieve size" << std::endl <<
		"   <J> - Decrement Sive size" << std::endl <<
		"   <R> - Increment Sieve Percentage" << std::endl <<
		"   <F> - Decrement Sieve Percentage" << std::endl <<
		"   <T> - Increment Round Sieve Percentage" << std::endl <<
		"   <G> - Decrement Round Sieve Percentage" << std::endl <<
		"   <P> - Enable/Disable Round Sieve Percentage Auto Tuning" << std::endl <<
		"   <S> - Print current settings" << std::endl <<
		"   <W> - Write current settings to config file" << std::endl;
		if( commandlineInput.enableCacheTunning ){
			std::cout << "Note: While the initial auto tuning is in progress several values cannot be changed." << std::endl;
		}
	}	

	// remove support for getwork completely.
		return jhMiner_main_xptMode();

	return 0;
}
