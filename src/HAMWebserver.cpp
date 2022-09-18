
#include <freertos/FreeRTOS.h>
#include "HAMWebserver.h"
#include "Schalter.h"

#define TAG "HAMWebserver"

extern std::vector<Raum *> vecRaum;
extern String requestHost;
void HAMWebserver::addHeader(AsyncResponseStream *response)
{
    response->addHeader("Access-Control-Allow-Origin", "*");
}

void HAMWebserver::notFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
}

void HAMWebserver::PutOperations(AsyncResponseStream *response)
{
    for (auto itrRaum : vecRaum)
    {
        itrRaum->PutOperations(*response, requestHost);
    }
}

void HAMWebserver::PutSettings(AsyncResponseStream *response)
{
    for (auto itrRaum : vecRaum)
    {
        itrRaum->PutSettings(*response, requestHost);
    }
}


extern uint32_t chipidLow;
void PutInfo(AsyncResponseStream *response)
{
	response->print(TOSTRING(HTML_INFO));
	response->print("<table>");
	char buffer[500];
	time_t now = hmMillis() / 1000;
	struct tm *ti = gmtime(&now);
	response->print("");
	snprintf(buffer, 500,
			 "<tr><th>Time since restart</th><td>%d Tage %02d:%02d:%02d</td>",
			 ti->tm_yday, ti->tm_hour, ti->tm_min, ti->tm_sec);
	response->print(buffer);
	response->print("<th>Local Time</th><td>");
	tm tmnow = Schalter::GetTime();
	strftime(buffer, sizeof(buffer), "%X", &tmnow);
	response->print(buffer);
	response->print("</td></tr>");

	for (auto itrRaum : vecRaum)
	{
		for (auto itr : itrRaum->vecSchaltern)
		{
			if (itr->Putinfo(buffer, 500))
			{
				response->print(buffer);
			}
		}
	}
    response->printf("<tr><th>Low ID</th><td>%d</td><th>MAC</th><td>", chipidLow);
    response->print(WiFi.macAddress());
    response->print("</td></tr>");
	response->print("</table>");
}

void HAMWebserver::mainPage(AsyncWebServerRequest *request)
{
    AsyncResponseStream *response = request->beginResponseStream("text/html;charset=utf-8");
    HAMWebserver::addHeader(response);
    Serial.printf("Write Response mainPage\n");
    response->print(TOSTRING(HTML_HEAD_TOP));
    char buffer[1024];
    std::string ip1 = "192.168.0.103";
    //std::string ip1 = "192.168.0.168";
    std::string ip2 = "192.168.0.104";
    std::string ip3 = "192.168.0.105";
    // std::string ip3 = "192.168.0.125";

    snprintf(buffer, 1024,
             TOSTRING(HTML_HEAD_CENTER), ip1.c_str(),
             ip2.c_str());

    response->print(buffer);
    response->print(TOSTRING(HTML_HEAD_BOTTOM));
    snprintf(buffer, 1024, TOSTRING(HTML_BODY),
             ip1.c_str(), ip2.c_str(), ip3.c_str(),
             requestHost.c_str());

    response->print(buffer);
    PutInfo(response);
    response->print(TOSTRING(HTML_FOOTER));
    // break out of the while loop:
    request->send(response);
}

void HAMWebserver::GET_OPERATIONS(AsyncWebServerRequest *request)
{
    Serial.printf("Write Response GET_OPERATIONS\n");
    AsyncResponseStream *response = request->beginResponseStream("text/html;charset=utf-8");
    HAMWebserver::addHeader(response);
    PutOperations(response);
    request->send(response);
}

void HAMWebserver::GET_SETTINGS(AsyncWebServerRequest *request)
{
    AsyncResponseStream *response = request->beginResponseStream("text/html;charset=utf-8");
    HAMWebserver::addHeader(response);
    // PutSettings(client);
}

void HAMWebserver::ExecuteCommand(AsyncWebServerRequest *request)
{
    AsyncResponseStream *response = request->beginResponseStream("text/html;charset=utf-8");
    HAMWebserver::addHeader(response);
    Schalter *pSchalter = NULL;
    String sensorNumber = request->pathArg(0);
    String action = request->pathArg(1);
    Serial.printf("Write Response ExecuteCommand schalter: %s, action %s\n", sensorNumber.c_str(), action.c_str());
    unsigned int currNumber = sensorNumber.toInt();

    for (auto itrRaum : vecRaum)
    {
        for (auto itr : itrRaum->vecSchaltern)
        {
            boolean found = strcmp(
                                itr->bezeichnung.c_str(),
                                action.c_str()) == 0;
            unsigned int bezAddr =
                (unsigned int)&(itr->bezeichnung);
            if ((bezAddr == currNumber) || found)
            {
                pSchalter = itr;
                Serial.printf(" Found: \n\"%s\"",
                         action.c_str());
                break;
            }
        }
        if (pSchalter != NULL)
        {
            pSchalter->ProcessCommand(std::string(action.c_str()), *response);
            Serial.printf(" ProcessCommand: \n\"%s\"", action.c_str());
            break;
        }
    }
    request->send(response);
}

HAMWebserver::HAMWebserver(uint16_t port) : AsyncWebServer(port)
{
    this->on("/", HTTP_GET, mainPage);
    this->on("/GET_OPERATIONS", HTTP_GET, GET_OPERATIONS);
    this->on("/GET_SETTINGS", HTTP_GET, GET_SETTINGS);
    this->on("^/([0-9]+)/(.+)/$", HTTP_GET, ExecuteCommand);
}