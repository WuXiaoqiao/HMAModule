#include "NukiOeffner.h"
#include <HTTPClient.h>
#include "base64.h"

#define TAG "NUKIOEFFNER"

enum NUKI_DoorState : byte
{
  unavailable = 0,
  deactivated = 1,
  door_closed = 2,
  door_opened = 3,
  door_state_unknown = 4,
  calibrating = 5
};
enum NUKI_lock_states : byte
{
  state_uncalibrated = 0,
  state_locked = 1,
  state_unlocking = 2,
  state_unlocked = 3,
  state_locking = 4,
  state_unlatched = 5,
  state_unlocked_lockngo = 6,
  state_unlatching = 7,
  state_motor_blocked = 254,
  state_undefined = 255
};

enum NUKI_lock_actions : byte
{
  action_unlock = 1,
  action_lock = 2,
  action_unlatch = 3,
  action_lockngo = 4,
  action_lockngo_with_unlatch = 5
};

enum NUKI_http_responses : int
{
  response_invalid_action = 400,
  response_invalid_token = 401,
  response_authentication_disabled = 403,
  response_samrtlock_unknown = 404,
  response_smartlock_offline = 503
};

extern String requestHost;

NukiOeffner::NukiOeffner(std::string ip, int32_t port, long nukiID, std::string secToken, std::string bezeichnung)
{
  this->ip = ip;
  this->port = port;
  this->nukiID = nukiID;
  this->secToken = secToken;
  this->bezeichnung = bezeichnung;
  operationen.push_back(
      std::make_pair("aufschliessen", new std::string(TOSTRING(LIGHT_ON))));
  operationen.push_back(
      std::make_pair("abschliessen", new std::string(TOSTRING(LIGHT_OFF))));
}

void NukiOeffner::ProcessCommand(std::string cmd, AsyncResponseStream &client)
{
  Serial.printf(" NukiOeffner::ProcessCommand: \n\"%s\"", cmd.c_str());
  if (cmd == "aufschliessen")
  {
    this->Unlock();
  }
  if (cmd == "abschliessen")
  {
    this->Lock();
  }
  PutOperations(client, requestHost);
  client.println();
  client.flush();
}


int NukiOeffner::HttpRequest(DynamicJsonDocument &doc, String url)
{
  // Send HTTP GET request
  HTTPClient http;
  http.setTimeout(30000);
  ESP_LOGI(TAG,"HTTP Request : %s", url.c_str());
  http.begin(url.c_str());
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0)
  {
    ESP_LOGI(TAG,"HTTP Response code: %d", httpResponseCode);
    deserializeJson(doc, http.getString());
  }
  else
  {
    ESP_LOGI(TAG,"Error code: %d", httpResponseCode);
  }
  // Free resources
  http.end();
  return httpResponseCode;
}
String NukiOeffner::getUrl()
{
  char buffer[1024];
  sprintf(buffer, "http://%s:%d", ip.c_str(), port);
  String s = buffer;
  return s;
}
void NukiOeffner::lockList()
{
  String lockListUrl = this->getUrl() + "/list?token=" + secToken.c_str();
  DynamicJsonDocument doc(1024);
  if (HttpRequest(doc, lockListUrl) == 200)
  {
    JsonArray arr = doc.as<JsonArray>();
    // using C++11 syntax (preferred):
    for (JsonObject value : arr)
    {
      if (value["nukiId"] == nukiID)
      {
        String output;
        serializeJson(value, output);
        Serial.println(output);
      }
    }
  }
  else
  {
    Serial.println("lockList Failed");
  }
}

void NukiOeffner::lockState()
{
  String lockStateUrl = this->getUrl() + "/lockState?token=6d2c6ed04e5073480b2a&nukiID=823727629";
  DynamicJsonDocument doc(1024);
  if (HttpRequest(doc, lockStateUrl) == 200)
  {
    String output;
    serializeJson(doc, output);
    Serial.println(output);
  }
  else
  {
    Serial.println("lockState Failed");
  }
}

