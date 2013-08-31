#include "global.h"
#include "ticker.h"
//#include<intrin.h>
#include<ctime>
#include<map>
#include <cstdlib>
#include <cstdio>
#include <iostream>


//curl
#include"curl_basic.h"



void notifyStats(){

	CURL *curl;
	CURLcode res;
 
	curl_global_init(CURL_GLOBAL_DEFAULT);
 
	curl = curl_easy_init();
	if(curl) {

		/* to keep the response */
		std::stringstream responseData;

		char sURL[512];
		// notify stats
			char chains[256] = "";
            for(int i=2; i<=std::max(6,(int)primeStats.bestPrimeChainDifficultySinceLaunch); i++){
				char t[20] = "";
				sprintf(t, "&ch[%d]=%d", i, primeStats.chainCounter[0][i]);
				strcat(chains, t);
			}
			
			sprintf(sURL, "%s/report.php?key=%s&workerid=%s&start=%llu&blockstart=%llu&now=%llu&val=%g&primes=%u&sieves=%g&candidates=%g&shares=%u%s", commandlineInput.centralServer, commandlineInput.csApiKey, curl_easy_escape(curl, jsonRequestTarget.authUser, 0), 
				primeStats.startTime, primeStats.primeLastUpdate, (uint64)getTimeMilliseconds(), primeStats.fShareValue, primeStats.primeChainsFound, primeStats.nSieveRounds, primeStats.nCandidateCount, valid_shares, chains );
			printf("URL Tried: %s", sURL);
		curl_easy_setopt(curl, CURLOPT_URL, sURL);

	//	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	 //   curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
 
		/* setting a callback function to return the data */
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback_func);

		/* passing the pointer to the response as the callback parameter */
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);

		//curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		
		/* Perform the request, res will get the return code */ 
		res = curl_easy_perform(curl);
		/* Check for errors */ 
		//if(res != CURLE_OK)
		//	fprintf(stderr, "curl_easy_perform() failed: %s\n",
		//		curl_easy_strerror(res));

		/* always cleanup */ 
		curl_easy_cleanup(curl);

		std::cout << std::endl << "Reponse from server: " << responseData.str() << std::endl;

			}
 
	curl_global_cleanup();


}


void NEWnotifyStats(){
	
	//			sprintf(sURL, "%s/report.php?key=%s&workerid=%s&start=%llu&blockstart=%llu&now=%llu&val=%g&primes=%u&sieves=%g&candidates=%g&shares=%u%s", 
	//			commandlineInput.centralServer, commandlineInput.csApiKey, curl_easy_escape(curl, jsonRequestTarget.authUser, 0), 
	//			primeStats.startTime, primeStats.primeLastUpdate, (uint64)getTimeMilliseconds(), primeStats.fShareValue, primeStats.primeChainsFound, 
	//			primeStats.nSieveRounds, primeStats.nCandidateCount, valid_shares, chains );

	
	fStr_buffer4kb_t fStrBuffer_parameter;
		fStr_t* fStr_parameter = fStr_alloc(&fStrBuffer_parameter, FSTR_FORMAT_UTF8);
		fStr_appendFormatted(fStr_parameter, "[\"key\":\"%s\"", commandlineInput.csApiKey); 
		fStr_appendFormatted(fStr_parameter, ",\"workerid\":\"%s\"", jsonRequestTarget.authUser);
		fStr_appendFormatted(fStr_parameter, ",\"start\":\"%u\"", primeStats.startTime);
		fStr_appendFormatted(fStr_parameter, ",\"blockstart\":\"%u\"", primeStats.primeLastUpdate);
		fStr_appendFormatted(fStr_parameter, ",\"now\":\"%u\"", (uint64)getTimeMilliseconds());
		fStr_appendFormatted(fStr_parameter, ",\"val\":\"%hf\"", primeStats.fShareValue);
		fStr_appendFormatted(fStr_parameter, ",\"primes\":\"%u\"", primeStats.primeChainsFound);
		fStr_appendFormatted(fStr_parameter, ",\"sieves\":\"%u\"", primeStats.nSieveRounds);
		fStr_appendFormatted(fStr_parameter, ",\"candidates\":\"%u\"", primeStats.nCandidateCount);
		fStr_appendFormatted(fStr_parameter, ",\"shares\":\"%d\"", valid_shares);
		for(int i=2; i<=std::max(6,(int)primeStats.bestPrimeChainDifficultySinceLaunch); i++){
			fStr_appendFormatted(fStr_parameter,",\"ch[%d]\":\"%d\"", i, primeStats.chainCounter[0][i]);
		}
	
		fStr_append(fStr_parameter, "]"); // finish constructing the request

		printf("JSON String: %s\n", fStr_get(fStr_parameter));

		printf("Stats server: %s\n", statsRequestTarget.ip);
		printf("Stats port: %u\n", statsRequestTarget.port);


		// send request
		sint32 rpcErrorCode = 0;
		char* command = "shareFound";
		jsonObject_t* jsonReturnValue = jsonClient_request(&statsRequestTarget, command, fStr_parameter, &rpcErrorCode);
		if( jsonReturnValue == NULL )
		{
			printf("Notify Stats failed :(\n");
		}
		else
		{
			// rpc call worked, sooooo.. is the server happy with the result?
			jsonObject_t* jsonReturnValueBool = jsonObject_getParameter(jsonReturnValue, "result");
			if( jsonObject_isTrue(jsonReturnValueBool) )
			{
				//jsonObject_t* jsonResult = jsonObject_getParameter(jsonReturnValue, "result");
				//jsonObject_t* jsonResult_data = jsonObject_getParameter(jsonResult, "data");
				//jsonObject_t* jsonResult_hash1 = jsonObject_getParameter(jsonResult, "hash1");
//				jsonObject_t* jsonResult_target = jsonObject_getParameter(jsonResult, "target");   unused?
				//jsonObject_t* jsonResult_serverData = jsonObject_getParameter(jsonResult, "serverData");
				//jsonObject_t* jsonResult_algorithm = jsonObject_getParameter(jsonResult, "algorithm");



		jsonObject_freeObject(jsonReturnValue);
			}
			else
			{
				// the server says no to this share :(
	//			printf("Server rejected share (BlockHeight: %u/%u nBits: 0x%08uX)\n", primecoinBlock->serverData.blockHeight, jhMiner_getCurrentWorkBlockHeight(primecoinBlock->threadIndex), primecoinBlock->serverData.client_shareBits);
				jsonObject_freeObject(jsonReturnValue);
			}
		}
//		jsonObject_freeObject(jsonReturnValue);


}


