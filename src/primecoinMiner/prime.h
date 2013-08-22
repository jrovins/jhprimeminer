// Copyright (c) 2013 Primecoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PRIMECOIN_PRIME_H
#define PRIMECOIN_PRIME_H

// Removed USE_ROTATE - Testing revealed while it was a good approximation that ran faster it
// left too many candidates in sieve that cost more on testing than a full sieve operation costs.
//#if defined(__i386__) || defined(_M_IX86) || defined(_X86_) || defined(__x86_64__) || defined(_M_X64)
//#define USE_ROTATE
//#endif
#include <cstdlib>

//#include "main.h"
#ifdef _WIN32
#include "mpirxx.h"
#else
#include <gmpxx.h>
#endif

extern unsigned int nMaxSieveSize;
extern unsigned int nSievePercentage;
extern bool nPrintDebugMessages;
extern unsigned long nOverrideTargetValue;
extern unsigned int nOverrideBTTargetValue;
static const uint256 hashBlockHeaderLimit = (uint256(1) << 255);
static const CBigNum bnOne = 1;
static const CBigNum bnTwo = 2;
static const CBigNum bnConst8 = 8;
static const CBigNum bnPrimeMax = (bnOne << 2000) - 1;
static const CBigNum bnPrimeMin = (bnOne << 255);
static const mpz_class mpzOne = 1;
static const mpz_class mpzTwo = 2;
static const mpz_class mpzConst8 = 8;
static const mpz_class mpzPrimeMax = (mpzOne << 2000) - 1;
static const mpz_class mpzPrimeMin = (mpzOne << 255);


extern unsigned int nTargetInitialLength;
extern unsigned int nTargetMinLength;
extern DWORD * threadHearthBeat;

// Prime Table
//std::vector<unsigned int> vPrimes;
//uint32* vPrimes;
extern uint32* vPrimesTwoInverse;
extern uint32 vPrimesSize;
extern std::vector<unsigned int> vPrimes;

// Generate small prime table
void GeneratePrimeTable(unsigned int nSieveSize);
// Get next prime number of p
//bool PrimeTableGetNextPrime(unsigned int* p);
bool PrimeTableGetNextPrime(unsigned int& p);
// Get previous prime number of p
bool PrimeTableGetPreviousPrime(unsigned int& p);

// Compute primorial number p#
void BNPrimorial(unsigned int p, CBigNum& bnPrimorial);
void Primorial(unsigned int p, mpz_class& mpzPrimorial);
unsigned int PrimorialFast(unsigned int p);
// Compute the first primorial number greater than or equal to bn
//void PrimorialAt(CBigNum& bn, CBigNum& bnPrimorial);
void PrimorialAt(mpz_class& bn, mpz_class& mpzPrimorial);

// Test probable prime chain for: bnPrimeChainOrigin
// fFermatTest
//   true - Use only Fermat tests
//   false - Use Fermat-Euler-Lagrange-Lifchitz tests
// Return value:
//   true - Probable prime chain found (one of nChainLength meeting target)
//   false - prime chain too short (none of nChainLength meeting target)
//bool ProbablePrimeChainTest(const CBigNum& bnPrimeChainOrigin, unsigned int nBits, bool fFermatTest, unsigned int& nChainLengthCunningham1, unsigned int& nChainLengthCunningham2, unsigned int& nChainLengthBiTwin);
bool ProbablePrimeChainTest(const mpz_class& bnPrimeChainOrigin, unsigned int nBits, bool fFermatTest, unsigned int& nChainLengthCunningham1, unsigned int& nChainLengthCunningham2, unsigned int& nChainLengthBiTwin, bool fullTest =false);
bool ProbablePrimeChainTestOrig(const mpz_class& bnPrimeChainOrigin, unsigned int nBits, bool fFermatTest, unsigned int& nChainLengthCunningham1, unsigned int& nChainLengthCunningham2, unsigned int& nChainLengthBiTwin, bool fullTest =false);


