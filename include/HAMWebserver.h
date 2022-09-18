#define CONFIG_ASYNC_TCP_RUNNING_CORE 1
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

class HAMWebserver : public AsyncWebServer
{

public:
    HAMWebserver(uint16_t port=81);
    static void addHeader(AsyncResponseStream *response);
    static void notFound(AsyncWebServerRequest *request);
    static void PutOperations(AsyncResponseStream *response);
    static void PutSettings(AsyncResponseStream *response);
    static void mainPage(AsyncWebServerRequest *request);
    static void GET_OPERATIONS(AsyncWebServerRequest *request);
    static void GET_SETTINGS(AsyncWebServerRequest *request);
    static void ExecuteCommand(AsyncWebServerRequest *request);
};
