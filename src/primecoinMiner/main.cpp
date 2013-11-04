#include"global.h"

#include<intrin.h>
#include<ctime>
#include<map>
#include<conio.h>

primeStats_t primeStats = {0};
volatile int total_shares = 0;
volatile int valid_shares = 0;
unsigned int nMaxSieveSize;
unsigned int nSievePercentage;
bool nPrintDebugMessages;
unsigned long nOverrideTargetValue;
unsigned int nOverrideBTTargetValue;
char* dt;
bool useGetBlockTemplate = true;
uint8 decodedWalletAddress[32];
int decodedWalletAddressLen;

typedef struct  
{
   bool isValidData;
   // block data
   uint32 version;
   uint32 height;
   uint32 nTime;
   uint32 nBits;
   uint8 previousBlockHash[32];
   uint8 target[32]; // sha256 & scrypt
   // coinbase aux
   uint8 coinbaseauxFlags[128];
   uint32 coinbaseauxFlagsLength; // in bytes
   // todo: mempool transactions
}getBlockTemplateData_t;

getBlockTemplateData_t getBlockTemplateData = {0};

typedef struct  
{
   char* workername;
   char* workerpass;
   char* host;
   sint32 port;
   bool useXPT;
   sint32 numThreads;
   sint32 sieveSize;
   sint32 sievePercentage;
   sint32 roundSievePercentage;
   sint32 sievePrimeLimit;	// how many primes should be sieved
   unsigned int L1CacheElements;
   unsigned int primorialMultiplier;
   bool enableCacheTunning;
   sint32 targetOverride;
   sint32 targetBTOverride;
   sint32 sieveExtensions;
   bool printDebug;
   // getblocktemplate stuff
   char* xpmAddress; // we will use this XPM address for block payout
}commandlineInput_t;

commandlineInput_t commandlineInput = {0};

bool error(const char *format, ...)
{
   puts(format);
   //__debugbreak();
   return false;
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


static const char* pszBase58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

inline bool DecodeBase58(const char* psz, uint8* vchRet, int* retLength)
{
   CAutoBN_CTX pctx;
   CBigNum bn58 = 58;
   CBigNum bn = 0;
   CBigNum bnChar;
   while (isspace(*psz))
      psz++;
   // Convert big endian string to bignum
   for (const char* p = psz; *p; p++)
   {
      const char* p1 = strchr(pszBase58, *p);
      if (p1 == NULL)
      {
         while (isspace(*p))
            p++;
         if (*p != '\0')
            return false;
         break;
      }
      bnChar.setulong(p1 - pszBase58);
      if (!BN_mul(&bn, &bn, &bn58, pctx))
         throw bignum_error("DecodeBase58 : BN_mul failed");
      bn += bnChar;
   }

   // Get bignum as little endian data
   std::vector<unsigned char> vchTmp = bn.getvch();

   // Trim off sign byte if present
   if (vchTmp.size() >= 2 && vchTmp.end()[-1] == 0 && vchTmp.end()[-2] >= 0x80)
      vchTmp.erase(vchTmp.end()-1);

   // Restore leading zeros
   int nLeadingZeros = 0;
   for (const char* p = psz; *p == pszBase58[0]; p++)
      nLeadingZeros++;
   // Convert little endian data to big endian
   int rLen = nLeadingZeros + vchTmp.size();
   for(int i=0; i<rLen; i++)
   {
      vchRet[rLen-i-1] = vchTmp[i];
   }
   *retLength = rLen;
   return true;
}

/*
* Parses a hex string
* Length should be a multiple of 2
*/
void jhMiner_parseHexString(char* hexString, uint32 length, uint8* output)
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
void jhMiner_parseHexStringLE(char* hexString, uint32 length, uint8* output)
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
   CRITICAL_SECTION cs;
   uint8 protocolMode;
   // xpm
   workDataEntry_t workEntry[128]; // work data for each thread (up to 128)
   // x.pushthrough
   xptClient_t* xptClient;
}workData_t;

#define MINER_PROTOCOL_GETWORK		(1)
#define MINER_PROTOCOL_STRATUM		(2)
#define MINER_PROTOCOL_XPUSHTHROUGH	(3)
#define MINER_PROTOCOL_GBT			(4)

bool bSoloMining = false;
workData_t workData;
int lastBlockCount = 0;

jsonRequestTarget_t jsonRequestTarget; // rpc login data
bool useLocalPrimecoindForLongpoll;

