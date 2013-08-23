#include <curl/curl.h>
#include <string.h>

#include <sstream>
#include <iostream>

size_t static curl_write_callback_func(void *buffer, size_t size, size_t nmemb, void *userp);

/* the function to invoke as the data recieved */
size_t static curl_write_callback_func(void *buffer, size_t size, size_t nmemb, void *userp)
{
	std::string buf = std::string(static_cast<char *>(buffer), size * nmemb);
    std::stringstream *response = static_cast<std::stringstream *>(userp);
    response->write(buf.c_str(), (std::streamsize)buf.size());

    return size * nmemb;
}