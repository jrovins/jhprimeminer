#include <cstdlib>



void notifyCentralServerofShare(uint32 shareErrorCode, float shareValue, char* rejectReason);
void NEWnotifyCentralServerofShare(uint32 shareErrorCode, float shareValue, char* rejectReason);
void notifyStats();
void NEWnotifyStats();

void curl_post(std::string &sURL, std::string &postData);
