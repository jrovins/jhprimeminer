#include <cstdlib>
#include <curl/curl.h>
#include <json/json.h>
#include <sstream>
#include <iostream>


void csNotifyStats();
void csNotifySettings(bool runtime);
void csNotifyShare(uint32 shareErrorCode, float shareValue, char* rejectReason);
int64_t csGetUUID();

bool loadConfigJSON(std::string configdata,bool runtime);
bool saveConfigJSON();

std::string curl_request(std::string url,std::string postdata = "", bool mode = false);