static const unsigned int nFractionalBits = 24;
static const unsigned int TARGET_FRACTIONAL_MASK = (1u<<nFractionalBits) - 1;
static const unsigned int TARGET_LENGTH_MASK = ~TARGET_FRACTIONAL_MASK;
static const uint64 nFractionalDifficultyMax = (1ull << (nFractionalBits + 32));
static const uint64 nFractionalDifficultyMin = (1ull << 32);
static const uint64 nFractionalDifficultyThreshold = (1ull << (8 + 32));
static const unsigned int nWorkTransitionRatio = 32;
unsigned int TargetGetLimit();
unsigned int TargetGetInitial();
unsigned int TargetGetLength(unsigned int nBits);
bool TargetSetLength(unsigned int nLength, unsigned int& nBits);
unsigned int TargetGetFractional(unsigned int nBits);
uint64 TargetGetFractionalDifficulty(unsigned int nBits);
bool TargetSetFractionalDifficulty(uint64 nFractionalDifficulty, unsigned int& nBits);
std::string TargetToString(unsigned int nBits);
unsigned int TargetFromInt(unsigned int nLength);
bool TargetGetMint(unsigned int nBits, uint64& nMint);
bool TargetGetNext(unsigned int nBits, uint64_t nInterval, uint64_t nTargetSpacing, uint64_t nActualSpacing, unsigned int& nBitsNext);

// Mine probable prime chain of form: n = h * p# +/- 1
//bool MineProbablePrimeChain(CBlock& block, CBigNum& bnFixedMultiplier, bool& fNewBlock, unsigned int& nTriedMultiplier, unsigned int& nProbableChainLength, unsigned int& nTests, unsigned int& nPrimesHit);

// Check prime proof-of-work
enum // prime chain type
{
	PRIME_CHAIN_CUNNINGHAM1 = 1u,
	PRIME_CHAIN_CUNNINGHAM2 = 2u,
	PRIME_CHAIN_BI_TWIN     = 3u
};
// bool CheckPrimeProofOfWork(uint256 hashBlockHeader, unsigned int nBits, const CBigNum& bnPrimeChainMultiplier, unsigned int& nChainType, unsigned int& nChainLength);

// prime target difficulty value for visualization
double GetPrimeDifficulty(unsigned int nBits);
// Estimate work transition target to longer prime chain
unsigned int EstimateWorkTransition(unsigned int nPrevWorkTransition, unsigned int nBits, unsigned int nChainLength);
// prime chain type and length value
std::string GetPrimeChainName(unsigned int nChainType, unsigned int nChainLength);