void notifyCentralServerofShare(uint32 shareErrorCode, float shareValue, char* rejectReason)
{
		CURL *curl;
	CURLcode res;
 
	curl_global_init(CURL_GLOBAL_DEFAULT);
 
	curl = curl_easy_init();
	if(curl) {

		/* to keep the response */
		std::stringstream responseData;

		char sURL[256];

		bool bValidShare;

	if( shareErrorCode == 0 ){
	
		bValidShare=true;
	
	}
	else{
	
		bValidShare=false;

	}


	//hostname, api key, machine id, validShare, shareValue, rejectReason
		sprintf(sURL, "http://%s/report.php?key=%s&workerid=%s&validshare=%d&sharevalue=%g&rejectreason=%s", commandlineInput.centralServer, curl_easy_escape(curl, commandlineInput.csApiKey, 0), curl_easy_escape(curl, jsonRequestTarget.authUser, 0), bValidShare, shareValue, curl_easy_escape(curl, rejectReason , 0));
			printf("URL Tried: %s", sURL);

		curl_easy_setopt(curl, CURLOPT_URL, sURL);

	//	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	 //   curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
 
		/* setting a callback function to return the data */
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback_func);

		/* passing the pointer to the response as the callback parameter */
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);

		//curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		
		/* Perform the request, res will get the return code */ 
		res = curl_easy_perform(curl);
		/* Check for errors */ 
		//if(res != CURLE_OK)
		//	fprintf(stderr, "curl_easy_perform() failed: %s\n",
		//		curl_easy_strerror(res));

		/* always cleanup */ 
		curl_easy_cleanup(curl);

		std::cout << std::endl << "Reponse from server: " << responseData.str() << std::endl;

			}
 
	curl_global_cleanup();
}