void NukiOeffner::Unlock()
{
  String lockStateUrl = this->getUrl() + "/lockState?token=6d2c6ed04e5073480b2a&nukiID=823727629";
  DynamicJsonDocument doc(1024);
  if (HttpRequest(doc, lockStateUrl) == 200)
  {
    String output;
    serializeJson(doc, output);
    Serial.printf("LockState Result: %s\n", output.c_str());
    if (byte(doc["doorsensorState"]) == door_closed)
    {
      char buffer[1024];
      sprintf(buffer , "%s/lockAction?token=6d2c6ed04e5073480b2a&nukiID=823727629&action=%d", this->getUrl().c_str(), action_unlatch);//aufmachen
      //sprintf(buffer, "%s/lockAction?token=6d2c6ed04e5073480b2a&nukiID=823727629&action=%d", this->getUrl().c_str(), action_unlock);
      String unlockUrl = buffer;
      Serial.printf("unlockUrl: %s\n", unlockUrl.c_str());
      if (HttpRequest(doc, unlockUrl) != 200)
      {
        Serial.printf("unlock Failed\n");
      }
    }
  }
  else
  {
    Serial.printf("unlock Failed\n");
  }
}

void NukiOeffner::Lock()
{
  String lockStateUrl = this->getUrl() + "/lockState?token=6d2c6ed04e5073480b2a&nukiID=823727629";
  DynamicJsonDocument doc(1024);
  if (HttpRequest(doc, lockStateUrl) == 200)
  {
    String output;
    serializeJson(doc, output);
    Serial.println(output);
    if (byte(doc["state"]) == NUKI_lock_states::state_unlocked && byte(doc["doorsensorState"]) == door_closed)
    {
      Serial.println("Start unlock");
      char buffer[1024];
      sprintf(buffer, "%s/lockAction?token=6d2c6ed04e5073480b2a&nukiID=823727629&action=%d", this->getUrl().c_str(), action_lock);
      String lockUrl(buffer);
      if (HttpRequest(doc, lockUrl) != 200)
      {
        Serial.println("Lock Failed");
      }
    }
    else
    {
      Serial.println("Lock Failed LockState");
    }
  }
}

long rannumber;
String hash(long randomnumber)
{
	char payload[256];
	sprintf(payload, "Hello SHA 256 from ESP32 %ld", randomnumber);
	byte shaResult[32];
	mbedtls_md_context_t ctx;
	mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
	const size_t payloadLength = strlen(payload);
	mbedtls_md_init(&ctx);
	mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
	mbedtls_md_starts(&ctx);
	mbedtls_md_update(&ctx, (const unsigned char *)payload, payloadLength);
	mbedtls_md_finish(&ctx, shaResult);
	mbedtls_md_free(&ctx);
	String base64Hash = base64::encode(shaResult, 32);
	return base64Hash;
}
// callback function that will be executed when data is received
// Create a struct_message called myData
struct_message myData;
extern boolean unlockHauptEingang;
extern boolean unlockNuki;
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
	Serial.printf("OnDataRecv Bytes received: %d\n", len);
	Serial.printf("Mac Address: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	if (sizeof(myData) == len)
	{
		memcpy(&myData, incomingData, sizeof(myData));
		Serial.printf("Char: %s\n", myData.pwd);
		Serial.printf("Long: %ld\n", myData.number);
		String msg = myData.pwd;
		if (msg == "Hello")
		{
			rannumber = random(LONG_MAX);
			myData.number = rannumber;
			esp_err_t result = esp_now_send(mac, (uint8_t *) &myData, sizeof(myData));

			if (result == ESP_OK) {
				ESP_LOGI(TAG,"Sent with success");
			}
			else {
				ESP_LOGI(TAG,"Error sending the data %d", result);
			}
		}
		else
		{
			if (hash(rannumber) == myData.pwd)
			{
				Serial.printf("Türe auf %s\r\n\n", myData.tuer);
        if(myData.tuer[0] =='W'){
          Serial.printf("Wohnungstür\n");
				  unlockNuki = true;
        }
        if(myData.tuer[0] =='H'){
          Serial.printf("Haupttür\n");
          unlockHauptEingang = true;
        }
			}
		}
	}
	else
	{
		Serial.printf("Wrong MessageSize: %d, myData size %d\r\n\n", len, sizeof(myData));
	}
	Serial.printf("\r\n\n");
}