/*
// Sieve of Eratosthenes for proof-of-work mining
class CSieveOfEratosthenes
{
	unsigned int nSieveSize; // size of the sieve
	unsigned int nBits; // target of the prime chain to search for
	mpz_class hashBlockHeader; // block header hash
	mpz_class mpzFixedFactor; // fixed factor to derive the chain
	CBigNum bnFixedFactor; // fixed factor to derive the chain

	// bitmaps of the sieve, index represents the variable part of multiplier
	//std::vector<bool> vfCompositeCunningham1;
	//std::vector<bool> vfCompositeCunningham2;
	//std::vector<bool> vfCompositeBiTwin;

	CAutoBN_CTX pctx;
	//BIGNUM bn_constTwo;
	mpz_t mpzTwo;
	uint32 bignumData_constTwo[0x200/4];
	//BIGNUM bn_nDelta1;
	//uint32 bignumData_constTwo[0x200/4];

	unsigned int nPrimeSeq; // prime sequence number currently being processed
	unsigned int nCandidateMultiplier; // current candidate for power test


public:
	//bool* vfCompositeCunningham1;
	//bool* vfCompositeCunningham2;
	//bool* vfCompositeBiTwin;
	unsigned int nSievePercentage;

	uint8* vfCompositeCunningham1;
	uint8* vfCompositeCunningham2;
	uint8* vfCompositeBiTwin;

	CSieveOfEratosthenes(unsigned int nSieveSize, unsigned int nBits, mpz_class& hashBlockHeader, mpz_class& bnFixedMultiplier)
	{
		this->nSieveSize = nSieveSize;
		this->nBits = nBits;
		this->hashBlockHeader = hashBlockHeader;
		this->mpzFixedFactor = bnFixedMultiplier * hashBlockHeader;
		nPrimeSeq = 0;
		uint32 maskBytes = (nMaxSieveSize+7)/8;
		vfCompositeCunningham1 = (uint8*)malloc(sizeof(uint8)*maskBytes);
		RtlZeroMemory(vfCompositeCunningham1, sizeof(uint8)*maskBytes);
		vfCompositeCunningham2 = (uint8*)malloc(sizeof(uint8)*maskBytes);
		RtlZeroMemory(vfCompositeCunningham2, sizeof(uint8)*maskBytes);
		vfCompositeBiTwin = (uint8*)malloc(sizeof(uint8)*maskBytes);
		RtlZeroMemory(vfCompositeBiTwin, sizeof(uint8)*maskBytes);
		nCandidateMultiplier = 0;
		// init bn_constTwo
		mpz_init_set_ui(mpzTwo, 2);
		//bn_constTwo.d = (unsigned int*)bignumData_constTwo;
		//bn_constTwo.dmax = 0x200/4;
		//bn_constTwo.flags = BN_FLG_STATIC_DATA;
		//bn_constTwo.neg = 0; 
		//bn_constTwo.top = 1; 
		//BN_set_word(&bn_constTwo, 2);
		nSievePercentage = 10;
	}



	~CSieveOfEratosthenes()
	{
		free(vfCompositeCunningham1);
		free(vfCompositeCunningham2);
		free(vfCompositeBiTwin);
		mpz_clear(mpzTwo);
	}

	// Get total number of candidates for power test
	unsigned int GetCandidateCount()
	{
		unsigned int nCandidates = 0;
		for (unsigned int nMultiplier = 0; nMultiplier < nSieveSize; nMultiplier++)
		{
			uint32 byteIdx = nMultiplier>>3;
			uint32 mask = 1<<(nMultiplier&7);
			if (!(vfCompositeCunningham1[byteIdx]&mask) ||
				!(vfCompositeCunningham2[byteIdx]&mask) ||
				!(vfCompositeBiTwin[byteIdx]&mask))
				nCandidates++;
		}
		return nCandidates;
	}

	// Scan for the next candidate multiplier (variable part)
	// Return values:
	//   True - found next candidate; nVariableMultiplier has the candidate
	//   False - scan complete, no more candidate and reset scan
	bool GetNextCandidateMultiplier(unsigned int& nVariableMultiplier)
	{
		for(;;)
		{
			nCandidateMultiplier++;
			if (nCandidateMultiplier >= nSieveSize)
			{
				nCandidateMultiplier = 0;
				return false;
			}
			uint32 byteIdx = nCandidateMultiplier>>3;
			uint32 mask = 1<<(nCandidateMultiplier&7);

			if (!(vfCompositeCunningham1[byteIdx]&mask) ||
				!(vfCompositeCunningham2[byteIdx]&mask) ||
				!(vfCompositeBiTwin[byteIdx]&mask))
			{
				nVariableMultiplier = nCandidateMultiplier;
				return true;
			}
		}
	}

	// Weave the sieve for the next prime in table
	// Return values:
	//   True  - weaved another prime; nComposite - number of composites removed
	//   False - sieve already completed
	bool WeaveOriginal();
	//bool WeaveFast();
	//bool WeaveFast2();
	bool WeaveFastAllBN();
	bool WeaveFastAll();

	// bool WeaveAlt();
};

*/

	// Sieve of Eratosthenes for proof-of-work mining
class CSieveOfEratosthenes
{
    unsigned int nSieveSize; // size of the sieve
	unsigned int nAllocatedSieveSize;
    mpz_class mpzHash; // hash of the block header
    mpz_class mpzFixedMultiplier; // fixed round multiplier

#ifdef _M_X64
   // final set of candidates for probable primality checking
   uint64 *vfCandidates;
   uint64 *vfCandidateBiTwin;
   uint64 *vfCandidateCunningham1;
   //uint64 *vfCandidateCunningham2;

   // bitsets that can be combined to obtain the final bitset of candidates
   uint64 *vfCompositeCunningham1A;
   uint64 *vfCompositeCunningham1B;
   uint64 *vfCompositeCunningham2A;
   uint64 *vfCompositeCunningham2B;
   static const unsigned int nWordBits = 8 * sizeof(uint64);	
#else
    // final set of candidates for probable primality checking
    unsigned long *vfCandidates;
    unsigned long *vfCandidateBiTwin;
    unsigned long *vfCandidateCunningham1;
   //unsigned long *vfCandidateCunningham2;
	
   // bitsets that can be combined to obtain the final bitset of candidates
   unsigned long *vfCompositeCunningham1A;
   unsigned long *vfCompositeCunningham1B;
   unsigned long *vfCompositeCunningham2A;
   unsigned long *vfCompositeCunningham2B;
    static const unsigned int nWordBits = 8 * sizeof(unsigned long);	
#endif

    unsigned int nCandidatesWords;
    unsigned int nCandidatesBytes;

