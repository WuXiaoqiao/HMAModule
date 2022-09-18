#include "Schalter.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "ArduinoJson.h"

class NukiOeffner: public Schalter {
protected:
	std::string ip;
	int32_t port;
	long nukiID; 
	std::string secToken;
    void lockList();
    void lockState();
    void Lock();
    String getUrl();
    int HttpRequest(DynamicJsonDocument& doc, String url);
public:
    void Unlock();
	NukiOeffner(std::string ip, int32_t port, long nukiID, std::string secToken,  std::string bezeichnung);
	virtual ~NukiOeffner(){}
	void CheckIO(){}
	void Switch(){}
	void Init(){}
	void ProcessCommand(std::string cmd, AsyncResponseStream& client);
};