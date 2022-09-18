#include "Arduino.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <WiFi.h>
#include <WiFiType.h>
#include "Esp.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_task_wdt.h"
#include "time.h"
#include <vector>
#include "Schalter.h"
#include "NukiOeffner.h"
#include "Common.h"
#include "html.h"
#include "WetterDaten.h"
#include <esp_now.h>
#include <esp_wifi.h>
#include <AsyncTCP.h>
#include "HAMWebserver.h"
#include <AsyncElegantOTA.h>

#define TAG "MODULE1"

#define MINUTE (60 * 1000)
#define STUNDE (60 * MINUTE)
#define TAGE (24 * STUNDE)
#define JAHR (365 * TAGE)

char ssid[] = SSID_MACRO; // your network SSID (name)

char pass[] = PASS_MACRO; // your network password (use for WPA, or use as key for WEP)

std::vector<Raum *> vecRaum;

//WiFiServer server(80);
HAMWebserver hamServer(80);

// connect to UDP returns true if successful or false if not
//void ws_server(void *pvParameters);
bool inited = false;
bool TIME_OBTAINED = false;
bool REFRESH_IP = true;
int64_t lastCheck;
WetterDaten daten;
String requestHost;
String requestHost2;
Schalter *pGlobalSchalter;
boolean unlockNuki = false;
boolean unlockHauptEingang = false;
NukiOeffner *pGlobalNukiOeffner = nullptr;
TasterSchalter *pHauptEingangTuer = nullptr;
uint32_t chipidLow;

void setup()
{
  Serial.begin(115200);
	uint64_t chipid = ESP.getEfuseMac();
	Serial.printf("ESP32 Chip ID = %04X\n", (uint16_t)(chipid >> 32)); // print High 2 bytes
	Serial.printf("%08X\n", (uint32_t)chipid);						 // print Low 4bytes.
	chipidLow = (chipid >> 32) % 65536;
	Serial.printf("chipidLow %d\n", chipidLow);
	Raum *raum;
	Serial.printf("Connecting to \n");
	Serial.printf("ESP_HWID %llu\n", ESP.getEfuseMac());
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, pass);

	Serial.printf("Wi-Fi Channel: %d\n", WiFi.channel());

	if ((chipidLow == 54686))
	{ // module 1
		Serial.printf("module 1 init\n");
		raum = new Raum("Zimmer Jayde");
		vecRaum.push_back(raum);
		raum->vecSchaltern.push_back(new LichtSchalter(13, 4, "Jayde"));
		raum->vecSchaltern.push_back(
			new RolloSchalter(14, 17, 12, 16, 600, 2000, "Jayde"));
		raum = new Raum("Arbeitszimmer");
		vecRaum.push_back(raum);
		raum->vecSchaltern.push_back(new LichtSchalter(27, 15, "AZ"));
		raum->vecSchaltern.push_back(
			new RolloSchalter(26, 0, 25, 2, 600, 2000, "AZ"));
		raum = new Raum("Elternzimmer");
		vecRaum.push_back(raum);
		raum->vecSchaltern.push_back(new LichtSchalter(33, 5, "EZ"));
		raum->vecSchaltern.push_back(
			new RolloSchalter(22, 19, 32, 18, 530, 2000, "EZ"));
	} // 4610366690621131300 247728676

	if ((chipidLow == 20500))
	{ // module 2
		Serial.printf("module2 init\n");
		raum = new Raum("Wohnzimmer");
		vecRaum.push_back(raum);
		raum->vecSchaltern.push_back(new LichtSchalter(13, 15, "Mitte"));
		raum->vecSchaltern.push_back(new LichtSchalter(12, 2, "Fenster"));
		raum->vecSchaltern.push_back(new LichtSchalter(14, 0, "Küche"));
		raum->vecSchaltern.push_back(
			new RolloSchalter(26, 4, 27, 16, 500, 2000, "Fenster"));
		raum->vecSchaltern.push_back(
			new RolloSchalter(25, 17, 33, 5, 500, 2000, "Tür"));
		raum = new Raum("Gang");
		vecRaum.push_back(raum);
		raum->vecSchaltern.push_back(new AutoLichtSchalter(32, 18, "Gang"));
		raum->vecSchaltern.push_back(
			new RolloSchalter(22, 19, 23, 21, 500, 2000, "Gang"));
	}
	if ((chipidLow == 21614) || (chipidLow == 15363) ||  (chipidLow == 23573)||(chipidLow == 31852))
	{ // module 3
		intEspNow();
		Serial.printf("module3 init\n");
		raum = new Raum("Haupteingang");
		vecRaum.push_back(raum);
		pHauptEingangTuer = new TasterSchalter(13, 4, "Tür");
		raum->vecSchaltern.push_back(pHauptEingangTuer);
		raum = new Raum("Wohnung");
		vecRaum.push_back(raum);
		pGlobalNukiOeffner = new NukiOeffner("192.168.0.88", 8080, 823727629, SECTOKEN_MACRO, "Tür");
		raum->vecSchaltern.push_back(pGlobalNukiOeffner);
	}
	raum = new Raum("Global");
	pGlobalSchalter = new GlobalSchalter();
	raum->vecSchaltern.push_back(pGlobalSchalter);
	vecRaum.push_back(raum);
	Serial.printf("First Init \n");
	for (auto itr : vecRaum)
	{
		itr->Init();
	}

	Serial.printf("Start Server\n");
	AsyncElegantOTA.begin(&hamServer);
	hamServer.begin();
	lastCheck = hmMillis();
}

void loop()
{
	if (((lastCheck + MINUTE) < hmMillis()) || (lastCheck > hmMillis()))
	{
		if (WiFi.status() != WL_CONNECTED)
		{
			WiFi.reconnect();
			REFRESH_IP = true;
		}
		lastCheck = hmMillis();
	}
	if (WiFi.status() == WL_CONNECTED)
	{
		if (!TIME_OBTAINED)
		{	
			obtain_time();
			TIME_OBTAINED = true;
			for (auto itr : vecRaum)
			{
				itr->Init();
			}
		}
		if(REFRESH_IP){
			requestHost = WiFi.localIP().toString();
			Serial.printf("Server IP %s\n", requestHost.c_str());
			Serial.print("Mac Address:");
			Serial.println(WiFi.macAddress());
			REFRESH_IP = false;
		}
		// daten.Check();
	}
	for (auto itrRaum : vecRaum)
	{
		for (auto itr : itrRaum->vecSchaltern)
		{
			itr->CheckIO();
		}
	}

	for (auto itrRaum : vecRaum)
	{
		for (auto itr : itrRaum->vecSchaltern)
		{
			itr->Switch();
		}
	}
  if(unlockNuki == true){
    Serial.printf( "unlockNuki abgefeuert begin\n");
    pGlobalNukiOeffner->Unlock();
    unlockNuki = false;
    Serial.printf("unlockNuki abgefeuert ende\n");
  }
  if(unlockHauptEingang == true){
    Serial.printf("unlockHauptEingang abgefeuert begin\n");
    pHauptEingangTuer->an();
    unlockHauptEingang = false;
    Serial.printf("unlockHauptEingang abgefeuert ende\n");
  }
	vTaskDelay(100 / portTICK_PERIOD_MS);
}