   unsigned int nCCMultiplierBytes;
   unsigned int nBTMultiplierBytes;
   unsigned int nAllocatedCCMultiplierBytes;
   unsigned int nAllocatedBTMultiplierBytes;
   unsigned int *vCunningham1AMultipliers;
   unsigned int *vCunningham1BMultipliers;
   unsigned int *vCunningham2AMultipliers;
   unsigned int *vCunningham2BMultipliers;

    unsigned int nPrimeSeq; // prime sequence number currently being processed
    unsigned int nCandidateCount; // cached total count of candidates
    unsigned int nCandidateMultiplier; // current candidate for power test
    
    unsigned int nChainLength;
   unsigned int nBTChainLength;
   unsigned int nCCChainLength;
    unsigned int nPrimes;
    
    //CBlockIndex* pindexPrev;
    
   __inline unsigned int GetWordNum(unsigned int nBitNum) {
        return nBitNum / nWordBits;
    }
       
   __inline void AddMultiplier(unsigned int *vMultipliers, const unsigned int nArrayOffset, const unsigned int nMultiplierPos, const unsigned int nPrimeSeq, const unsigned int nSolvedMultiplier);


#ifdef _M_X64
   __inline uint64  GetBitMask(unsigned int nBitNum) {
      return (uint64)1UL << (nBitNum % nWordBits);
    }

