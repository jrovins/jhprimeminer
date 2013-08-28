#include"global.h"
#include "ticker.h"
//#include<intrin.h>
#include<ctime>
#include<map>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <termios.h>            //termios, TCSANOW, ECHO, ICANON
#include <unistd.h>     //STDIN_FILENO


//used for get_num_cpu
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

primeStats_t primeStats = {0};
commandlineInput_t commandlineInput ={0};
volatile int total_shares = 0;
volatile int valid_shares = 0;
unsigned int nMaxSieveSize;
unsigned int nSievePercentage;
bool nPrintDebugMessages;
unsigned long nOverrideTargetValue;
unsigned int nOverrideBTTargetValue;
unsigned int nTarget;
char* dt;

bool error(const char *format, ...)
{
	puts(format);
	return false;
}

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

bool hex2bin(unsigned char *p, const char *hexstr, size_t len)
{
	bool ret = false;

	while (*hexstr && len) {
		char hex_byte[4];
		unsigned int v;

		if (!hexstr[1]) {
			printf("hex2bin str truncated");
			return ret;
		}

		memset(hex_byte, 0, 4);
		hex_byte[0] = hexstr[0];
		hex_byte[1] = hexstr[1];

		if (sscanf(hex_byte, "%x", &v) != 1) {
			printf( "hex2bin sscanf '%s' failed", hex_byte);
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

typedef struct  
{
	bool dataIsValid;
	uint8 data[128];
	uint32 dataHash; // used to detect work data changes
	uint8 serverData[32]; // contains data from the server 
}workDataEntry_t;

typedef struct  
{
#ifdef _WIN32
	CRITICAL_SECTION cs;
#else
  pthread_mutex_t cs;
#endif
	uint8 protocolMode;
	// xpm
	workDataEntry_t workEntry[128]; // work data for each thread (up to 128)
	// x.pushthrough
	xptClient_t* xptClient;
}workData_t;

#define MINER_PROTOCOL_GETWORK		(1)
#define MINER_PROTOCOL_STRATUM		(2)
#define MINER_PROTOCOL_XPUSHTHROUGH	(3)

workData_t workData;

jsonRequestTarget_t jsonRequestTarget; // rpc login data
jsonRequestTarget_t statsRequestTarget; // rpc login data
jsonRequestTarget_t jsonLocalPrimeCoin; // rpc login data
bool useLocalPrimecoindForLongpoll;


/*
 * Pushes the found block data to the server for giving us the $$$
 * Uses getwork to push the block
 * Returns true on success
 * Note that the primecoin data can be larger due to the multiplier at the end, so we use 256 bytes per default
 */
bool jhMiner_pushShare_primecoin(uint8 data[256], primecoinBlock_t* primecoinBlock)
{
	if( workData.protocolMode == MINER_PROTOCOL_GETWORK )
	{
		// prepare buffer to send
		fStr_buffer4kb_t fStrBuffer_parameter;
		fStr_t* fStr_parameter = fStr_alloc(&fStrBuffer_parameter, FSTR_FORMAT_UTF8);
		fStr_append(fStr_parameter, "[\""); // \"]
		fStr_addHexString(fStr_parameter, data, 256);
		fStr_appendFormatted(fStr_parameter, "\",\"");
		fStr_addHexString(fStr_parameter, (uint8*)&primecoinBlock->serverData, 32);
		fStr_append(fStr_parameter, "\"]");
		// send request
		sint32 rpcErrorCode = 0;
		jsonObject_t* jsonReturnValue = jsonClient_request(&jsonRequestTarget, "getwork", fStr_parameter, &rpcErrorCode);
		if( jsonReturnValue == NULL )
		{
			printf("PushWorkResult failed :(\n");
			return false;
		}
		else
		{
			// rpc call worked, sooooo.. is the server happy with the result?
			jsonObject_t* jsonReturnValueBool = jsonObject_getParameter(jsonReturnValue, "result");
			if( jsonObject_isTrue(jsonReturnValueBool) )
			{
				total_shares++;
				valid_shares++;
				time_t now = time(0);
				dt = ctime(&now);
				//printf("Valid share found!");
				//printf("[ %d / %d ] %s",valid_shares, total_shares,dt);
				jsonObject_freeObject(jsonReturnValue);
				return true;
			}
			else
			{
				total_shares++;
				// the server says no to this share :(
				printf("Server rejected share (BlockHeight: %u/%u nBits: 0x%08uX)\n", primecoinBlock->serverData.blockHeight, jhMiner_getCurrentWorkBlockHeight(primecoinBlock->threadIndex), primecoinBlock->serverData.client_shareBits);
				jsonObject_freeObject(jsonReturnValue);
				return false;
			}
		}
		jsonObject_freeObject(jsonReturnValue);
		return false;
	}
	else if( workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH )
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
			return true;
		}
		else
		{
			printf("Share submission failed. The client is not connected to the pool.\n");
			return false;
		}
	}
	return false;
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


void jhMiner_queryWork_primecoin()
{
	sint32 rpcErrorCode = 0;
	//uint32 time1 = GetTickCount();
	jsonObject_t* jsonReturnValue = jsonClient_request(&jsonRequestTarget, "getwork", NULL, &rpcErrorCode);
	//uint32 time2 = GetTickCount() - time1;
	// printf("request time: %dms\n", time2);
	if( jsonReturnValue == NULL )
	{
		printf("Getwork() failed with %serror code %d\n", (rpcErrorCode>1000)?"http ":"", rpcErrorCode>1000?rpcErrorCode-1000:rpcErrorCode);
		workData.workEntry[0].dataIsValid = false;
		return;
	}
	else
	{
		jsonObject_t* jsonResult = jsonObject_getParameter(jsonReturnValue, "result");
		jsonObject_t* jsonResult_data = jsonObject_getParameter(jsonResult, "data");
		//jsonObject_t* jsonResult_hash1 = jsonObject_getParameter(jsonResult, "hash1");
//		jsonObject_t* jsonResult_target = jsonObject_getParameter(jsonResult, "target");   unused?
		jsonObject_t* jsonResult_serverData = jsonObject_getParameter(jsonResult, "serverData");
		//jsonObject_t* jsonResult_algorithm = jsonObject_getParameter(jsonResult, "algorithm");
		if( jsonResult_data == NULL )
		{
			printf("Error :(\n");
			workData.workEntry[0].dataIsValid = false;
			jsonObject_freeObject(jsonReturnValue);
			return;
		}
		// data
		uint32 stringData_length = 0;
		uint8* stringData_data = jsonObject_getStringData(jsonResult_data, &stringData_length);
		//printf("data: %.*s...\n", (sint32)min(48, stringData_length), stringData_data);
#ifdef _WIN32
		EnterCriticalSection(&workData.cs);
#else
    pthread_mutex_lock(&workData.cs);
#endif
		yPoolWorkMgr_parseHexString((char*)stringData_data, std::min<unsigned long>(128*2, stringData_length), workData.workEntry[0].data);
		workData.workEntry[0].dataIsValid = true;
		// get server data
		uint32 stringServerData_length = 0;
		uint8* stringServerData_data = jsonObject_getStringData(jsonResult_serverData, &stringServerData_length);
		memset(workData.workEntry[0].serverData, 0, 32);
		if( jsonResult_serverData )
			yPoolWorkMgr_parseHexString((char*)stringServerData_data, std::min(128*2, 32*2), workData.workEntry[0].serverData);
		// generate work hash
		uint32 workDataHash = 0x5B7C8AF4;
		for(uint32 i=0; i<stringData_length/2; i++)
		{
			workDataHash = (workDataHash>>29)|(workDataHash<<3);
			workDataHash += (uint32)workData.workEntry[0].data[i];
		}
		workData.workEntry[0].dataHash = workDataHash;
#ifdef _WIN32
		LeaveCriticalSection(&workData.cs);
#else
    pthread_mutex_unlock(&workData.cs);
#endif
		jsonObject_freeObject(jsonReturnValue);
	}
}

/*
 * Returns the block height of the most recently received workload
 */
uint32 jhMiner_getCurrentWorkBlockHeight(sint32 threadIndex)
{
	if( workData.protocolMode == MINER_PROTOCOL_GETWORK )
		return ((serverData_t*)workData.workEntry[0].serverData)->blockHeight;	
	else
		return ((serverData_t*)workData.workEntry[threadIndex].serverData)->blockHeight;
}

/*
 * Worker thread mainloop for getwork() mode
 */
#ifdef _WIN32
int jhMiner_workerThread_getwork(int threadIndex)
{
#else
void* jhMiner_workerThread_getwork(void *arg)
{
  uint32_t threadIndex = static_cast<uint32_t>((uintptr_t)arg);
#endif
	while( true )
	{
		uint8 localBlockData[128];
		// copy block data from global workData
//		uint32 workDataHash = 0;  unused?
		uint8 serverData[32];
		while( workData.workEntry[0].dataIsValid == false ) Sleep(200);
#ifdef _WIN32
		EnterCriticalSection(&workData.cs);
#else
    pthread_mutex_lock(&workData.cs);
#endif
		memcpy(localBlockData, workData.workEntry[0].data, 128);
		//seed = workData.seed;
		memcpy(serverData, workData.workEntry[0].serverData, 32);
#ifdef _WIN32
		LeaveCriticalSection(&workData.cs);
#else
    pthread_mutex_unlock(&workData.cs);
#endif
		// swap endianess
		for(uint32 i=0; i<128/4; i++)
		{
			*(uint32*)(localBlockData+i*4) = _swapEndianessU32(*(uint32*)(localBlockData+i*4));
		}
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
		BitcoinMiner(&primecoinBlock, threadIndex);
		primecoinBlock.mpzPrimeChainMultiplier = 0;
	}
	return 0;
}


/*
 * Worker thread mainloop for xpt() mode
 */
#ifdef _WIN32
int jhMiner_workerThread_xpt(int threadIndex)
{
#else
void *jhMiner_workerThread_xpt(void *arg)
{
  uint32_t threadIndex = static_cast<uint32_t>((uintptr_t)arg);
#endif
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
      		BitcoinMiner(&primecoinBlock, threadIndex);
		//printf("Mining stopped after %dms\n", GetTickCount()-time1);
		primecoinBlock.mpzPrimeChainMultiplier = 0;
	}
	return 0;
}



void jhMiner_printHelp()
{
	puts("Usage: jhPrimeminer.exe [options]");
	puts("Options:");
	puts("   -o, -O                        The miner will connect to this url");
	puts("                                 You can specifiy an port after the url using -o url:port");
	puts("   -u                            The username (workername) used for login");
	puts("   -p                            The password used for login");
	puts("   -t <num>                      The number of threads for mining (default 1)");
	puts("                                     For most efficient mining, set to number of CPU cores");
	puts("   -s <num>                      Set MaxSieveSize range from 200000 - 10000000");
	puts("                                     Default is 1500000.");
	puts("   -d <num>                      Set SievePercentage - range from 1 - 100");
	puts("                                     Default is 15 and it's not recommended to use lower values than 8.");
	puts("                                     It limits how many base primes are used to filter out candidate multipliers in the sieve.");
	puts("   -r <num>                      Set RoundSievePercentage - range from 3 - 97");
	puts("                                     The parameter determines how much time is spent running the sieve.");
	puts("                                     By default 80% of time is spent in the sieve and 20% is spent on checking the candidates produced by the sieve");
	puts("   -primes <num>                 Sets how many prime factors are used to filter the sieve");
	puts("                                     Default is MaxSieveSize. Valid range: 300 - 200000000");

	puts("Example usage:");
	puts("   jhPrimeminer.exe -o http://poolurl.com:8332 -u workername.1 -p workerpass -t 4");
#ifdef _WIN32
	puts("Press any key to continue...");
	getchar();
#endif
}

void jhMiner_parseCommandline(int argc, char **argv)
{
	sint32 cIdx = 1;
	while( cIdx < argc )
	{
		char* argument = argv[cIdx];
		cIdx++;
		if( memcmp(argument, "-o", 3)==0 || memcmp(argument, "-O", 3)==0 )
		{
			// -o
			if( cIdx >= argc )
			{
				printf("Missing URL after -o option\n");
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
		else if( memcmp(argument, "-u", 3)==0 )
		{
			// -u
			if( cIdx >= argc )
			{
				printf("Missing username/workername after -u option\n");
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
				printf("Missing password after -p option\n");
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
				printf("Missing thread number after -t option\n");
				exit(0);
			}
			commandlineInput.numThreads = atoi(argv[cIdx]);
			if( commandlineInput.numThreads < 1 || commandlineInput.numThreads > 128 )
			{
				printf("-t parameter out of range");
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-s", 3)==0 )
		{
			// -s
			if( cIdx >= argc )
			{
				printf("Missing number after -s option\n");
				exit(0);
			}
			commandlineInput.sieveSize = atoi(argv[cIdx]);
			if( commandlineInput.sieveSize < 200000 || commandlineInput.sieveSize > 40000000 )
			{
				printf("-s parameter out of range, must be between 200000 - 10000000");
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-d", 3)==0 )
		{
			// -s
			if( cIdx >= argc )
			{
				printf("Missing number after -d option\n");
				exit(0);
			}
			commandlineInput.sievePercentage = atoi(argv[cIdx]);
			if( commandlineInput.sievePercentage < 1 || commandlineInput.sievePercentage > 100 )
			{
				printf("-d parameter out of range, must be between 1 - 100");
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-r", 3)==0 )
		{
			// -s
			if( cIdx >= argc )
			{
				printf("Missing number after -r option\n");
				exit(0);
			}
			commandlineInput.roundSievePercentage = atoi(argv[cIdx]);
			if( commandlineInput.roundSievePercentage < 3 || commandlineInput.roundSievePercentage > 97 )
			{
				printf("-r parameter out of range, must be between 3 - 97");
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-primes", 8)==0 )
		{
			// -primes
			if( cIdx >= argc )
			{
				printf("Missing number after -primes option\n");
				exit(0);
			}
			commandlineInput.sievePrimeLimit = atoi(argv[cIdx]);
			if( commandlineInput.sievePrimeLimit < 300 || commandlineInput.sievePrimeLimit > 200000000 )
			{
				printf("-primes parameter out of range, must be between 300 - 200000000");
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-c", 3)==0 )
		{
			// -c
			if( cIdx >= argc )
			{
				printf("Missing number after -c option\n");
				exit(0);
			}
			commandlineInput.L1CacheElements = atoi(argv[cIdx]);
			if( commandlineInput.L1CacheElements < 300 || commandlineInput.L1CacheElements > 200000000  || commandlineInput.L1CacheElements % 32 != 0) 
			{
				printf("-c parameter out of range, must be between 64000 - 2000000 and multiply of 32");
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-m", 3)==0 )
		{
			// -primes
			if( cIdx >= argc )
			{
				printf("Missing number after -m option\n");
				exit(0);
			}
			commandlineInput.primorialMultiplier = atoi(argv[cIdx]);
			if( commandlineInput.primorialMultiplier < 5 || commandlineInput.primorialMultiplier > 1009) 
			{
				printf("-m parameter out of range, must be between 5 - 1009 and should be a prime number");
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-tune", 6)==0 )
		{
         // -tune
			if( cIdx >= argc )
			{
            printf("Missing flag after -tune option\n");
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
            printf("Missing number after -target option\n");
            exit(0);
         }
         commandlineInput.targetOverride = atoi(argv[cIdx]);
         if( commandlineInput.targetOverride < 0 || commandlineInput.targetOverride > 100 )
         {
            printf("-target parameter out of range, must be between 0 - 100");
            exit(0);
         }
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
         if( commandlineInput.targetBTOverride < 0 || commandlineInput.targetBTOverride > 100 )
         {
            printf("-bttarget parameter out of range, must be between 0 - 100");
            exit(0);
         }
         cIdx++;
      }
      else if( memcmp(argument, "-primorial", 11)==0 )
      {
         // -primorial
         if( cIdx >= argc )
         {
            printf("Missing number after -primorial option\n");
            exit(0);
         }
         commandlineInput.initialPrimorial = atoi(argv[cIdx]);
         if( commandlineInput.initialPrimorial < 11 || commandlineInput.initialPrimorial > 1000 )
         {
            printf("-primorial parameter out of range, must be between 11 - 1000");
            exit(0);
         }
         cIdx++;
      }
	  else if( memcmp(argument, "-cs", 4)==0 )
		{
			// -cs
			if( cIdx >= argc )
			{
				printf("Missing central server address after -cs option\n");
				exit(0);
			}


			if( strstr(argv[cIdx], "http://") )
				commandlineInput.centralServer = fStrDup(strstr(argv[cIdx], "http://")+7);
			else
				commandlineInput.centralServer = fStrDup(argv[cIdx]);
			char* portStr = strstr(commandlineInput.centralServer, ":");
			if( portStr )
			{
				*portStr = '\0';
				commandlineInput.centralServerPort = atoi(portStr+1);
			}
			commandlineInput.csEnabled = true;
			cIdx++;
	  }
       else if( memcmp(argument, "-key", 5)==0 )
		{
			// -key
			if( cIdx >= argc )
			{
				printf("Missing API key after -key option\n");
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
            printf("Missing flag after -debug option\n");
            exit(0);
         }
         if (memcmp(argument, "true", 5) == 0 ||  memcmp(argument, "1", 2) == 0)
            commandlineInput.printDebug = true;
         cIdx++;
      }
else if( memcmp(argument, "-se", 4)==0 )
		{
			// -target
			if( cIdx >= argc )
			{
				printf("Missing number after -se option\n");
				exit(0);
			}
			commandlineInput.sieveExtensions = atoi(argv[cIdx]);
			if( commandlineInput.sieveExtensions <= 1 || commandlineInput.sieveExtensions > 15 )
			{
				printf("-se parameter out of range, must be between 0 - 15\n");
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-help", 6)==0 || memcmp(argument, "--help", 7)==0 )
		{
			jhMiner_printHelp();
			exit(0);
		}
		else
		{
			printf("'%s' is an unknown option.\nType jhPrimeminer.exe --help for more info\n", argument); 
			exit(-1);
		}
	}
	if( argc <= 1 )
	{
		jhMiner_printHelp();
		exit(0);
	}
}



bool fIncrementPrimorial = true;

BYTE nRoundSievePercentage;
bool bOptimalL1SearchInProgress = false;

#ifdef _WIN32
static void CacheAutoTuningWorkerThread(bool bEnabled)
#else
void *CacheAutoTuningWorkerThread(void * arg)
#endif
{
#ifndef _WIN32
  bool bEnabled = static_cast<bool>((uintptr_t)arg);
#endif

#ifdef _WIN32
  if (bOptimalL1SearchInProgress)
		return 0;
#endif

	bOptimalL1SearchInProgress = true;
	
//	uint64_t startTime = getTimeMilliseconds();	
	unsigned int nL1CacheElementsStart = 8 * sizeof(unsigned long) * 1000;
	unsigned int nL1CacheElementsMax   = 2000000;
	unsigned int nL1CacheElementsIncrement = 8 * sizeof(unsigned long) * 1000;
	BYTE nSampleSeconds = 10;

	unsigned int nL1CacheElements = primeStats.nL1CacheElements;
	std::map <unsigned int, unsigned int> mL1Stat;
	std::map <unsigned int, unsigned int>::iterator mL1StatIter;
	typedef std::pair <unsigned int, unsigned int> KeyVal;
	if (bEnabled)
	primeStats.nL1CacheElements = nL1CacheElementsStart;
	
	long nCounter = 0;
	while (true && bEnabled && (xptClient_isDisconnected(workData.xptClient, NULL) == false))
	{		
		primeStats.nWaveTime = 0;
		primeStats.nWaveRound = 0;
		primeStats.nTestTime = 0;
		primeStats.nTestRound = 0;
		Sleep(nSampleSeconds*1000);
	//	uint32_t waveTime = primeStats.nWaveTime;
		nCounter ++;
		if (nCounter <=1) 
			continue;// wait a litle at the beginning
		if ( !xptClient_isDisconnected(workData.xptClient, NULL) ){
			bOptimalL1SearchInProgress = false;
			primeStats.nL1CacheElements = commandlineInput.L1CacheElements;
			return 0;
		}

		if (bOptimalL1SearchInProgress && nCounter >=1)
		{
		nL1CacheElements = primeStats.nL1CacheElements;
		mL1Stat.insert( KeyVal((uint32_t)primeStats.nL1CacheElements, (uint32_t)(primeStats.nWaveRound == 0 ? 0xFFFF : primeStats.nWaveTime / primeStats.nWaveRound)));
		if (nL1CacheElements < nL1CacheElementsMax)
			primeStats.nL1CacheElements += nL1CacheElementsIncrement;
		else
		{
			// set the best value
			DWORD minWeveTime = mL1Stat.begin()->second;
			unsigned int nOptimalSize = nL1CacheElementsStart;
			for (  mL1StatIter = mL1Stat.begin(); mL1StatIter != mL1Stat.end(); mL1StatIter++ )
			{
				if (mL1StatIter->second < minWeveTime)
				{
					minWeveTime = mL1StatIter->second;
					nOptimalSize = mL1StatIter->first;
				}
			}
			printf("The optimal L1CacheElement size is: %u\n", nOptimalSize);
			primeStats.nL1CacheElements = nOptimalSize;
			nL1CacheElements = nOptimalSize;
			bOptimalL1SearchInProgress = false;
			break;
		}			
		printf("Auto Tuning in progress: %u %%\n", ((primeStats.nL1CacheElements  - nL1CacheElementsStart)*100) / (nL1CacheElementsMax - nL1CacheElementsStart));
		}
				
		float ratio = primeStats.nWaveTime == 0 ? 0 : ((float)primeStats.nWaveTime / (float)(primeStats.nWaveTime + primeStats.nTestTime)) * 100.0;
		printf("WaveTime %u - Wave Round %u - L1CacheSize %u - TotalWaveTime: %u - TotalTestTime: %u - Ratio: %.01f / %.01f %%\n", 
			primeStats.nWaveRound == 0 ? 0 : primeStats.nWaveTime / primeStats.nWaveRound, primeStats.nWaveRound, nL1CacheElements,
			primeStats.nWaveTime, primeStats.nTestTime, ratio, 100.0 - ratio);
		if (bEnabled)
			nCounter ++;
	}
}

bool bEnablenPrimorialMultiplierTuning = true;

#ifdef _WIN32
int RoundSieveAutoTuningWorkerThread(void)
#else
void *RoundSieveAutoTuningWorkerThread(void *)
#endif
{
		// Auto Tuning for nPrimorialMultiplier
		int nSampleSeconds = 3;

		while (true && !xptClient_isDisconnected(workData.xptClient, NULL))
		{
			if (!bOptimalL1SearchInProgress || !bEnablenPrimorialMultiplierTuning){
			primeStats.nWaveTime = 0;
			primeStats.nWaveRound = 0;
			primeStats.nTestTime = 0;
			primeStats.nTestRound = 0;
			Sleep(nSampleSeconds*1000);
			float ratio = primeStats.nWaveTime == 0 ? 0 : ((float)primeStats.nWaveTime / (float)(primeStats.nWaveTime + primeStats.nTestTime)) * 100.0;
			//printf("\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\n");
			//printf("WaveTime %u - Wave Round %u - L1CacheSize %u - TotalWaveTime: %u - TotalTestTime: %u - Ratio: %.01f / %.01f %%\n", 
			//	primeStats.nWaveRound == 0 ? 0 : primeStats.nWaveTime / primeStats.nWaveRound, primeStats.nWaveRound, nL1CacheElements,
			//	primeStats.nWaveTime, primeStats.nTestTime, ratio, 100.0 - ratio);
			//printf( "PrimorialMultiplier: %u\n",  primeStats.nPrimorialMultiplier);
			//printf("\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\n");

			if (!bEnablenPrimorialMultiplierTuning)
				continue; // Auto Tuning is disabled

			if ( !xptClient_isDisconnected(workData.xptClient, NULL) ){
				bEnablenPrimorialMultiplierTuning = false;
				return 0;
			}

			if (ratio > nRoundSievePercentage + 5)
			{
      // explicit cast to ref removes g++ warning but might be dumb, dunno
				if (!PrimeTableGetNextPrime((unsigned int &)  primeStats.nPrimorialMultiplier))
					error("PrimecoinMiner() : primorial increment overflow - resetting");
				primeStats.nPrimorialMultiplier = commandlineInput.initialPrimorial;
				printf( "\nSieve/Test ratio: %.01f / %.01f %%  - New PrimorialMultiplier: %u\n", ratio, 100.0 - ratio,  primeStats.nPrimorialMultiplier);
			}
			else
			{
				if (ratio < nRoundSievePercentage - 5)
				{
					//fix 0/100% bug lack of error message...
					if ( primeStats.nPrimorialMultiplier >= 2)
					{
						if (!PrimeTableGetPreviousPrime((unsigned int &) primeStats.nPrimorialMultiplier)){
							//@todo: fix (bandaid) 0/100% bug?
							error("PrimecoinMiner() : primorial decrement overflow - resetting");
							primeStats.nPrimorialMultiplier = commandlineInput.initialPrimorial;
						}
					}
					printf( "\nSieve/Test ratio: %.01f / %.01f %%  - New PrimorialMultiplier: %u\n", ratio, 100.0 - ratio,  primeStats.nPrimorialMultiplier);
				}
			}
		}
	}
		return 0;
}

void PrintCurrentSettings()
{
	unsigned long uptime = (getTimeMilliseconds() - primeStats.startTime);

	unsigned int days = uptime / (24 * 60 * 60 * 1000);
    uptime %= (24 * 60 * 60 * 1000);
    unsigned int hours = uptime / (60 * 60 * 1000);
    uptime %= (60 * 60 * 1000);
    unsigned int minutes = uptime / (60 * 1000);
    uptime %= (60 * 1000);
    unsigned int seconds = uptime / (1000);

	printf("\n--------------------------------------------------------------------------------\n");	
	printf("Worker name (-u): %s\n", commandlineInput.workername);
if(commandlineInput.csEnabled)
	printf("Central Server (-cs): %s\n", commandlineInput.centralServer);
	printf("Number of mining threads (-t): %u\n", commandlineInput.numThreads);
	printf("Sieve Size (-s): %u\n", nMaxSieveSize);
	printf("Sieve Percentage (-d): %u\n", nSievePercentage);
	printf("Round Sieve Percentage (-r): %u\n", nRoundSievePercentage);
	printf("Prime Limit (-primes): %u\n", commandlineInput.sievePrimeLimit);
	printf("Primorial Multiplier (-m): %u\n", primeStats.nPrimorialMultiplier);
	printf("L1CacheElements (-c): %u\n", primeStats.nL1CacheElements);	
	printf("Chain Length Target (-target): %u\n", nOverrideTargetValue);	
	printf("BiTwin Length Target (-bttarget): %u\n", nOverrideBTTargetValue);	
	printf("Sieve Extensions (-se): %u\n", nSieveExtensions);	
	printf("Total Runtime: %u Days, %u Hours, %u minutes, %u seconds\n", days, hours, minutes, seconds);	
	printf("Total Share Value submitted to the Pool: %.05f\n", primeStats.fTotalSubmittedShareValue);	
	printf("--------------------------------------------------------------------------------\n");
}



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
			std::exit(0);
#ifdef _WIN32
			return;
#else
			/*restore the old settings*/
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
	return 0;
#endif
			break;
		case '[':
			// explicit cast to ref removes g++ warning but might be dumb, dunno
			if (!PrimeTableGetPreviousPrime((unsigned int &) primeStats.nPrimorialMultiplier))
				error("PrimecoinMiner() : primorial decrement overflow");	
			printf("Primorial Multiplier: %u\n", primeStats.nPrimorialMultiplier);
			break;
		case ']':
			// explicit cast to ref removes g++ warning but might be dumb, dunno
			if (!PrimeTableGetNextPrime((unsigned int &)  primeStats.nPrimorialMultiplier))
				error("PrimecoinMiner() : primorial increment overflow");
			printf("Primorial Multiplier: %u\n", primeStats.nPrimorialMultiplier);
			break;
		case 'p': case 'P':
			bEnablenPrimorialMultiplierTuning = !bEnablenPrimorialMultiplierTuning;
			printf("Primorial Multiplier Auto Tuning was %s.\n", bEnablenPrimorialMultiplierTuning ? "Enabled": "Disabled");
			break;
		case 'c': case 'C':
			if (!bOptimalL1SearchInProgress)
			{
#ifdef _WIN32
				CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CacheAutoTuningWorkerThread, (LPVOID)true, 0, 0);
#else
				uint32_t totalThreads = commandlineInput.numThreads + 2;
				pthread_t threads[totalThreads];
				const bool enabled = true;
				pthread_create(&threads[commandlineInput.numThreads+1], NULL, CacheAutoTuningWorkerThread, (void *)&enabled);
#endif
				puts("Auto tunning for L1CacheElements size was started");
			}
			break;
		case 's': case 'S':			
			PrintCurrentSettings();
			break;
		case '+': case '=':
			if (!bOptimalL1SearchInProgress && nMaxSieveSize < 10000000)
				nMaxSieveSize += 100000;
			printf("Sieve size: %u\n", nMaxSieveSize);
			break;
		case '-':
			if (!bOptimalL1SearchInProgress && nMaxSieveSize > 100000)
				nMaxSieveSize -= 100000;
			printf("Sieve size: %u\n", nMaxSieveSize);
			break;
		case 0: case 224:
			{
				input = getchar();	
				switch (input)
				{
				case 72: // key up
					if (!bOptimalL1SearchInProgress && nSievePercentage < 100)
						nSievePercentage ++;
					printf("Sieve Percentage: %u%%\n", nSievePercentage);
					break;

				case 80: // key down
					if (!bOptimalL1SearchInProgress && nSievePercentage > 3)
						nSievePercentage --;
					printf("Sieve Percentage: %u%%\n", nSievePercentage);
					break;

				case 77:    // key right
					if( nRoundSievePercentage < 98)
						nRoundSievePercentage++;
					printf("Round Sieve Percentage: %u%%\n", nRoundSievePercentage);
					break;
				case 75:    // key left
					if( nRoundSievePercentage > 2)
						nRoundSievePercentage--;
					printf("Round Sieve Percentage: %u%%\n", nRoundSievePercentage);
					break;
				}
			}

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
#ifdef _WIN32
typedef std::pair <DWORD, HANDLE> thMapKeyVal;
DWORD * threadHearthBeat;

static void watchdog_thread(std::map<DWORD, HANDLE> threadMap)
{
	DWORD maxIdelTime = 10 * 1000;
	std::map <DWORD, HANDLE> :: const_iterator thMap_Iter;
		while(true)
		{
			if (!IsXptClientConnected())
				continue;
			uint64 currentTick = getTimeMilliseconds();

			for (int i = 0; i < threadMap.size(); i++)
			{
				DWORD heartBeatTick = threadHearthBeat[i];
				if (currentTick - heartBeatTick > maxIdelTime)
				{
					//restart the thread
					printf("Restarting thread %d\n", i);
					//__try
					//{

						//HANDLE h = threadMap.at(i);
						thMap_Iter = threadMap.find(i);
						if (thMap_Iter != threadMap.end())
						{
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
			Sleep( 1*1000);
		}
}
#endif

/*
 * Mainloop when using xpt mode
 */
int jhMiner_main_xptMode()
{
	#ifdef _WIN32
	// start the Auto Tuning thread
  if( commandlineInput.enableCacheTunning ){
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CacheAutoTuningWorkerThread, (LPVOID)commandlineInput.enableCacheTunning, 0, 0);
  }
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RoundSieveAutoTuningWorkerThread, NULL, 0, 0);
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)input_thread, NULL, 0, 0);
	// start threads
	for(sint32 threadIdx=0; threadIdx<commandlineInput.numThreads; threadIdx++)
	{
		HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)jhMiner_workerThread_xpt, (LPVOID)threadIdx, 0, 0);
		SetThreadPriority(hThread, THREAD_PRIORITY_BELOW_NORMAL);
	}
#else
 uint32_t totalThreads = commandlineInput.numThreads + 2;
  pthread_t threads[totalThreads];
  // start the Auto Tuning thread
  if( commandlineInput.enableCacheTunning ){
  pthread_create(&threads[commandlineInput.numThreads+1], NULL, CacheAutoTuningWorkerThread, (void *)&commandlineInput.enableCacheTunning);
  }
  pthread_create(&threads[commandlineInput.numThreads+2], NULL, RoundSieveAutoTuningWorkerThread, NULL);
  pthread_create(&threads[commandlineInput.numThreads], NULL, input_thread, NULL);
  pthread_attr_t threadAttr;
  pthread_attr_init(&threadAttr);
  // Set the stack size of the thread
  pthread_attr_setstacksize(&threadAttr, 120*1024);
  // free resources of thread upon return
  pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED);
  
  // start threads
	for(sint32 threadIdx=0; threadIdx<commandlineInput.numThreads; threadIdx++)
  {
	pthread_create(&threads[threadIdx], 
                   &threadAttr, 
                   jhMiner_workerThread_xpt, 
                   (void *)threadIdx);
  }
  pthread_attr_destroy(&threadAttr);
#endif
	// main thread, don't query work, just wait and process
	sint32 loopCounter = 0;
	uint32 xptWorkIdentifier = 0xFFFFFFFF;
	uint64 time_multiAdjust = getTimeMilliseconds();
   //unsigned long lastFiveChainCount = 0;
   //unsigned long lastFourChainCount = 0;
	while( true )
	{
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
            //float fiveSharePerPeriod = ((double)(primeStats.chainCounter[0][5] - lastFiveChainCount) / statsPassedTime) * 3600000.0;
            //float fourSharePerPeriod = ((double)(primeStats.chainCounter[0][4] - lastFourChainCount) / statsPassedTime) * 3600000.0;
            //lastFiveChainCount = primeStats.chainCounter[0][5];
            //lastFourChainCount = primeStats.chainCounter[0][4];
            printf("\nVal/h: %8f - PPS: %d - SPS: %.03f - ACC: %d\n", shareValuePerHour, (sint32)primesPerSecond, sievesPerSecond, (sint32)avgCandidatesPerRound);
            printf(" Chain/Hr: ");

            for(int i=6; i<=10; i++)
				{
               printf("%2d: %8.02f ", i, ((double)primeStats.chainCounter[0][i] / statsPassedTime) * 3600000.0);
				}
            if (primeStats.bestPrimeChainDifficultySinceLaunch >= 11)
            {
            //   printf("\n           ");
               for(int i=11; i<=15; i++)
               {
                  printf("%2d: %8.02f ", i, ((double)primeStats.chainCounter[0][i] / statsPassedTime) * 3600000.0);
               }
            }
            printf("\n\n");
				//printf(" - Best: %.04f - Max: %.04f\n", primeDifficulty, primeStats.bestPrimeChainDifficultySinceLaunch);
			}
		}
		// wait and check some stats
		uint64 time_updateWork = getTimeMilliseconds();
		while( true )
		{
			uint64 tickCount = getTimeMilliseconds();
			uint64 passedTime = tickCount - time_updateWork;


	/*		if (tickCount - time_multiAdjust >= 60000)
			{
				MultiplierAutoAdjust();
				time_multiAdjust = getTimeMilliseconds();
			}*/

			if( passedTime >= 4000 )
				break;
			xptClient_process(workData.xptClient);
			char* disconnectReason = false;
			if( workData.xptClient == NULL || xptClient_isDisconnected(workData.xptClient, &disconnectReason) )
			{
				// disconnected, mark all data entries as invalid
				for(uint32 i=0; i<128; i++)
					workData.workEntry[i].dataIsValid = false;
				printf("xpt: Disconnected, auto reconnect in 30 seconds\n");
				if( workData.xptClient && disconnectReason )
					printf("xpt: Disconnect reason: %s\n", disconnectReason);
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
               printf("\n--------------------------------------------------------------------------------\n");
               printf("New Block: %u - Diff: %.06f / %.06f\n", workData.xptClient->blockWorkInfo.height, blockDiff, poolDiff);
               printf("Total/Valid shares: [ %d / %d ]  -  Max diff: %.06f\n", total_shares,valid_shares, primeStats.bestPrimeChainDifficultySinceLaunch);
               statsPassedTime = (double)(getTimeMilliseconds() - primeStats.blockStartTime);
               if( statsPassedTime < 1.0 ) statsPassedTime = 1.0; // avoid division by zero
               for (int i = 6; i <= std::max(6,(int)primeStats.bestPrimeChainDifficultySinceLaunch); i++)
               {
                  double sharePerHour = ((double)primeStats.chainCounter[0][i] / statsPassedTime) * 3600000.0;
                  printf("%2dch/h: %8.02f - %u [ %u / %u / %u ]\n", // - Val: %0.06f\n", 
                     i, sharePerHour, 
                     primeStats.chainCounter[0][i],
                     primeStats.chainCounter[1][i],
                     primeStats.chainCounter[2][i],
                     primeStats.chainCounter[3][i]//, 
                     //(double)primeStats.chainCounter[0][i] * GetValueOfShareMajor(i)
                     );
               }
               printf("Share Value submitted - Last Block/Total: %0.6f / %0.6f\n", primeStats.fBlockShareValue, primeStats.fTotalSubmittedShareValue);
               printf("Current Primorial Value: %u\n", primeStats.nPrimorialMultiplier);
               printf("--------------------------------------------------------------------------------\n");

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
	commandlineInput.numThreads = getNumThreads();
	commandlineInput.numThreads = std::max(commandlineInput.numThreads, 1);
	commandlineInput.sieveSize = 1000000; // default maxSieveSize
	commandlineInput.sievePercentage = 10; // default 
	commandlineInput.roundSievePercentage = 70; // default 
	commandlineInput.enableCacheTunning = false;
	commandlineInput.L1CacheElements = 256000;
	commandlineInput.primorialMultiplier = 0; // for default 0 we will swithc aouto tune on
commandlineInput.targetOverride = 0;
   commandlineInput.targetBTOverride = 0;
   commandlineInput.initialPrimorial = 61;
   commandlineInput.printDebug = 0;
   commandlineInput.centralServer = "http://xpm.tandyuk.com";
   commandlineInput.centralServerPort = 80;
   commandlineInput.csEnabled = false;
	commandlineInput.sieveExtensions = 7;

	
	commandlineInput.sievePrimeLimit = 0;
	// parse command lines
	jhMiner_parseCommandline(argc, argv);
	// Sets max sieve size
	nMaxSieveSize = commandlineInput.sieveSize;
	nSievePercentage = commandlineInput.sievePercentage;
	nRoundSievePercentage = commandlineInput.roundSievePercentage;
   nOverrideTargetValue = commandlineInput.targetOverride;
   nOverrideBTTargetValue = commandlineInput.targetBTOverride;
	nSieveExtensions = commandlineInput.sieveExtensions;

	if (commandlineInput.sievePrimeLimit == 0) //default before parsing 
		commandlineInput.sievePrimeLimit = commandlineInput.sieveSize;  //default is sieveSize 
	primeStats.nL1CacheElements = commandlineInput.L1CacheElements;

	if(commandlineInput.primorialMultiplier == 0)
	{
		primeStats.nPrimorialMultiplier = 37;
		bEnablenPrimorialMultiplierTuning = true;
	}
	else
	{
		primeStats.nPrimorialMultiplier = commandlineInput.primorialMultiplier;
		bEnablenPrimorialMultiplierTuning = false;
	}

	if( commandlineInput.host == NULL )
	{
		printf("Missing -o option\n");
		exit(-1);	
	}

	//CRYPTO_set_mem_ex_functions(mallocEx, reallocEx, freeEx);
	
	printf(" ============================================================================== \n");
	printf("|  jhPrimeMiner - mod by rdebourbon -v3.2beta                     |\n");
	printf("|     optimised from hg5fm (mumus) v7.1 build + HP10 updates      |\n");
	printf("|  author: JH (http://ypool.net)                                  |\n");
	printf("|  contributors: x3maniac                                         |\n");
	printf("|  Credits: Sunny King for the original Primecoin client&miner    |\n");
	printf("|  Credits: mikaelh for the performance optimizations             |\n");
	printf("|  Credits: erkmos for the original linux port                    |\n");
	printf("|  Credits: tandyuk for the linux build of rdebourbons mod        |\n");
	printf("|                                                                 |\n");
	printf("|  Donations:                                                   |\n");
	printf("|        XPM: AUwKMCYCacE6Jq1rsLcSEHSNiohHVVSiWv                |\n");
	printf("|        LTC: LV7VHT3oGWQzG9EKjvSXd3eokgNXj6ciFE                |\n");
	printf("|        BTC: 1Fph7y622HJ5Cwq4bkzfeZXWep2Jyi5kp7                |\n");
	printf(" ============================================================================== \n");
	printf("Launching miner...\n");
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
	printf("Sieve Percentage: %u %%\n", nSievePercentage);
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
	if( ((ip>>0)&0xFF) != 255 )
	{
		printf("Connecting to '%s' (%lu.%lu.%lu.%lu)\n", commandlineInput.host, ((ip>>0)&0xFF), ((ip>>8)&0xFF), ((ip>>16)&0xFF), ((ip>>24)&0xFF));
	}
#else
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  getaddrinfo(commandlineInput.host, 0, &hints, &res);
  char ipText[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr, ipText, INET_ADDRSTRLEN);
#endif
  
	// setup RPC connection data (todo: Read from command line)
	jsonRequestTarget.ip = ipText;
	jsonRequestTarget.port = commandlineInput.port;
	jsonRequestTarget.authUser = commandlineInput.workername;
	jsonRequestTarget.authPass = commandlineInput.workerpass;




	if(commandlineInput.csEnabled){
		// connect to host
#ifdef _WIN32
	hostent* hostInfo = gethostbyname(commandlineInput.centralServer);
	if( hostInfo == NULL )
	{
		printf("Cannot resolve '%s'. Is it a valid URL?\n", commandlineInput.centralServer);
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
	if( ((ip>>0)&0xFF) != 255 )
	{
		printf("Connecting to '%s' (%lu.%lu.%lu.%lu) for stats output and remote config\n", commandlineInput.host, ((ip>>0)&0xFF), ((ip>>8)&0xFF), ((ip>>16)&0xFF), ((ip>>24)&0xFF));
	}
#else
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  getaddrinfo(commandlineInput.centralServer, 0, &hints, &res);
  char ipText[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr, ipText, INET_ADDRSTRLEN);
#endif
  		statsRequestTarget.ip = commandlineInput.centralServer;
		statsRequestTarget.port = commandlineInput.centralServerPort;
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
	printf("Using %d threads\n", commandlineInput.numThreads);
	printf("Username: %s\n", jsonRequestTarget.authUser);
	printf("Password: %s\n", jsonRequestTarget.authPass);
	// decide protocol
	if( commandlineInput.port == 10034 )
	{
		// port 10034 indicates xpt protocol (in future we will also add a -o URL prefix)
		workData.protocolMode = MINER_PROTOCOL_XPUSHTHROUGH;
		printf("Using x.pushthrough protocol\n");
	}
	else
	{
		workData.protocolMode = MINER_PROTOCOL_GETWORK;
		printf("Using GetWork() protocol\n");
		printf("Warning: \n");
		printf("   GetWork() is outdated and inefficient. You are losing mining performance\n");
		printf("   by using it. If the pool supports it, consider switching to x.pushthrough.\n");
		printf("   Just add the port :10034 to the -o parameter.\n");
		printf("   Example: jhPrimeminer.exe -o http://poolurl.net:10034 ...\n");
	}
	// initial query new work / create new connection
	if( workData.protocolMode == MINER_PROTOCOL_GETWORK )
	{
		jhMiner_queryWork_primecoin();
	}
	else if( workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH )
	{
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
				printf("Failed to connect, retry in 30 seconds\n");
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
			if( disconnectReason )
				printf("xpt error: %s\n", disconnectReason);
			// delete client
			xptClient_free(workData.xptClient);
			// try again in 30 seconds
			printf("x.pushthrough authentication sequence failed, retry in 30 seconds\n");
			Sleep(30*1000);
		}
	}

   printf("\nVal/h = 'Share Value per Hour', PPS = 'Primes per Second', \n");
   printf("SPS = 'Sieves per Second', ACC = 'Avg. Candidate Count / Sieve' \n");
   printf("===============================================================\n");
	printf("Keyboard shortcuts:\n");
	printf("   <Ctrl-C>, <Q>     - Quit\n");
	printf("   <Up arrow key>    - Increment Sieve Percentage\n");
	printf("   <Down arrow key>  - Decrement Sieve Percentage\n");
	printf("   <Left arrow key>  - Decrement Round Sieve Percentage\n");
	printf("   <Right arrow key> - Increment Round Sieve Percentage\n");
	printf("   <P> - Enable/Disable Round Sieve Percentage Auto Tuning\n");
	printf("   <S> - Print current settings\n");
	printf("   <[> - Decrement Primorial Multiplier\n");
	printf("   <]> - Increment Primorial Multiplier\n");
	printf("   <-> - Decrement Sive size\n");
	printf("   <+> - Increment Sieve size\n");
if( commandlineInput.enableCacheTunning ){
	printf("Note: While the initial auto tuning is in progress several values cannot be changed.\n");
}

	// enter different mainloops depending on protocol mode
	if( workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH )
		return jhMiner_main_xptMode();

	return 0;
}