/*
* Pushes the found block data to the server for giving us the $$$
* Uses getwork to push the block
* Returns true on success
* Note that the primecoin data can be larger due to the multiplier at the end, so we use 512 bytes per default
* 29.sep: switched to 512 bytes per block as default, since Primecoin can use up to 2000 bits (250 bytes) for the multiplier chain + length prefix of 2 bytes
*/
bool jhMiner_pushShare_primecoin(uint8 data[512], primecoinBlock_t* primecoinBlock)
{
   if( workData.protocolMode == MINER_PROTOCOL_GETWORK )
   {
      // prepare buffer to send
      fStr_buffer4kb_t fStrBuffer_parameter;
      fStr_t* fStr_parameter = fStr_alloc(&fStrBuffer_parameter, FSTR_FORMAT_UTF8);
      fStr_append(fStr_parameter, "[\"");
      fStr_addHexString(fStr_parameter, data, 512);
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
            printf("Server rejected share (BlockHeight: %d/%d nBits: 0x%08X)\n", primecoinBlock->serverData.blockHeight, jhMiner_getCurrentWorkBlockHeight(primecoinBlock->threadIndex), primecoinBlock->serverData.client_shareBits);
            jsonObject_freeObject(jsonReturnValue);
            return false;
         }
      }
      jsonObject_freeObject(jsonReturnValue);
      return false;
   }
   else if( workData.protocolMode == MINER_PROTOCOL_GBT )
   {
      // use submitblock
      char* methodName = "submitblock";
      // get multiplier
      CBigNum bnPrimeChainMultiplier;
      bnPrimeChainMultiplier.SetHex(primecoinBlock->mpzPrimeChainMultiplier.get_str(16));
      std::vector<unsigned char> bnSerializeData = bnPrimeChainMultiplier.getvch();
      sint32 lengthBN = bnSerializeData.size();
      //memcpy(xptShareToSubmit->chainMultiplier, &bnSerializeData[0], lengthBN);
      //xptShareToSubmit->chainMultiplierSize = lengthBN;
      // prepare raw data of block
      uint8 dataRaw[512] = {0};
      uint8 proofOfWorkHash[32];
      bool shareAccepted = false;
      memset(dataRaw, 0x00, sizeof(dataRaw));
      *(uint32*)(dataRaw+0) = primecoinBlock->version;
      memcpy((dataRaw+4), primecoinBlock->prevBlockHash, 32);
      memcpy((dataRaw+36), primecoinBlock->merkleRoot, 32);
      *(uint32*)(dataRaw+68) = primecoinBlock->timestamp;
      *(uint32*)(dataRaw+72) = primecoinBlock->nBits;
      *(uint32*)(dataRaw+76) = primecoinBlock->nonce;
      *(uint8*)(dataRaw+80) = lengthBN;
      if( lengthBN > 0x7F )
         printf("Warning: chainMultiplierSize exceeds 0x7F in jhMiner_pushShare_primecoin()\n");
      memcpy(dataRaw+81, &bnSerializeData[0], lengthBN);
      // create stream to write block data to
      stream_t* blockStream = streamEx_fromDynamicMemoryRange(1024*64);
      // write block data
      stream_writeData(blockStream, dataRaw, 80+1+lengthBN);
      // generate coinbase transaction
      bitclientTransaction_t* txCoinbase = bitclient_createCoinbaseTransactionFromSeed(primecoinBlock->seed, primecoinBlock->threadIndex, getBlockTemplateData.height, decodedWalletAddress+1, jhMiner_primeCoin_targetGetMint(primecoinBlock->nBits));
      // write amount of transactions (varInt)
      bitclient_addVarIntFromStream(blockStream, 1);
      bitclient_writeTransactionToStream(blockStream, txCoinbase);
      // map buffer
      sint32 blockDataLength = 0;
      uint8* blockData = (uint8*)streamEx_map(blockStream, &blockDataLength);
      // clean up
      bitclient_destroyTransaction(txCoinbase);
      // prepare buffer to send
      fStr_buffer4kb_t fStrBuffer_parameter;
      fStr_t* fStr_parameter = fStr_alloc(&fStrBuffer_parameter, FSTR_FORMAT_UTF8);
      fStr_append(fStr_parameter, "[\""); // \"]
      fStr_addHexString(fStr_parameter, blockData, blockDataLength);
      fStr_append(fStr_parameter, "\"]");
      // send request
      sint32 rpcErrorCode = 0;
      jsonObject_t* jsonReturnValue = NULL;
      jsonReturnValue = jsonClient_request(&jsonRequestTarget, methodName, fStr_parameter, &rpcErrorCode);		
      // clean up rest
      stream_destroy(blockStream);
      free(blockData);
      // process result
      if( jsonReturnValue == NULL )
      {
         printf("SubmitBlock failed :(\n");
         return false;
      }
      else
      {
         // is the bitcoin client happy with the result?
         jsonObject_t* jsonReturnValueRejectReason = jsonObject_getParameter(jsonReturnValue, "result");
         if( jsonObject_getType(jsonReturnValueRejectReason) == JSON_TYPE_NULL )
         {
            printf("Valid block found!\n");
            jsonObject_freeObject(jsonReturnValue);
            return true;
         }
         else
         {
            // :( the client says no
            printf("Coin daemon rejected block :(\n");
            jsonObject_freeObject(jsonReturnValue);
            return false;
         }
      }

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
      if( workData.xptClient && !workData.xptClient->disconnected)
         xptClient_foundShare(workData.xptClient, xptShareToSubmit);
      else
      {
         printf("Share submission failed. The client is not connected to the pool.\n");
      }

   }
}

int queryLocalPrimecoindBlockCount(bool useLocal)
{
   sint32 rpcErrorCode = 0;
   jsonObject_t* jsonReturnValue = jsonClient_request(&jsonRequestTarget, "getblockcount", NULL, &rpcErrorCode);
   if( jsonReturnValue == NULL )
   {
      printf("getblockcount() failed with %serror code %d\n", (rpcErrorCode>1000)?"http ":"", rpcErrorCode>1000?rpcErrorCode-1000:rpcErrorCode);
      return 0;
   }
   else
   {
      jsonObject_t* jsonResult = jsonObject_getParameter(jsonReturnValue, "result");
      return (int) jsonObject_getNumberValueAsS32(jsonResult);
      jsonObject_freeObject(jsonReturnValue);
   }

   return 0;
}

static double DIFFEXACTONE = 26959946667150639794667015087019630673637144422540572481103610249215.0;
static const uint64_t diffone = 0xFFFF000000000000ull;

static double target_diff(const unsigned char *target)
{
   double targ = 0;
   signed int i;

   for (i = 31; i >= 0; --i)
      targ = (targ * 0x100) + target[i];

   return DIFFEXACTONE / (targ ? targ: 1);
}

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