   void ProcessMultiplier(uint64 *vfComposites,const unsigned int nArrayOffset, const unsigned int nMinMultiplier, const unsigned int nMaxMultiplier, const std::vector<unsigned int>& vPrimes, unsigned int *vMultipliers)
    {
        // Wipe the part of the array first
      memset(vfComposites + GetWordNum(nMinMultiplier), 0, (nMaxMultiplier - nMinMultiplier + nWordBits - 1) / nWordBits * sizeof(uint64));
#else
   __inline unsigned long  GetBitMask(unsigned int nBitNum) {
      return 1UL << (nBitNum % nWordBits);
    }

   void ProcessMultiplier(unsigned long *vfComposites,const unsigned int nArrayOffset, const unsigned int nMinMultiplier, const unsigned int nMaxMultiplier, const std::vector<unsigned int>& vPrimes, unsigned int *vMultipliers)
    {
        // Wipe the part of the array first
      memset(vfComposites + GetWordNum(nMinMultiplier), 0, (nMaxMultiplier - nMinMultiplier + nWordBits - 1) / nWordBits * sizeof(unsigned long));
#endif

        for (unsigned int nPrimeSeq = 1; nPrimeSeq < nPrimes; nPrimeSeq++)
        {
            const unsigned int nPrime = vPrimes[nPrimeSeq];
#ifdef USE_ROTATE
            const unsigned int nRotateBits = nPrime % nWordBits;
         for (unsigned int i = 0; i < nArrayOffset; i++)
            {
            unsigned int nVariableMultiplier = vMultipliers[nPrimeSeq * nArrayOffset + i];
            if (nVariableMultiplier == 0xFFFFFFFF) continue;
                unsigned long lBitMask = GetBitMask(nVariableMultiplier);
                for (; nVariableMultiplier < nMaxMultiplier; nVariableMultiplier += nPrime)
                {
                    vfComposites[GetWordNum(nVariableMultiplier)] |= lBitMask;
                    lBitMask = (lBitMask << nRotateBits) | (lBitMask >> (nWordBits - nRotateBits));
                }
            vMultipliers[nPrimeSeq * nArrayOffset + i] = nVariableMultiplier;
            }
#else
         for (unsigned int i = 0; i < nArrayOffset; i++)
            {
            unsigned int nVariableMultiplier = vMultipliers[nPrimeSeq * nArrayOffset + i];
            if (nVariableMultiplier == 0xFFFFFFFF) continue;
                for (; nVariableMultiplier < nMaxMultiplier; nVariableMultiplier += nPrime)
                {
                    vfComposites[GetWordNum(nVariableMultiplier)] |= GetBitMask(nVariableMultiplier);
                }
            vMultipliers[nPrimeSeq * nArrayOffset + i] = nVariableMultiplier;
            }
#endif
        }
    }

public:

	unsigned int nSievePercentage;

   CSieveOfEratosthenes(unsigned int nSieveSize, unsigned int nTargetChainLength, unsigned int nTargetBTLength, mpz_class& mpzHash, mpz_class& mpzFixedMultiplier)
    {
      nSievePercentage = 16;
        this->nSieveSize = nSieveSize;
		this->nAllocatedSieveSize = nSieveSize;
        this->mpzHash = mpzHash;
        this->mpzFixedMultiplier = mpzFixedMultiplier;
      this->nChainLength = nTargetChainLength;
      this->nBTChainLength = nTargetBTLength;
      this->nCCChainLength = nChainLength - nBTChainLength;
      this->nPrimes = (uint64)vPrimesSize * nSievePercentage / 100;
        //this->pindexPrev = pindexPrev;
        nPrimeSeq = 0;
        nCandidateCount = 0;
        nCandidateMultiplier = 0;
        nCandidatesWords = (nSieveSize + nWordBits - 1) / nWordBits;
#ifdef _M_X64
      nCandidatesBytes = nCandidatesWords * sizeof(uint64);
      vfCandidates = (uint64 *)malloc(nCandidatesBytes);
      vfCandidateBiTwin = (uint64 *)malloc(nCandidatesBytes);
      vfCandidateCunningham1 = (uint64 *)malloc(nCandidatesBytes);
      //vfCandidateCunningham2 = (uint64 *)malloc(nCandidatesBytes);
      vfCompositeCunningham1A = (uint64 *)malloc(nCandidatesBytes);
      vfCompositeCunningham1B = (uint64 *)malloc(nCandidatesBytes);
      vfCompositeCunningham2A = (uint64 *)malloc(nCandidatesBytes);
      vfCompositeCunningham2B = (uint64 *)malloc(nCandidatesBytes);
#else
      nCandidatesBytes = nCandidatesWords * sizeof(unsigned long);
      vfCandidates = (unsigned long *)malloc(nCandidatesBytes);
      vfCandidateBiTwin = (unsigned long *)malloc(nCandidatesBytes);
      vfCandidateCunningham1 = (unsigned long *)malloc(nCandidatesBytes);
      //vfCandidateCunningham2 = (unsigned long *)malloc(nCandidatesBytes);
      vfCompositeCunningham1A = (unsigned long *)malloc(nCandidatesBytes);
      vfCompositeCunningham1B = (unsigned long *)malloc(nCandidatesBytes);
      vfCompositeCunningham2A = (unsigned long *)malloc(nCandidatesBytes);
      vfCompositeCunningham2B = (unsigned long *)malloc(nCandidatesBytes);
#endif
        memset(vfCandidates, 0, nCandidatesBytes);
        memset(vfCandidateBiTwin, 0, nCandidatesBytes);
        memset(vfCandidateCunningham1, 0, nCandidatesBytes);
      //memset(vfCandidateCunningham2, 0, nCandidatesBytes);
      //memset(vfCompositeCunningham1A, 0, nCandidatesBytes);
      //memset(vfCompositeCunningham1B, 0, nCandidatesBytes);
      //memset(vfCompositeCunningham2A, 0, nCandidatesBytes);
      //memset(vfCompositeCunningham2B, 0, nCandidatesBytes);

      nBTMultiplierBytes = vPrimesSize * nBTChainLength * sizeof(unsigned int);
      nCCMultiplierBytes = vPrimesSize * nCCChainLength * sizeof(unsigned int);
      nAllocatedBTMultiplierBytes = nBTMultiplierBytes;
      nAllocatedCCMultiplierBytes = nCCMultiplierBytes;
      vCunningham1AMultipliers = (unsigned int *)malloc(nBTMultiplierBytes);
      vCunningham2AMultipliers = (unsigned int *)malloc(nBTMultiplierBytes);
      vCunningham1BMultipliers = (unsigned int *)malloc(nCCMultiplierBytes);
      vCunningham2BMultipliers = (unsigned int *)malloc(nCCMultiplierBytes);
      memset(vCunningham1AMultipliers, 0xFF, nBTMultiplierBytes);
      memset(vCunningham2AMultipliers, 0xFF, nBTMultiplierBytes);
      memset(vCunningham1BMultipliers, 0xFF, nCCMultiplierBytes);
      memset(vCunningham2BMultipliers, 0xFF, nCCMultiplierBytes);
    }
    
    ~CSieveOfEratosthenes()
    {
        free(vfCandidates);
		free(vfCandidateBiTwin);
		free(vfCandidateCunningham1);
      //free(vfCandidateCunningham2);
      free(vfCompositeCunningham1A);
      free(vfCompositeCunningham1B);
      free(vfCompositeCunningham2A);
      free(vfCompositeCunningham2B);
      free(vCunningham1AMultipliers);
      free(vCunningham1BMultipliers);
      free(vCunningham2AMultipliers);
      free(vCunningham2BMultipliers);
    }


   void Init(unsigned int nSieveSize, unsigned int nTargetChainLength, unsigned int nTargetBTLength, mpz_class& mpzHash, mpz_class& mpzFixedMultiplier, unsigned int nSiPercentage)
    {
        this->nSieveSize = nSieveSize;
      //this->nBits = nBits;
        this->mpzHash = mpzHash;
        this->mpzFixedMultiplier = mpzFixedMultiplier;
      this->nSievePercentage = nSiPercentage;
      this->nChainLength = nTargetChainLength;
      this->nBTChainLength = nTargetBTLength;
      this->nCCChainLength = nChainLength - nBTChainLength;
      this->nPrimes = (uint64)vPrimesSize * nSievePercentage / 100;
        //this->pindexPrev = pindexPrev;
        nPrimeSeq = 0;
        nCandidateCount = 0;
        nCandidateMultiplier = 0;
        nCandidatesWords = (nSieveSize + nWordBits - 1) / nWordBits;
#ifdef _M_X64
      nCandidatesBytes = nCandidatesWords * sizeof(uint64);
#else
      nCandidatesBytes = nCandidatesWords * sizeof(unsigned long);
#endif
		if (nSieveSize > nAllocatedSieveSize)
		{
			free(vfCandidates);
			free(vfCandidateBiTwin);
			free(vfCandidateCunningham1);
         //free(vfCandidateCunningham2);
         free(vfCompositeCunningham1A);
         free(vfCompositeCunningham1B);
         free(vfCompositeCunningham2A);
         free(vfCompositeCunningham2B);
#ifdef _M_X64
         vfCandidates = (uint64 *)malloc(nCandidatesBytes);
         vfCandidateBiTwin = (uint64 *)malloc(nCandidatesBytes);
         vfCandidateCunningham1 = (uint64 *)malloc(nCandidatesBytes);
         //vfCandidateCunningham2 = (uint64 *)malloc(nCandidatesBytes);
         vfCompositeCunningham1A = (uint64 *)malloc(nCandidatesBytes);
         vfCompositeCunningham1B = (uint64 *)malloc(nCandidatesBytes);
         vfCompositeCunningham2A = (uint64 *)malloc(nCandidatesBytes);
         vfCompositeCunningham2B = (uint64 *)malloc(nCandidatesBytes);
#else
         vfCandidates = (unsigned long *)malloc(nCandidatesBytes);
         vfCandidateBiTwin = (unsigned long *)malloc(nCandidatesBytes);
         vfCandidateCunningham1 = (unsigned long *)malloc(nCandidatesBytes);
         //vfCandidateCunningham2 = (unsigned long *)malloc(nCandidatesBytes);
         vfCompositeCunningham1A = (unsigned long *)malloc(nCandidatesBytes);
         vfCompositeCunningham1B = (unsigned long *)malloc(nCandidatesBytes);
         vfCompositeCunningham2A = (unsigned long *)malloc(nCandidatesBytes);
         vfCompositeCunningham2B = (unsigned long *)malloc(nCandidatesBytes);
#endif
			nAllocatedSieveSize = nSieveSize;
		}
        memset(vfCandidates, 0, nCandidatesBytes);
        memset(vfCandidateBiTwin, 0, nCandidatesBytes);
        memset(vfCandidateCunningham1, 0, nCandidatesBytes);
      //memset(vfCandidateCunningham2, 0, nCandidatesBytes);
      //memset(vfCompositeCunningham1A, 0, nCandidatesBytes);
      //memset(vfCompositeCunningham1B, 0, nCandidatesBytes);
      //memset(vfCompositeCunningham2A, 0, nCandidatesBytes);
      //memset(vfCompositeCunningham2B, 0, nCandidatesBytes);

      nBTMultiplierBytes = vPrimesSize * nBTChainLength * sizeof(unsigned int);
      if (nBTMultiplierBytes > nAllocatedBTMultiplierBytes)
      {
         free(vCunningham1AMultipliers);
         free(vCunningham2AMultipliers);
         vCunningham1AMultipliers = (unsigned int *)malloc(nBTMultiplierBytes);
         vCunningham2AMultipliers = (unsigned int *)malloc(nBTMultiplierBytes);
         nAllocatedBTMultiplierBytes = nBTMultiplierBytes;
      }
      nCCMultiplierBytes = vPrimesSize * nCCChainLength * sizeof(unsigned int);
      if (nCCMultiplierBytes > nAllocatedCCMultiplierBytes)
      {
         free(vCunningham1BMultipliers);
         free(vCunningham2BMultipliers);
         vCunningham1BMultipliers = (unsigned int *)malloc(nCCMultiplierBytes);
         vCunningham2BMultipliers = (unsigned int *)malloc(nCCMultiplierBytes);
         nAllocatedCCMultiplierBytes = nCCMultiplierBytes;
      }
      memset(vCunningham1AMultipliers, 0xFF, nBTMultiplierBytes);
      memset(vCunningham2AMultipliers, 0xFF, nBTMultiplierBytes);
      memset(vCunningham1BMultipliers, 0xFF, nCCMultiplierBytes);
      memset(vCunningham2BMultipliers, 0xFF, nCCMultiplierBytes);
    }


    // Get total number of candidates for power test
    unsigned int GetCandidateCount()
    {
      //if (nCandidateCount)
      //   return nCandidateCount;

        unsigned int nCandidates = 0;
#ifdef __GNUC__
        for (unsigned int i = 0; i < nCandidatesWords; i++)
        {
            nCandidates += __builtin_popcountl(vfCandidates[i]);
        }
#else
        for (unsigned int i = 0; i < nCandidatesWords; i++)
        {
#ifdef _M_X64
         uint64 lBits = vfCandidates[i];
#else
         unsigned long lBits = vfCandidates[i];
#endif
            for (unsigned int j = 0; j < nWordBits; j++)
            {
                nCandidates += (lBits & 1UL);
                lBits >>= 1;
            }
        }
#endif
      //nCandidateCount = nCandidates;
        return nCandidates;
    }

    // Scan for the next candidate multiplier (variable part)
    // Return values:
    //   True - found next candidate; nVariableMultiplier has the candidate
    //   False - scan complete, no more candidate and reset scan
    bool GetNextCandidateMultiplier(unsigned int& nVariableMultiplier, unsigned int& nCandidateType)
    {
      unsigned int lWordNum = GetWordNum(nCandidateMultiplier);

#ifdef _M_X64
      uint64 lBits = vfCandidates[lWordNum];
      uint64 lBitMask;
#else
      unsigned long lBits = vfCandidates[lWordNum];
      unsigned long lBitMask;
#endif
        for(;;)
        {
            nCandidateMultiplier++;
            if (nCandidateMultiplier >= nSieveSize)
            {
                nCandidateMultiplier = 0;
                return false;
            }
            if (nCandidateMultiplier % nWordBits == 0)
            {
            lWordNum = GetWordNum(nCandidateMultiplier);
            lBits = vfCandidates[lWordNum];
                if (lBits == 0)
                {
                    // Skip an entire word
                    nCandidateMultiplier += nWordBits - 1;
                    continue;
                }
            }
         lBitMask = GetBitMask(nCandidateMultiplier);
         if (lBits & lBitMask)
            {

                nVariableMultiplier = nCandidateMultiplier;
				if (vfCandidateBiTwin[GetWordNum(nCandidateMultiplier)] & GetBitMask(nCandidateMultiplier))
                    nCandidateType = PRIME_CHAIN_BI_TWIN;
                else if (vfCandidateCunningham1[GetWordNum(nCandidateMultiplier)] & GetBitMask(nCandidateMultiplier))
                    nCandidateType = PRIME_CHAIN_CUNNINGHAM1;
                else
                    nCandidateType = PRIME_CHAIN_CUNNINGHAM2;
                return true;
            }
        }
    }

    // Get progress percentage of the sieve
    unsigned int GetProgressPercentage();

    // Weave the sieve for the next prime in table
    // Return values:
    //   True  - weaved another prime; nComposite - number of composites removed
    //   False - sieve already completed
    bool Weave();

   void SetSievePercentage(unsigned int nNewSiPercentage)
   {
      this->nSievePercentage = nNewSiPercentage;
      this->nPrimes = (uint64)vPrimesSize * nSievePercentage / 100;
   }
};

inline void mpz_set_uint256(mpz_t r, uint256& u)
{
    mpz_import(r, 32 / sizeof(unsigned long), -1, sizeof(unsigned long), -1, 0, &u);
}


#endif