void NEWnotifyCentralServerofShare(uint32 shareErrorCode, float shareValue, char* rejectReason)
{
	printf("Running new notify to %s:%d \n",statsRequestTarget.ip,statsRequestTarget.port);
	//build the stats into a nice json bundle

		double statsPassedTime = getTimeMilliseconds() - primeStats.primeLastUpdate;
		if (statsPassedTime < 0) statsPassedTime *= -1;
		if( statsPassedTime < 1.0 )
			statsPassedTime = 1.0; // avoid division by zero
		double primesPerSecond = (double)primeStats.primeChainsFound / (statsPassedTime / 1000.0);

	// prepare buffer to send
			fStr_buffer4kb_t fStrBuffer_parameter;
		fStr_t* fStr_parameter = fStr_alloc(&fStrBuffer_parameter, FSTR_FORMAT_UTF8);
		fStr_appendFormatted(fStr_parameter, "[\"key\":\"%utf8\"", commandlineInput.csApiKey); 

		fStr_appendFormatted(fStr_parameter, ",\"workerid\":\"%utf8\"", jsonRequestTarget.authUser);

		bool bValidShare;
		if( shareErrorCode == 0 ){
			bValidShare=true;
		}else{
			bValidShare=false;
		}

		fStr_appendFormatted(fStr_parameter, ",\"validshare\":\"%B\"", bValidShare);
		//shareValue
		fStr_appendFormatted(fStr_parameter, ",\"shareValue\":\"%f\"", shareValue);

		//reject reason
		fStr_appendFormatted(fStr_parameter, ",\"rejectReason\":\"");
		if( rejectReason[0] != '\0' ){
			fStr_appendFormatted(fStr_parameter, "%utf8", rejectReason);
		}
		fStr_appendFormatted(fStr_parameter, "\"");



//		fStr_appendFormatted(fStr_parameter, ",\"%s\":\"%s\"","key","value");



		fStr_append(fStr_parameter, "]"); // finish constructing the request

		printf("JSON String: %s\n", fStr_parameter);

		// send request
		sint32 rpcErrorCode = 0;
		jsonObject_t* jsonReturnValue = jsonClient_request(&statsRequestTarget, "shareFound", fStr_parameter, &rpcErrorCode);
		if( jsonReturnValue == NULL )
		{
			printf("Notify Share failed :(\n");
		}
		else
		{
			// rpc call worked, sooooo.. is the server happy with the result?
			jsonObject_t* jsonReturnValueBool = jsonObject_getParameter(jsonReturnValue, "result");
			if( jsonObject_isTrue(jsonReturnValueBool) )
			{
				//jsonObject_t* jsonResult = jsonObject_getParameter(jsonReturnValue, "result");
				//jsonObject_t* jsonResult_data = jsonObject_getParameter(jsonResult, "data");
				//jsonObject_t* jsonResult_hash1 = jsonObject_getParameter(jsonResult, "hash1");
//				jsonObject_t* jsonResult_target = jsonObject_getParameter(jsonResult, "target");   unused?
				//jsonObject_t* jsonResult_serverData = jsonObject_getParameter(jsonResult, "serverData");
				//jsonObject_t* jsonResult_algorithm = jsonObject_getParameter(jsonResult, "algorithm");



		jsonObject_freeObject(jsonReturnValue);
			}
			else
			{
				// the server says no to this share :(
	//			printf("Server rejected share (BlockHeight: %u/%u nBits: 0x%08uX)\n", primecoinBlock->serverData.blockHeight, jhMiner_getCurrentWorkBlockHeight(primecoinBlock->threadIndex), primecoinBlock->serverData.client_shareBits);
				jsonObject_freeObject(jsonReturnValue);
			}
		}
		jsonObject_freeObject(jsonReturnValue);


/*	sint32 rpcErrorCode = 0;
	//uint32 time1 = GetTickCount();
//	jsonObject_t* jsonReturnValue = jsonClient_request(&jsonRequestTarget, "notifyStats", NULL, &rpcErrorCode);
	//uint32 time2 = GetTickCount() - time1;
	// printf("request time: %dms\n", time2);
	if( jsonReturnValue == NULL )
	{
		printf("centralServer notifyStats() failed with %serror code %d\n", (rpcErrorCode>1000)?"http ":"", rpcErrorCode>1000?rpcErrorCode-1000:rpcErrorCode);
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

		jsonObject_freeObject(jsonReturnValue);
	}
*/



}






void curl_post(std::string &sURL, std::string &postData){

			CURL *curl;
	CURLcode res;
 
	curl_global_init(CURL_GLOBAL_DEFAULT);
 
	curl = curl_easy_init();
	if(curl) {

		/* to keep the response */
		std::stringstream responseData;
		char* safeurl = curl_easy_escape(curl, sURL.c_str(), 0);
	//hostname, api key, machine id, validShare, shareValue, rejectReason
		curl_easy_setopt(curl, CURLOPT_URL, safeurl);
		
	//	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	 //   curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
 
		curl_easy_setopt(curl, CURLOPT_POST, 1);
		char* safepostData = curl_easy_escape(curl, postData.c_str(), 0);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS,  safepostData);
		/* setting a callback function to return the data */
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback_func);

		/* passing the pointer to the response as the callback parameter */
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);


		//curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		
		/* Perform the request, res will get the return code */ 
		res = curl_easy_perform(curl);
		/* Check for errors */ 
		//if(res != CURLE_OK)
		//	fprintf(stderr, "curl_easy_perform() failed: %s\n",
		//		curl_easy_strerror(res));

		/* always cleanup */ 
		curl_free(safeurl);
		curl_free(safepostData);
		curl_easy_cleanup(curl);

		std::cout << std::endl << "Reponse from server: " << responseData.str() << std::endl;

			}
 
	curl_global_cleanup();

}