/*
* Queries the work data from the coin client
* Uses "getblocktemplate"
* Should be called periodically (5-15 seconds) to keep the current block data up-to-date
*/
void jhMiner_queryWork_primecoin_getblocktemplate()
{
   sint32 rpcErrorCode = 0;
   fStr_buffer4kb_t fStrBuffer_parameter;
   fStr_t* fStr_parameter = fStr_alloc(&fStrBuffer_parameter, FSTR_FORMAT_UTF8);
   fStr_append(fStr_parameter, "[{\"capabilities\": [\"coinbasetxn\", \"workid\", \"coinbase/append\"]}]");
   jsonObject_t* jsonReturnValue = NULL;
   jsonReturnValue = jsonClient_request(&jsonRequestTarget, "getblocktemplate", fStr_parameter, &rpcErrorCode);
   if( jsonReturnValue == NULL )
   {
      printf("UpdateWork(GetBlockTemplate) failed.\n");
      getBlockTemplateData.isValidData = false;
      return;
   }
   else
   {
      jsonObject_t* jsonResult = jsonObject_getParameter(jsonReturnValue, "result");
      // data
      jsonObject_t* jsonResult_version = jsonObject_getParameter(jsonResult, "version");
      jsonObject_t* jsonResult_previousblockhash = jsonObject_getParameter(jsonResult, "previousblockhash");
      jsonObject_t* jsonResult_target = jsonObject_getParameter(jsonResult, "target");
      //jsonObject_t* jsonResult_mintime = jsonObject_getParameter(jsonResult, "mintime");
      jsonObject_t* jsonResult_curtime = jsonObject_getParameter(jsonResult, "curtime");
      jsonObject_t* jsonResult_bits = jsonObject_getParameter(jsonResult, "bits");
      jsonObject_t* jsonResult_height = jsonObject_getParameter(jsonResult, "height");
      jsonObject_t* jsonResult_coinbaseaux = jsonObject_getParameter(jsonResult, "coinbaseaux");
      jsonObject_t* jsonResult_coinbaseaux_flags = NULL;
      if( jsonResult_coinbaseaux )
         jsonResult_coinbaseaux_flags = jsonObject_getParameter(jsonResult_coinbaseaux, "flags");
      // are all fields present?
      if( jsonResult_version == NULL || jsonResult_previousblockhash == NULL || jsonResult_curtime == NULL || jsonResult_bits == NULL || jsonResult_height == NULL || jsonResult_coinbaseaux_flags == NULL )
      {
         printf("UpdateWork(GetBlockTemplate) failed due to missing fields in the response.\n");
         jsonObject_freeObject(jsonReturnValue);
      }
      // prepare field lengths
      uint32 stringLength_previousblockhash = 0;
      uint32 stringLength_target = 0;
      uint32 stringLength_bits = 0;
      uint32 stringLength_height = 0;
      // get version
      uint32 gbtVersion = jsonObject_getNumberValueAsS32(jsonResult_version);
      // get previous block hash
      uint8* stringData_previousBlockHash = jsonObject_getStringData(jsonResult_previousblockhash, &stringLength_previousblockhash);
      RtlZeroMemory(getBlockTemplateData.previousBlockHash, 32);
      jhMiner_parseHexStringLE((char*)stringData_previousBlockHash, stringLength_previousblockhash, getBlockTemplateData.previousBlockHash);
      // get target hash (optional)
      uint8* stringData_target = jsonObject_getStringData(jsonResult_target, &stringLength_target);
      RtlZeroMemory(getBlockTemplateData.target, 32);
      if( stringData_target )
         jhMiner_parseHexStringLE((char*)stringData_target, stringLength_target, getBlockTemplateData.target);
      // get timestamp (mintime)
      uint32 gbtTime = jsonObject_getNumberValueAsU32(jsonResult_curtime);
      // get bits
      char bitsTmpText[32]; // temporary buffer so we can add NT
      uint8* stringData_bits = jsonObject_getStringData(jsonResult_bits, &stringLength_bits);
      memcpy(bitsTmpText, stringData_bits, stringLength_bits);
      bitsTmpText[stringLength_bits] = '\0'; 
      uint32 gbtBits = 0;
      sscanf((const char*)bitsTmpText, "%x", &gbtBits);
      // get height
      uint32 gbtHeight = jsonObject_getNumberValueAsS32(jsonResult_height);
      // get coinbase aux flags
      uint32 stringLength_coinbaseauxFlags = 0;
      uint8* stringData_coinbaseauxFlags = jsonObject_getStringData(jsonResult_coinbaseaux_flags, &stringLength_coinbaseauxFlags);
      jhMiner_parseHexString((char*)stringData_coinbaseauxFlags, stringLength_coinbaseauxFlags, getBlockTemplateData.coinbaseauxFlags);
      getBlockTemplateData.coinbaseauxFlagsLength = stringLength_coinbaseauxFlags/2;
      // set remaining number parameters
      getBlockTemplateData.version = gbtVersion;
      getBlockTemplateData.nBits = gbtBits;
      getBlockTemplateData.nTime = gbtTime;
      getBlockTemplateData.height = gbtHeight;
      // done
      jsonObject_freeObject(jsonReturnValue);
      getBlockTemplateData.isValidData = true;
   }
}

void jhMiner_queryWork_primecoin_getwork()
{
   sint32 rpcErrorCode = 0;
   uint32 time1 = GetTickCount();
   jsonObject_t* jsonReturnValue = jsonClient_request(&jsonRequestTarget, "getwork", NULL, &rpcErrorCode);
   uint32 time2 = GetTickCount() - time1;
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
      jsonObject_t* jsonResult_target = jsonObject_getParameter(jsonResult, "target");
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

      EnterCriticalSection(&workData.cs);
      jhMiner_parseHexString((char*)stringData_data, min(128*2, stringData_length), workData.workEntry[0].data);
      workData.workEntry[0].dataIsValid = true;
      if (jsonResult_serverData == NULL)
      {
         unsigned char binDataReverse[128];
         for (unsigned int i = 0; i < 128 / 4;++i) 
            ((unsigned int *)binDataReverse)[i] = _swapEndianessU32(((unsigned int *)workData.workEntry[0].data)[i]);
         blockHeader_t * blockHeader = (blockHeader_t *)&binDataReverse[0];

         RtlZeroMemory(workData.workEntry[0].serverData, 32);
         ((serverData_t*)workData.workEntry[0].serverData)->nBitsForShare = blockHeader->nBits;
         ((serverData_t*)workData.workEntry[0].serverData)->blockHeight = lastBlockCount;
         useLocalPrimecoindForLongpoll = false;
         bSoloMining = true;

      }
      else
      {
         // get server data
         uint32 stringServerData_length = 0;
         uint8* stringServerData_data = jsonObject_getStringData(jsonResult_serverData, &stringServerData_length);
         RtlZeroMemory(workData.workEntry[0].serverData, 32);
         if( jsonResult_serverData )
            jhMiner_parseHexString((char*)stringServerData_data, min(128*2, 32*2), workData.workEntry[0].serverData);
      }
      // generate work hash
      uint32 workDataHash = 0x5B7C8AF4;
      for(uint32 i=0; i<stringData_length/2; i++)
      {
         workDataHash = (workDataHash>>29)|(workDataHash<<3);
         workDataHash += (uint32)workData.workEntry[0].data[i];
      }
      workData.workEntry[0].dataHash = workDataHash;
      LeaveCriticalSection(&workData.cs);
      jsonObject_freeObject(jsonReturnValue);
   }
}

bool SubmitBlock(primecoinBlock_t* pcBlock)
{
   blockHeader_t block = {0};
   memcpy(&block, pcBlock, 80);
   CBigNum bnPrimeChainMultiplier;
   bnPrimeChainMultiplier.SetHex(pcBlock->mpzPrimeChainMultiplier.get_str(16));
   std::vector<unsigned char> primemultiplier = bnPrimeChainMultiplier.getvch();

   //printf("nBits: %d\n", block.nBits);
   //printf("nNonce: %d\n", block.nonce);
   //printf("hashPrevBlock: %s\n", block.prevBlockHash.GetHex().c_str());
   //printf("block   - hashMerkleRoot: %s\n", block.merkleRoot.GetHex().c_str());
   //printf("pcBlock - hashMerkleRoot: %s\n",  HexStr(BEGIN(pcBlock->merkleRoot), END(pcBlock->merkleRoot)).c_str());
   //printf("Multip: %s\n", bnPrimeChainMultiplier.GetHex().c_str());

   if (primemultiplier.size() > 47) {
      error("primemultiplier is too big");
      return false;
   }

   block.primeMultiplier[0] = primemultiplier.size();

   for (size_t i = 0; i < primemultiplier.size(); ++i) 
      block.primeMultiplier[1 + i] = primemultiplier[i];

   for (unsigned int i = 0; i < 128 / 4; ++i) ((unsigned int *)&block)[i] =
      _swapEndianessU32(((unsigned int *)&block)[i]);

   unsigned char pdata[128] = {0};
   memcpy(pdata, &block, 128);
   std::string data_hex = HexStr(BEGIN(pdata), END(pdata));

   fStr_buffer4kb_t fStrBuffer_parameter;
   fStr_t* fStr_parameter = fStr_alloc(&fStrBuffer_parameter, FSTR_FORMAT_UTF8);
   fStr_append(fStr_parameter, "[\""); 
   fStr_append(fStr_parameter, (char *) data_hex.c_str());
   fStr_append(fStr_parameter, "\"]");

   // send request
   sint32 rpcErrorCode = 0;
   //jsonObject_t* jsonReturnValue = jsonClient_request(&jsonRequestTarget, "submitblock", fStr_parameter, &rpcErrorCode);
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
         printf("Submit block succeeded! :)\n");
         jsonObject_freeObject(jsonReturnValue);
         return true;
      }
      else
      {
         total_shares++;
         // the server says no to this share :(
         printf("Server rejected the Block. :(\n");
         jsonObject_freeObject(jsonReturnValue);
         return false;
      }
   }
   jsonObject_freeObject(jsonReturnValue);
   return false;
}


/*
* Returns the block height of the most recently received workload
*/
uint32 jhMiner_getCurrentWorkBlockHeight(sint32 threadIndex)
{
   if( workData.protocolMode == MINER_PROTOCOL_GETWORK )
      return ((serverData_t*)workData.workEntry[0].serverData)->blockHeight;	
   else if( workData.protocolMode == MINER_PROTOCOL_GBT )
      return getBlockTemplateData.height;	
   else
      return ((serverData_t*)workData.workEntry[threadIndex].serverData)->blockHeight;
}

/*
* Worker thread mainloop for getwork() mode
*/
int jhMiner_workerThread_getwork(int threadIndex)
{
   CSieveOfEratosthenes* psieve = NULL;
   while( true )
   {
      uint8 localBlockData[128];
      // copy block data from global workData
      uint32 workDataHash = 0;
      uint8 serverData[32];
      while( workData.workEntry[0].dataIsValid == false ) Sleep(200);
      EnterCriticalSection(&workData.cs);
      memcpy(localBlockData, workData.workEntry[0].data, 128);
      //seed = workData.seed;
      memcpy(serverData, workData.workEntry[0].serverData, 32);
      LeaveCriticalSection(&workData.cs);
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
      if (!BitcoinMiner(&primecoinBlock, psieve, threadIndex, commandlineInput.numThreads))
         break;
      primecoinBlock.mpzPrimeChainMultiplier = 0;
   }
   if( psieve )
   {
      delete psieve;
      psieve = NULL;
   }
   return 0;
}

static const sint64 PRIMECOIN_COIN = 100000000;
static const sint64 PRIMECOIN_CENT = 1000000;
static const unsigned int PRIMECOIN_nFractionalBits = 24;

/*
* Returns value of block
*/
uint64 jhMiner_primeCoin_targetGetMint(unsigned int nBits)
{
   if( nBits == 0 )
      return 0;
   uint64 nMint = 0;
   static uint64 nMintLimit = 999ull * PRIMECOIN_COIN;
   uint64 bnMint = nMintLimit;
   bnMint = (bnMint << PRIMECOIN_nFractionalBits) / nBits;
   bnMint = (bnMint << PRIMECOIN_nFractionalBits) / nBits;
   bnMint = (bnMint / PRIMECOIN_CENT) * PRIMECOIN_CENT;  // mint value rounded to cent
   nMint = bnMint;
   return nMint;
}

/*
* Worker thread mainloop for getblocktemplate mode
*/
int jhMiner_workerThread_gbt(int threadIndex)
{
   CSieveOfEratosthenes* psieve = NULL;
   while( true )
   {
      //uint8 localBlockData[128];
      primecoinBlock_t primecoinBlock = {0};
      // copy block data from global workData
      //uint32 workDataHash = 0;
      //uint8 serverData[32];
      while( getBlockTemplateData.isValidData == false ) Sleep(200);
      EnterCriticalSection(&workData.cs);
      // generate work from getBlockTemplate data
      primecoinBlock.threadIndex = threadIndex;
      primecoinBlock.version = getBlockTemplateData.version;
      primecoinBlock.timestamp = getBlockTemplateData.nTime;
      primecoinBlock.nonce = 0;
      primecoinBlock.seed = rand();
      primecoinBlock.nBits = getBlockTemplateData.nBits;
      memcpy(primecoinBlock.prevBlockHash, getBlockTemplateData.previousBlockHash, 32);
      // setup serverData struct
      primecoinBlock.serverData.blockHeight = getBlockTemplateData.height;
      primecoinBlock.serverData.nBitsForShare = getBlockTemplateData.nBits;
      // generate coinbase transaction and merkleroot
      bitclientTransaction_t* txCoinbase = bitclient_createCoinbaseTransactionFromSeed(primecoinBlock.seed, threadIndex, getBlockTemplateData.height, decodedWalletAddress+1, jhMiner_primeCoin_targetGetMint(primecoinBlock.nBits));
      bitclientTransaction_t* txList[64];
      txList[0] = txCoinbase;
      uint32 numberOfTx = 1;
      // generate tx hashes (currently we only support coinbase transaction)
      uint8 txHashList[64*32];
      for(uint32 t=0; t<numberOfTx; t++)
         bitclient_generateTxHash(txList[t], (txHashList+t*32));
      bitclient_calculateMerkleRoot(txHashList, numberOfTx, primecoinBlock.merkleRoot);
      bitclient_destroyTransaction(txCoinbase);
      LeaveCriticalSection(&workData.cs);
      primecoinBlock.xptMode = false;
      // start mining
      if (!BitcoinMiner(&primecoinBlock, psieve, threadIndex, commandlineInput.numThreads))
         break;
      primecoinBlock.mpzPrimeChainMultiplier = 0;
   }
   if( psieve )
   {
      delete psieve;
      psieve = NULL;
   }
   return 0;
}

/*
* Worker thread mainloop for xpt() mode
*/
int jhMiner_workerThread_xpt(int threadIndex)
{
   CSieveOfEratosthenes* psieve = NULL;
   while( true )
   {
      uint8 localBlockData[128];
      // copy block data from global workData
      uint32 workDataHash = 0;
      uint8 serverData[32];
      while( workData.workEntry[threadIndex].dataIsValid == false ) Sleep(50);
      EnterCriticalSection(&workData.cs);
      memcpy(localBlockData, workData.workEntry[threadIndex].data, 128);
      memcpy(serverData, workData.workEntry[threadIndex].serverData, 32);
      workDataHash = workData.workEntry[threadIndex].dataHash;
      LeaveCriticalSection(&workData.cs);
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
      if (!BitcoinMiner(&primecoinBlock, psieve, threadIndex, commandlineInput.numThreads))
         break;
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
   puts("Usage: jhPrimeminer.exe [options]");
   puts("Options:");
   puts("   -o, -O                        The miner will connect to this url");
   puts("                                 You can specifiy an port after the url using -o url:port");
   puts("   -xpt                          Use x.pushthrough protocol");
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
   puts("   -xpm <wallet address>         When doing solo mining this is the address your mined XPM will be transfered to.");
   puts("Example usage:");
   puts("   jhPrimeminer.exe -o http://poolurl.com:8332 -u workername.1 -p workerpass -t 4");
   puts("Press any key to continue...");
   _getch();
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
      else if( memcmp(argument, "-xpm", 5)==0 )
      {
         // -xpm
         if( cIdx >= argc )
         {
            printf("Missing wallet address after -xpm option\n");
            exit(0);
         }
         commandlineInput.xpmAddress = fStrDup(argv[cIdx], 64);
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
         // -primes
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
      else if( memcmp(argument, "-target", 7)==0 )
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
      else if( memcmp(argument, "-bttarget", 9)==0 )
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
      else if( memcmp(argument, "-se", 4)==0 )
      {
         // -target
         if( cIdx >= argc )
         {
            printf("Missing number after -se option\n");
            exit(0);
         }
         commandlineInput.sieveExtensions = atoi(argv[cIdx]);
         if( commandlineInput.sieveExtensions < 1 || commandlineInput.sieveExtensions > 15 )
         {
            printf("-se parameter out of range, must be between 0 - 15\n");
            exit(0);
         }
         cIdx++;
      }
      else if( memcmp(argument, "-debug", 6)==0 )
      {
         // -debug
         if( cIdx >= argc )
         {
            printf("Missing flag after -debug option\n");
            ExitProcess(0);
         }
         if (memcmp(argument, "true", 5) == 0 ||  memcmp(argument, "1", 2) == 0)
            commandlineInput.printDebug = true;
         cIdx++;
      }
      else if( memcmp(argument, "-xpt", 5)==0 )
      {
         commandlineInput.useXPT = true;
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

//void MultiplierAutoAdjust()
//{
//   //printf("\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\n");
//   //printf( "ChainHit:  %f - PrevChainHit: %f - PrimorialMultiplier: %u\n", primeStats.nChainHit, primeStats.nPrevChainHit, primeStats.nPrimorialMultiplier);
//   //printf("\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\n");
//
//   //bool fIncrementPrimorial = true;
//   if (primeStats.nChainHit == 0)
//      return;
//
//   if ( primeStats.nChainHit < primeStats.nPrevChainHit)
//      fIncrementPrimorial = !fIncrementPrimorial;
//
//   primeStats.nPrevChainHit = primeStats.nChainHit;
//   primeStats.nChainHit = 0;
//   // Primecoin: dynamic adjustment of primorial multiplier
//   if (fIncrementPrimorial)
//   {
//      if (!PrimeTableGetNextPrime((unsigned int)  primeStats.nPrimorialMultiplier))
//         error("PrimecoinMiner() : primorial increment overflow");
//   }
//   else if (primeStats.nPrimorialMultiplier > 7)
//   {
//      if (!PrimeTableGetPreviousPrime((unsigned int) primeStats.nPrimorialMultiplier))
//         error("PrimecoinMiner() : primorial decrement overflow");
//   }
//}

BYTE nRoundSievePercentage;
bool bOptimalL1SearchInProgress = false;

static void CacheAutoTuningWorkerThread(bool bEnabled)
{

   if (bOptimalL1SearchInProgress || !bEnabled)
      return;

   bOptimalL1SearchInProgress = true;

   DWORD startTime = GetTickCount();	
   unsigned int nL1CacheElementsStart = 64000;
   unsigned int nL1CacheElementsMax   = 2560000;
   unsigned int nL1CacheElementsIncrement = 64000;
   BYTE nSampleSeconds = 20;

   unsigned int nL1CacheElements = primeStats.nL1CacheElements;
   std::map <unsigned int, unsigned int> mL1Stat;
   std::map <unsigned int, unsigned int>::iterator mL1StatIter;
   typedef std::pair <unsigned int, unsigned int> KeyVal;

   primeStats.nL1CacheElements = nL1CacheElementsStart;

   long nCounter = 0;
   while (true && bEnabled && !appQuitSignal)
   {		
      primeStats.nWaveTime = 0;
      primeStats.nWaveRound = 0;
      primeStats.nTestTime = 0;
      primeStats.nTestRound = 0;
      Sleep(nSampleSeconds*1000);
      DWORD waveTime = primeStats.nWaveTime;
      if (bEnabled)
         nCounter ++;
      if (nCounter <=1) 
         continue;// wait a litle at the beginning

      nL1CacheElements = primeStats.nL1CacheElements;
      mL1Stat.insert( KeyVal((unsigned int)primeStats.nL1CacheElements, (unsigned int)primeStats.nWaveRound == 0 ? 0xFFFF : primeStats.nWaveTime / primeStats.nWaveRound));
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

      float ratio = primeStats.nWaveTime == 0 ? 0 : ((float)primeStats.nWaveTime / (float)(primeStats.nWaveTime + primeStats.nTestTime)) * 100.0;
      printf("WaveTime %u - Wave Round %u - L1CacheSize %u - TotalWaveTime: %u - TotalTestTime: %u - Ratio: %.01f / %.01f %%\n", 
         primeStats.nWaveRound == 0 ? 0 : primeStats.nWaveTime / primeStats.nWaveRound, primeStats.nWaveRound, nL1CacheElements,
         primeStats.nWaveTime, primeStats.nTestTime, ratio, 100.0 - ratio);

   }
}

bool bEnablenPrimorialMultiplierTuning = true;

static void RoundSieveAutoTuningWorkerThread(bool bEnabled)
{
   __try
   {


      // Auto Tuning for nPrimorialMultiplier
      int nSampleSeconds = 15;

      while (true)
      {
         if (bOptimalL1SearchInProgress || !bEnablenPrimorialMultiplierTuning || !IsXptClientConnected())
         {
            Sleep(10);
            continue;
         }
         primeStats.nWaveTime = 0;
         primeStats.nWaveRound = 0;
         primeStats.nTestTime = 0;
         primeStats.nTestRound = 0;
         Sleep(nSampleSeconds*1000);

         if (appQuitSignal)
            return;

         if (bOptimalL1SearchInProgress || !bEnablenPrimorialMultiplierTuning || !IsXptClientConnected())
            continue;

         float ratio = primeStats.nWaveTime == 0 ? 0 : ((float)primeStats.nWaveTime / (float)(primeStats.nWaveTime + primeStats.nTestTime)) * 100.0;
         //printf("\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\n");
         //printf("WaveTime %u - Wave Round %u - L1CacheSize %u - TotalWaveTime: %u - TotalTestTime: %u - Ratio: %.01f / %.01f %%\n", 
         //	primeStats.nWaveRound == 0 ? 0 : primeStats.nWaveTime / primeStats.nWaveRound, primeStats.nWaveRound, nL1CacheElements,
         //	primeStats.nWaveTime, primeStats.nTestTime, ratio, 100.0 - ratio);
         //printf( "PrimorialMultiplier: %u\n",  primeStats.nPrimorialMultiplier);
         //printf("\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\n");

         if (ratio == 0) continue; // No weaves occurred, don't change anything.

         if (ratio > nRoundSievePercentage + 5)
         {
            if (!PrimeTableGetNextPrime((unsigned int)  primeStats.nPrimorialMultiplier))
               error("PrimecoinMiner() : primorial increment overflow");
            printf( "Sieve/Test ratio: %.01f / %.01f %%  - New PrimorialMultiplier: %u\n", ratio, 100.0 - ratio,  primeStats.nPrimorialMultiplier);
         }
         else if (ratio < nRoundSievePercentage - 5)
         {
            if ( primeStats.nPrimorialMultiplier > 2)
            {
               if (!PrimeTableGetPreviousPrime((unsigned int) primeStats.nPrimorialMultiplier))
                  error("PrimecoinMiner() : primorial decrement overflow");
               printf( "Sieve/Test ratio: %.01f / %.01f %%  - New PrimorialMultiplier: %u\n", ratio, 100.0 - ratio,  primeStats.nPrimorialMultiplier);
            }
         }
      }
   }
   __except(EXCEPTION_EXECUTE_HANDLER)
   {
   }

}

void PrintCurrentSettings()
{
   unsigned long uptime = (GetTickCount() - primeStats.startTime);

   unsigned int days = uptime / (24 * 60 * 60 * 1000);
   uptime %= (24 * 60 * 60 * 1000);
   unsigned int hours = uptime / (60 * 60 * 1000);
   uptime %= (60 * 60 * 1000);
   unsigned int minutes = uptime / (60 * 1000);
   uptime %= (60 * 1000);
   unsigned int seconds = uptime / (1000);

   printf("\n\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\n");	
   printf("Worker name (-u): %s\n", commandlineInput.workername);
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
   if (!bSoloMining)
      printf("Total Share Value submitted to the Pool: %.05f\n", primeStats.fTotalSubmittedShareValue);	
   printf("\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\n\n");
}



bool appQuitSignal = false;

static void input_thread()
{
   while (true) 
   {
      int input;
      input = _getch();		
      switch (input) {
      case 'q': case 'Q': case 3: //case 27:
         appQuitSignal = true;
         Sleep(3200);
         std::exit(0);
         return;
         break;
      case '[':
         if (!PrimeTableGetPreviousPrime((unsigned int) primeStats.nPrimorialMultiplier))
            error("PrimecoinMiner() : primorial decrement overflow");	
         printf("Primorial Multiplier: %u\n", primeStats.nPrimorialMultiplier);
         break;
      case ']':
         if (!PrimeTableGetNextPrime((unsigned int)  primeStats.nPrimorialMultiplier))
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
            CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CacheAutoTuningWorkerThread, (LPVOID)true, 0, 0);
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
            input = _getch();	
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

   return;
}


typedef std::pair <DWORD, HANDLE> thMapKeyVal;
DWORD * threadHearthBeat;

static void watchdog_thread(std::map<DWORD, HANDLE> threadMap)
{
   DWORD maxIdelTime = 30 * 1000; // Allow 30 secs of "idle" time between heartbeats before a thread is deemed "dead".
   std::map <DWORD, HANDLE> :: const_iterator thMap_Iter;
   while(true)
   {
      if ((workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH) && (!IsXptClientConnected()))
      {
         // Miner is not connected, wait 5 secs before trying again.
         Sleep(5000);
         {
            Sleep(10);
            continue;
         }
         DWORD currentTick = GetTickCount();

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
                  threadHearthBeat[i] = GetTickCount();
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
}

void OnNewBlock(double nBitsShare, double nBits, unsigned long blockHeight)
{
   double totalRunTime = (double)(GetTickCount() - primeStats.startTime);
   double statsPassedTime = (double)(GetTickCount() - primeStats.primeLastUpdate);
   if( statsPassedTime < 1.0 ) statsPassedTime = 1.0; // avoid division by zero
   if( totalRunTime < 1.0 ) totalRunTime = 1.0; // avoid division by zero
   double poolDiff = GetPrimeDifficulty(nBitsShare);
   double blockDiff = GetPrimeDifficulty(nBits);
   float bestChainSinceLaunch = GetChainDifficulty(primeStats.bestPrimeChainDifficultySinceLaunch);
   printf("\n\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\n");
   printf("New Block: %u - Diff: %.06f / %.06f\n", blockHeight, blockDiff, poolDiff);
   printf("Total/Valid shares: [ %d / %d ]  -  Max diff: %.06f\n", total_shares, valid_shares, bestChainSinceLaunch);
   for (int i = 6; i <= max(6,(int)bestChainSinceLaunch); i++)
   {
      double sharePerHour = ((double)primeStats.chainCounter[0][i] / totalRunTime) * 3600000.0;
      printf("%2dch/h: %8.02f - %u [ %u / %u / %u ]\n", // - Val: %0.06f\n", 
         i, sharePerHour, 
         primeStats.chainCounter[0][i],
         primeStats.chainCounter[1][i],
         primeStats.chainCounter[2][i],
         primeStats.chainCounter[3][i]//, 
      );
   }
   if (!bSoloMining)
      printf("Share Value submitted - Last Block/Total: %0.6f / %0.6f\n", primeStats.fBlockShareValue, primeStats.fTotalSubmittedShareValue);
   printf("Current Primorial Value: %u\n", primeStats.nPrimorialMultiplier);
   printf("\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\n");

   primeStats.fBlockShareValue = 0;
   multiplierSet.clear();
}

void PrintStat()
{
   if( workData.workEntry[0].dataIsValid )
   {
      double totalRunTime = (double)(GetTickCount() - primeStats.startTime);
      double statsPassedTime = (double)(GetTickCount() - primeStats.primeLastUpdate);
      if( statsPassedTime < 1.0 ) statsPassedTime = 1.0; // avoid division by zero
      if( totalRunTime < 1.0 ) totalRunTime = 1.0; // avoid division by zero
      double primesPerSecond = (double)primeStats.primeChainsFound / (statsPassedTime / 1000.0);
      float avgCandidatesPerRound = (double)primeStats.nCandidateCount / primeStats.nSieveRounds;
      float sievesPerSecond = (double)primeStats.nSieveRounds / (statsPassedTime / 1000.0);

      uint32 bestDifficulty = primeStats.bestPrimeChainDifficulty;
      primeStats.bestPrimeChainDifficulty = 0;
      float primeDifficulty = GetChainDifficulty(bestDifficulty);
      float bestChainSinceLaunch = GetChainDifficulty(primeStats.bestPrimeChainDifficultySinceLaunch);
      float shareValuePerHour = primeStats.fShareValue / totalRunTime * 3600000.0;
      printf("\nVal/h:%8f - PPS:%d - SPS:%.03f - ACC:%d\n", shareValuePerHour, (sint32)primesPerSecond, sievesPerSecond, (sint32)avgCandidatesPerRound);
      printf(" Chain/Hr: ");

      for(int i=6; i<=10; i++)
      {
         printf("%2d: %8.02f ", i, ((double)primeStats.chainCounter[0][i] / totalRunTime) * 3600000.0);
      }
      if (bestChainSinceLaunch >= 11)
      {
         printf("\n           ");
         for(int i=11; i<=15; i++)
         {
            printf("%2d: %8.02f ", i, ((double)primeStats.chainCounter[0][i] / statsPassedTime) * 3600000.0);
         }
      }
      printf("\n\n");
   }
}

/*
* Mainloop when using getblocktemplate mode
*/
int jhMiner_main_gbtMode()
{
   // main thread, query work every x seconds
   sint32 loopCounter = 0;
   while( true )
   {
      // query new work
      jhMiner_queryWork_primecoin_getblocktemplate();
      // calculate stats every second tick
      if( loopCounter&1 )
      {
         PrintStat();
         primeStats.primeLastUpdate = GetTickCount();
         primeStats.primeChainsFound = 0;
         primeStats.nCandidateCount = 0;
         primeStats.nSieveRounds = 0;
      }		
      // wait and check some stats
      uint32 time_updateWork = GetTickCount();
      while( true )
      {
         if (appQuitSignal)
            return 0;
         uint32 passedTime = GetTickCount() - time_updateWork;
         if( passedTime >= 4000 )
            break;
         int currentBlockCount = getBlockTemplateData.height;
         if (currentBlockCount != lastBlockCount && lastBlockCount > 0)
         {	
            serverData_t* serverData = (serverData_t*)workData.workEntry[0].serverData; 				
            // update serverData
            serverData->nBitsForShare = getBlockTemplateData.nBits;
            serverData->blockHeight = getBlockTemplateData.height;
            OnNewBlock(serverData->nBitsForShare, serverData->nBitsForShare, serverData->blockHeight);
            lastBlockCount = currentBlockCount;
            break;
         }
         lastBlockCount = currentBlockCount;
         Sleep(200);
      }
      loopCounter++;
   }
   return 0;
}

/*
* Mainloop when using getwork() mode
*/
int jhMiner_main_getworkMode()
{
   // main thread, query work every x seconds
   sint32 loopCounter = 0;
   while( true )
   {
      // query new work
      jhMiner_queryWork_primecoin_getwork();
      // calculate stats every second tick
      if( loopCounter&1 )
      {		
         PrintStat();
         primeStats.primeLastUpdate = GetTickCount();
         primeStats.primeChainsFound = 0;
         primeStats.nCandidateCount = 0;
         primeStats.nSieveRounds = 0;
      }		
      // wait and check some stats
      uint32 time_updateWork = GetTickCount();
      while( true )
      {
         if (appQuitSignal)
            return 0;
         uint32 passedTime = GetTickCount() - time_updateWork;
         if( passedTime >= 4000 )
            break;

         int currentBlockCount = queryLocalPrimecoindBlockCount(useLocalPrimecoindForLongpoll);

         if (currentBlockCount != lastBlockCount && lastBlockCount > 0)
         {	
            serverData_t * serverData = (serverData_t*)workData.workEntry[0].serverData; 				
            OnNewBlock( serverData->nBitsForShare, serverData->nBitsForShare, serverData->blockHeight);
            lastBlockCount = currentBlockCount;
            break;
         }
         lastBlockCount = currentBlockCount;

         Sleep(200);
      }
      loopCounter++;
   }
   return 0;
}



/*
* Mainloop when using xpt mode
*/
int jhMiner_main_xptMode()
{
   // main thread, don't query work, just wait and process
   sint32 loopCounter = 0;
   uint32 xptWorkIdentifier = 0xFFFFFFFF;
   //unsigned long lastFiveChainCount = 0;
   //unsigned long lastFourChainCount = 0;
   while( true )
   {
      // calculate stats every ~30 seconds
      if( loopCounter % 10 == 0 )
      {
         PrintStat();
         primeStats.primeLastUpdate = GetTickCount();
         primeStats.primeChainsFound = 0;
         primeStats.nCandidateCount = 0;
         primeStats.nSieveRounds = 0;
      }
      // wait and check some stats
      uint32 time_updateWork = GetTickCount();
      while( true )
      {
         if (appQuitSignal)
            return 0;
         uint32 tickCount = GetTickCount();
         uint32 passedTime = tickCount - time_updateWork;

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
               OnNewBlock( workData.xptClient->blockWorkInfo.nBitsShare, workData.xptClient->blockWorkInfo.nBits, workData.xptClient->blockWorkInfo.height);
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
   SYSTEM_INFO sysinfo;
   GetSystemInfo( &sysinfo );
   commandlineInput.numThreads = sysinfo.dwNumberOfProcessors;
   commandlineInput.numThreads = max(commandlineInput.numThreads, 1);
   commandlineInput.sieveSize = 1024000; // default maxSieveSize
   commandlineInput.sievePercentage = 10; // default 
   commandlineInput.roundSievePercentage = 70; // default 
   commandlineInput.enableCacheTunning = false;
   commandlineInput.L1CacheElements = 256000;
   commandlineInput.primorialMultiplier = 0; // for default 0 we will swithc aouto tune on
   commandlineInput.targetOverride = 0;
   commandlineInput.targetBTOverride = 0;
   commandlineInput.sieveExtensions = 7;
   commandlineInput.printDebug = 0;
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
   // if set, validate xpm address
   if( commandlineInput.xpmAddress )
   {
      DecodeBase58(commandlineInput.xpmAddress, decodedWalletAddress, &decodedWalletAddressLen);
      sha256_context ctx;
      uint8 addressValidationHash[32];
      sha256_starts(&ctx);
      sha256_update(&ctx, (uint8*)decodedWalletAddress, 20+1);
      sha256_finish(&ctx, addressValidationHash);
      sha256_starts(&ctx); // is this line needed?
      sha256_update(&ctx, addressValidationHash, 32);
      sha256_finish(&ctx, addressValidationHash);
      if( *(uint32*)addressValidationHash != *(uint32*)(decodedWalletAddress+21) )
      {
         printf("Address '%s' is not a valid wallet address.\n", decodedWalletAddress);
         exit(-2);
      }
   }
   // print header
   printf("\n");
   printf("\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB\n");
   printf("\xBA  jhPrimeMiner - mod by rdebourbon -v3.4beta                   \xBA\n");
   printf("\xBA     merged with hg5fm (mumus) v8.0 build                      \xBA\n");
   printf("\xBA  author: JH (http://ypool.net)                                \xBA\n");
   printf("\xBA  contributors: x3maniac                                       \xBA\n");
   printf("\xBA  Credits: Sunny King for the original Primecoin client&miner  \xBA\n");
   printf("\xBA  Credits: mikaelh for the performance optimizations           \xBA\n");
   printf("\xBA                                                               \xBA\n");
   printf("\xBA  Donations:                                                   \xBA\n");
   printf("\xBA        XPM: AUwKMCYCacE6Jq1rsLcSEHSNiohHVVSiWv                \xBA\n");
   printf("\xBA        LTC: LV7VHT3oGWQzG9EKjvSXd3eokgNXj6ciFE                \xBA\n");
   printf("\xBA        BTC: 1Fph7y622HJ5Cwq4bkzfeZXWep2Jyi5kp7                \xBA\n");
   printf("\xC8\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBC\n");
   printf("Launching miner...\n");
   // set priority lower so the user still can do other things
   SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
   // init memory speedup (if not already done in preMain)
   //mallocSpeedupInit();
   if( pctx == NULL )
      pctx = BN_CTX_new();
   // init prime table
   GeneratePrimeTable(commandlineInput.sievePrimeLimit);
   printf("Sieve Percentage: %u %%\n", nSievePercentage);
   // init winsock
   WSADATA wsa;
   WSAStartup(MAKEWORD(2,2),&wsa);
   // init critical section
   InitializeCriticalSection(&workData.cs);
   // connect to host
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
      printf("Connecting to '%s' (%d.%d.%d.%d)\n", commandlineInput.host, ((ip>>0)&0xFF), ((ip>>8)&0xFF), ((ip>>16)&0xFF), ((ip>>24)&0xFF));
   }
   // setup RPC connection data (todo: Read from command line)
   jsonRequestTarget.ip = ipText;
   jsonRequestTarget.port = commandlineInput.port;
   jsonRequestTarget.authUser = commandlineInput.workername;
   jsonRequestTarget.authPass = commandlineInput.workerpass;
   // init stats
   primeStats.primeLastUpdate = primeStats.startTime = GetTickCount();
   primeStats.shareFound = false;
   primeStats.shareRejected = false;
   primeStats.primeChainsFound = 0;
   primeStats.foundShareCount = 0;
   for(int i = 0; i < sizeof(primeStats.chainCounter[0])/sizeof(uint32);  i++)
   {
      primeStats.chainCounter[0][i] = 0;
      primeStats.chainCounter[1][i] = 0;
      primeStats.chainCounter[2][i] = 0;
      primeStats.chainCounter[3][i] = 0;
   }
   primeStats.fShareValue = 0;
   primeStats.fBlockShareValue = 0;
   primeStats.fTotalSubmittedShareValue = 0;
   primeStats.nWaveTime = 0;
   primeStats.nWaveRound = 0;
   //primeStats.nL1CacheElements = 256000;

   // setup thread count and print info
   printf("Using %d threads\n", commandlineInput.numThreads);
   printf("Username: %s\n", jsonRequestTarget.authUser);
   printf("Password: %s\n", jsonRequestTarget.authPass);
   // decide protocol
   if( commandlineInput.port == 10034 || commandlineInput.useXPT )
   {
      // port 10034 indicates xpt protocol (in future we will also add a -o URL prefix)
      workData.protocolMode = MINER_PROTOCOL_XPUSHTHROUGH;
      printf("Using x.pushthrough protocol\n");
   }
   else
   {
      if( useGetBlockTemplate )
      {
         workData.protocolMode = MINER_PROTOCOL_GBT;
         // getblocktemplate requires a valid xpm address to be set
         if( commandlineInput.xpmAddress == NULL )
         {
            printf("GetBlockTemplate mode requires -xpm parameter\n");
            exit(-3);
         }
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
   }
   // initial query new work / create new connection
   if( workData.protocolMode == MINER_PROTOCOL_GETWORK )
   {
      jhMiner_queryWork_primecoin_getwork();
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
   printf("Note: While the initial auto tuning is in progress several values cannot be changed.\n");


   // start the Auto Tuning thread
   CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CacheAutoTuningWorkerThread, (LPVOID)commandlineInput.enableCacheTunning, 0, 0);
   CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RoundSieveAutoTuningWorkerThread, NULL, 0, 0);
   CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)input_thread, NULL, 0, 0);


   std::map<DWORD, HANDLE> threadMap;
   threadHearthBeat = (DWORD *)malloc( commandlineInput.numThreads * sizeof(DWORD));
   // start threads
   for(sint32 threadIdx=0; threadIdx<commandlineInput.numThreads; threadIdx++)
   {
      HANDLE hThread;
      if( workData.protocolMode == MINER_PROTOCOL_GETWORK )
         hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)jhMiner_workerThread_getwork, (LPVOID)threadIdx, 0, 0);
      else if( workData.protocolMode == MINER_PROTOCOL_GBT )
         hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)jhMiner_workerThread_gbt, (LPVOID)threadIdx, 0, 0);
      else if( workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH )
         hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)jhMiner_workerThread_xpt, (LPVOID)threadIdx, 0, 0);

      SetThreadPriority(hThread, THREAD_PRIORITY_BELOW_NORMAL);
      threadMap.insert(thMapKeyVal(threadIdx,hThread));
      threadHearthBeat[threadIdx] = GetTickCount();
   }

   CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)watchdog_thread, (LPVOID)&threadMap, 0, 0);

   // enter different mainloops depending on protocol mode
   if( workData.protocolMode == MINER_PROTOCOL_GBT )
      return jhMiner_main_gbtMode();
   else if( workData.protocolMode == MINER_PROTOCOL_GETWORK )
      return jhMiner_main_getworkMode();
   else if( workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH )
      return jhMiner_main_xptMode();

   return 0;
}
