/*
 * Schalter.cpp
 *
 *  Created on: 26.10.2017
 *      Author: Xiaoqiao
 */

#include "Schalter.h"
#include "sdkconfig.h"
#include "esp32-hal-gpio.h"
#include "esp_log.h"
#include "time.h"
#include "SomeOneAtHome.h"
#include "WetterDaten.h"
#include "html.h"

#define TAG "Schalter"

extern WetterDaten daten;
extern std::vector<Raum*> vecRaum;
extern String requestHost;

Kanal::Kanal(uint8_t in, uint8_t out) {
	this->in = in;
	this->out = out;
	this->state = HIGH;
	this->lastChangeTime = 0;
	this->autoOff = LONG_LONG_MAX;
	autoOn = LONG_LONG_MAX;
	this->retry = 0;
}

void Kanal::Init() {
	pinMode(out, OUTPUT);
	digitalWrite(out, state);
	pinMode(in, INPUT_PULLUP);
	Serial.printf("GPIO OUT: %d, Schalter: %d\n", out, state);
	Serial.printf("GPIO IN: %d, state: %d\n", in, digitalRead(in));
	lastChangeTime = hmMillis();
}

tm Schalter::GetTime() {
	time_t now;
	struct tm timeinfo;
	time(&now);
	localtime_r(&now, &timeinfo);
	return timeinfo;
}

int64_t Schalter::GetNextAutoTime(uint8_t hour, uint8_t min, uint8_t sec) {
	int64_t millsek = (((hour * 60 + min) * 60) + sec) * 1000;
	struct tm ti = GetTime();
	int64_t passed = ((ti.tm_hour * 60 + ti.tm_min) * 60 + ti.tm_sec) * 1000;
	if (millsek < passed) {
		millsek = hmMillis() + (24 * 60 * 60 * 1000) + millsek - passed;
		Serial.printf("GetNextAutoTime future hour %d, min: %d, sec: %d retval:%llu\n", hour, min, sec,millsek);

	} else {
		millsek = (millsek - passed) + hmMillis();
		Serial.printf("GetNextAutoTime past hour %d, min: %d, sec: %d retval:%llu\n", hour, min, sec, millsek);
	}
	return millsek;
}
////LichtSchalter begin

LichtSchalter::LichtSchalter(uint8_t in, uint8_t out, std::string bezeichnung) :
		SchalterKanal(in, out) {
	this->bezeichnung = bezeichnung;
	operationen.push_back(
			std::make_pair("an", new std::string(TOSTRING(LIGHT_ON))));
	operationen.push_back(
			std::make_pair("aus", new std::string(TOSTRING(LIGHT_OFF))));
}

LichtSchalter::~LichtSchalter() {

}

void LichtSchalter::CheckIO() {
	if (digitalRead(SchalterKanal.in) == LOW) {
		Serial.printf("Taster gedrückt GPIO IN: %d, GPIO OUT: %d, Schalter: %s\n",
				SchalterKanal.in, SchalterKanal.out, bezeichnung.c_str());
		if ((hmMillis() - SchalterKanal.lastChangeTime) > 250) {
			if (SchalterKanal.state == HIGH) {
				an();
			} else {
				aus();
			}
			while (digitalRead(SchalterKanal.in) == LOW) {
			}ESP_LOGI(TAG,
					"Taster losgelassen GPIO IN: %d, GPIO OUT: %d, Schalter: %s",
					SchalterKanal.in, SchalterKanal.out, bezeichnung.c_str());
		} else {
			int8_t state = SchalterKanal.state;
			for (auto itr : verbunden) {
				int64_t lastChangeTime = itr->SchalterKanal.lastChangeTime;
				if ((hmMillis() - lastChangeTime) > 250) {
					if (state == HIGH) {
						itr->aus();
					} else {
						itr->an();
					}
				}
			}
			while (digitalRead(SchalterKanal.in) == LOW) {
			}ESP_LOGI(TAG,
					"Taster losgelassen GPIO IN: %d, GPIO OUT: %d, Schalter: %s",
					SchalterKanal.in, SchalterKanal.out, bezeichnung.c_str());
		}
	}
}

void LichtSchalter::Switch() {
	digitalWrite(SchalterKanal.out, SchalterKanal.state);
}

void LichtSchalter::Init() {
	SchalterKanal.Init();
	objStatus = Status::AUS;
}

void LichtSchalter::AddVerbunden(LichtSchalter* lichtSchalter) {
	verbunden.push_back(lichtSchalter);
}

void LichtSchalter::an() {
	SchalterKanal.state = LOW;
	SchalterKanal.autoOff = LONG_LONG_MAX;
	SchalterKanal.lastChangeTime = hmMillis();
	Serial.printf("Schalter an GPIO IN: %d, GPIO OUT: %d, Schalter: %s\n",
			SchalterKanal.in, SchalterKanal.out, bezeichnung.c_str());
	objStatus = Status::AN;
}
void LichtSchalter::aus() {
	SchalterKanal.state = HIGH;
	SchalterKanal.autoOff = LONG_LONG_MAX;
	SchalterKanal.lastChangeTime = hmMillis();
	Serial.printf("Schalter aus GPIO IN: %d, GPIO OUT: %d, Schalter: %s\n",
			SchalterKanal.in, SchalterKanal.out, bezeichnung.c_str());
	objStatus = Status::AUS;
}

void LichtSchalter::PutOperations(AsyncResponseStream& client,
		const String& requestHost) {
	if (SchalterKanal.state == HIGH) {
		// the content of the HTTP response follows the header:
		client.printf(
				"<button onclick=\"execOperation('http://%s/%d/an/', this)\">%s</button>",
				requestHost.c_str(), (int) &(bezeichnung), TOSTRING(LIGHT_OFF));
	} else {
		client.printf(
				"<button onclick=\"execOperation('http://%s/%d/aus/', this)\">%s</button>",
				requestHost.c_str(), (int) &(bezeichnung), TOSTRING(LIGHT_ON));
	}
}

void LichtSchalter::ProcessCommand(std::string cmd, AsyncResponseStream& client) {
	if (cmd == "an") {
		an();
	}
	if (cmd == "aus") {
		aus();
	}
	PutOperations(client, requestHost);
}

//autolichtschalter begin

AutoLichtSchalter::AutoLichtSchalter(uint8_t in, uint8_t out,
		std::string bezeichnung) :
		LichtSchalter(in, out, bezeichnung) {
}
AutoLichtSchalter::~AutoLichtSchalter() {

}
void AutoLichtSchalter::CheckIO() {
	if (digitalRead(SchalterKanal.in) == LOW) {
		Serial.printf("Taster gedrückt GPIO IN: %d, GPIO OUT: %d, Schalter: %s\n",
				SchalterKanal.in, SchalterKanal.out, bezeichnung.c_str());
		if ((hmMillis() - SchalterKanal.lastChangeTime) > 250) {
			if (SchalterKanal.state == HIGH) {
				an();
			} else {
				aus();
			}
			while (digitalRead(SchalterKanal.in) == LOW) {
			}ESP_LOGI(TAG,
					"Taster losgelassen GPIO IN: %d, GPIO OUT: %d, Schalter: %s",
					SchalterKanal.in, SchalterKanal.out, bezeichnung.c_str());
			if (SchalterKanal.state == LOW) //licht Gang
			{
				if (((hmMillis() - SchalterKanal.lastChangeTime)) < 500) {
					SchalterKanal.autoOff = SchalterKanal.lastChangeTime
							+ (10 * 60 * 1000); //10 min
				} else {
					SchalterKanal.autoOff = SchalterKanal.lastChangeTime
							+ (60 * 60 * 1000); //1 Stunde
				}
			}
		}
	}
	if (SchalterKanal.autoOff < hmMillis()) {
		Serial.printf("SchalterKanal autoOff firering %s\n", bezeichnung.c_str());
		aus();
	}
}

void AutoLichtSchalter::ProcessCommand(std::string cmd, AsyncResponseStream& client) {
	if (cmd == "an") {
		an();
		SchalterKanal.autoOff = hmMillis() + (10 * 60 * 1000);
	}
	if (cmd == "aus") {
		aus();
	}
	PutOperations(client, requestHost);
}

//autolichtschalter end
//LichtSchalter end
RolloSchalter::RolloSchalter(uint8_t inAuf, uint8_t outAuf, uint8_t inAb,
		uint8_t outAb, int auto_rauffahren, int auto_runterfahren,
		std::string bezeichnung) :
		SchalterAuf(inAuf, outAuf), SchalterAb(inAb, outAb), autoCheck(0) {
	this->bezeichnung = bezeichnung;
	char buffer[512];
	char anfangBuchstabe = bezeichnung[0];
	snprintf(buffer, 512, TOSTRING(ARROW_FULL_UP), anfangBuchstabe);
	operationen.push_back(std::make_pair("ganz_auf", new std::string(buffer)));
	snprintf(buffer, 512, TOSTRING(ARROW_FULL_DOWN), anfangBuchstabe);
	operationen.push_back(std::make_pair("ganz_zu", new std::string(buffer)));
	snprintf(buffer, 512, TOSTRING(ARROW_UP), anfangBuchstabe);
	operationen.push_back(std::make_pair("auf", new std::string(buffer)));
	snprintf(buffer, 512, TOSTRING(ARROW_DOWN), anfangBuchstabe);
	operationen.push_back(std::make_pair("zu", new std::string(buffer)));
	//operationen.push_back(std::make_pair("SetConfigValue", new std::string("S")));
	for (int i = 0; i < 7; i++) {
		this->auto_rauffahren[i] = (auto_rauffahren / 100) * 3600
				+ (auto_rauffahren % 100) * 60;
		this->auto_runterfahren[i] = (auto_runterfahren / 100) * 3600
				+ (auto_runterfahren % 100) * 60;
	}
}

RolloSchalter::~RolloSchalter() {
}

void RolloSchalter::CheckIO() {
	if (digitalRead(SchalterAuf.in) == LOW) {
		if (SchalterAuf.state != LOW) {
			auf();
		} else {
			if ((hmMillis() - SchalterAuf.lastChangeTime) > (1500)) {
				ganz_auf();
			}
		}
	} else {
		if ((SchalterAuf.autoOff == LONG_LONG_MAX)
				&& (SchalterAuf.state == LOW)) {
			SchalterAuf.state = HIGH;  //AUS
			SchalterAuf.lastChangeTime = hmMillis();
			ESP_LOGI(TAG,
					"Taster losgelassen GPIO IN: %d, GPIO OUT: %d, Schalter: %s auf",
					SchalterAuf.in, SchalterAuf.out, bezeichnung.c_str());
		}
		if (digitalRead(SchalterAb.in) == LOW) {
			if (SchalterAb.state != LOW) {
				zu();
			} else {
				if ((hmMillis() - SchalterAb.lastChangeTime) > (1500)) {
					ganz_zu();
				}
			}
		} else if ((SchalterAb.autoOff == LONG_LONG_MAX)
				&& (SchalterAb.state == LOW)) {
			SchalterAb.state = HIGH;  //AUS
			SchalterAb.lastChangeTime = hmMillis();
			ESP_LOGI(TAG,
					"Taster losgelassen GPIO IN: %d, GPIO OUT: %d, Schalter: %s ab",
					SchalterAb.in, SchalterAb.out, bezeichnung.c_str());
		}
	}

	if (SchalterAuf.autoOn < hmMillis()) {
		ESP_LOGI(TAG,
				"SchalterAuf autoOn firering %s, autoon: %lld, hmMillis: %lld",
				bezeichnung.c_str(), SchalterAuf.autoOn, hmMillis());
		ganz_auf();
		SchalterAuf.autoOn = this->GetNextAutoTimeAuf();
	}
	if (SchalterAb.autoOn < hmMillis()) {
		if (SomeOneAtHome()) {
			Serial.printf("SchalterAb autoOn firering %s\n", bezeichnung.c_str());
			ganz_zu();
			SchalterAb.autoOn = this->GetNextAutoTimeAb();
			SchalterAuf.retry = 8;
		} else {
			SchalterAuf.retry--;
			if (SchalterAuf.retry < 0) {
				SchalterAb.autoOn = this->GetNextAutoTimeAb();
				SchalterAuf.retry = 8;
				ESP_LOGI(TAG,
						"SchalterAb autoOn niemand zuhause auf nüchsten tag verschoben %s, SchalterAuf.autoOn: %lld, SchalterAb.autoOn: %lld",
						bezeichnung.c_str(), SchalterAuf.autoOn,
						SchalterAb.autoOn);
			} else {
				SchalterAb.autoOn = hmMillis() + 60 * 1000;
				ESP_LOGI(TAG,
						"SchalterAb autoOn niemand zuhause auf 5 min spüter verschoben %s, SchalterAuf.autoOn: %lld, SchalterAb.autoOn: %lld",
						bezeichnung.c_str(), SchalterAuf.autoOn,
						SchalterAb.autoOn);
			}
		}
	}

	if (SchalterAuf.autoOff < hmMillis()) {
		Serial.printf("SchalterAuf autoOff firering %s\n", bezeichnung.c_str());
		SchalterAuf.state = HIGH;
		SchalterAuf.autoOff = LONG_LONG_MAX;
		SchalterAuf.lastChangeTime = hmMillis();
	}

	if (SchalterAb.autoOff < hmMillis()) {
		Serial.printf("SchalterAb autoOff firering %s\n", bezeichnung.c_str());
		SchalterAb.state = HIGH;
		SchalterAb.autoOff = LONG_LONG_MAX;
		SchalterAb.lastChangeTime = hmMillis();
	}
	if (autoCheck < hmMillis()) {
		Serial.printf("Windgeschwindigkeitcheck: %d, Max:%d, Zeit: %lld\n",
				daten.WindGeschwindigkeit, daten.WindGeschwindigkeitMax,
				hmMillis());
		if ((daten.WindGeschwindigkeit + daten.WindGeschwindigkeitMax) / 2
				> 60) {
			Serial.printf("Windgeschwindigkeitcheck: %d, Max:%d, ganz auf\n",
					daten.WindGeschwindigkeit, daten.WindGeschwindigkeitMax);
			ganz_auf();
		}
		autoCheck = hmMillis() + 15 * 60 * 1000;
	}
}
void RolloSchalter::Switch() {
	digitalWrite(SchalterAuf.out, SchalterAuf.state);
	digitalWrite(SchalterAb.out, SchalterAb.state);
}

void RolloSchalter::Init() {
	SchalterAuf.Init();
	SchalterAb.Init();
	SchalterAuf.autoOn = this->GetNextAutoTimeAuf();
	SchalterAb.autoOn = this->GetNextAutoTimeAb();
	SchalterAuf.retry = 8;
	Serial.printf("Schalter Init: %s UP: %lld down: %lld\n", bezeichnung.c_str(), SchalterAuf.autoOn/1000, SchalterAb.autoOn/1000);
}

int64_t RolloSchalter::GetNextAutoTimeAuf() {
	struct tm ti = GetTime();
	return GetNextAutoTime(auto_rauffahren[ti.tm_wday] / 3600,
			(auto_rauffahren[ti.tm_wday] % 3600) / 60, 0);
}
int64_t RolloSchalter::GetNextAutoTimeAb() {
	struct tm ti = GetTime();
	return GetNextAutoTime(auto_runterfahren[ti.tm_wday] / 3600,
			(auto_runterfahren[ti.tm_wday] % 3600) / 60, 0);
}

void RolloSchalter::ganz_auf() {
	digitalWrite(SchalterAb.out, HIGH);
	SchalterAb.state = HIGH;
	SchalterAb.lastChangeTime = hmMillis();
	SchalterAb.autoOff = LONG_LONG_MAX;
	ESP_LOGI(TAG,
			"ganz_auf gedrückt GPIO IN: %d, GPIO OUT: %d, Schalter: %s auf",
			SchalterAuf.in, SchalterAuf.out, bezeichnung.c_str());
	SchalterAuf.state = LOW;  //ein
	SchalterAuf.lastChangeTime = hmMillis();
	SchalterAuf.autoOff = SchalterAuf.lastChangeTime + (60 * 1000); //1min
	objStatus = Status::OFFEN;
}
void RolloSchalter::ganz_zu() {
	digitalWrite(SchalterAuf.out, HIGH);
	SchalterAuf.state = HIGH;
	SchalterAuf.lastChangeTime = hmMillis();
	SchalterAuf.autoOff = LONG_LONG_MAX;
	Serial.printf("ganz_zu gedrückt GPIO IN: %d, GPIO OUT: %d, Schalter: %s ab\n",
			SchalterAb.in, SchalterAb.out, bezeichnung.c_str());
	SchalterAb.state = LOW;  //ein
	SchalterAb.lastChangeTime = hmMillis();
	SchalterAb.autoOff = SchalterAb.lastChangeTime + (60 * 1000); //1min
	objStatus = Status::ZU;
}
void RolloSchalter::auf(bool autoOff) {
	digitalWrite(SchalterAb.out, HIGH);
	SchalterAb.state = HIGH;
	SchalterAb.lastChangeTime = hmMillis();
	SchalterAb.autoOff = LONG_LONG_MAX;
	SchalterAuf.state = LOW;  //ein
	SchalterAuf.lastChangeTime = hmMillis();
	if (autoOff) {
		SchalterAuf.autoOff = SchalterAuf.lastChangeTime + (750); //1min
	}Serial.printf("auf gedrückt GPIO IN: %d, GPIO OUT: %d, Schalter: %s auf\n",
			SchalterAuf.in, SchalterAuf.out, bezeichnung.c_str());
}

void RolloSchalter::zu(bool autoOff) {
	digitalWrite(SchalterAuf.out, HIGH);
	SchalterAuf.state = HIGH;
	SchalterAuf.lastChangeTime = hmMillis();
	SchalterAuf.autoOff = LONG_LONG_MAX;
	SchalterAb.state = LOW;  //ein
	SchalterAb.lastChangeTime = hmMillis();
	if (autoOff) {
		SchalterAb.autoOff = SchalterAb.lastChangeTime + (750); //1min
	}Serial.printf("zu gedrückt GPIO IN: %d, GPIO OUT: %d, Schalter: %s ab\n",
			SchalterAb.in, SchalterAb.out, bezeichnung.c_str());
}

void RolloSchalter::ProcessCommand(std::string cmd, AsyncResponseStream& client) {
	if (cmd == "ganz_auf") {
		ganz_auf();
	}
	if (cmd == "ganz_zu") {
		ganz_zu();
	}

	if (cmd == "auf") {
		auf(true);
	}

	if (cmd == "zu") {
		zu(true);
	}
	String currenNr = "";
	if (cmd == "SETCONFIG_AUF") {
		int i = 0;
		int stunde = 0;
		int minute = 0;
		/*while (client.connected() && client.available()) { // loop while the client's connected
			char c = client.read();
			if (c == '?') {
				i = client.read() - '0';
			} else if (c == '&') {
				minute = atoi(currenNr.c_str());
				currenNr = "";
				if (i >= 0 && i < 7) {
					auto_rauffahren[i] = stunde * 3600 + minute * 60;
				}
				i = client.read() - '0';
			} else if (c == '=') {
				currenNr = "";
			} else if (c == '%') {
				c = client.read();
				c = client.read();
				stunde = atoi(currenNr.c_str());
				currenNr = "";
			} else {
				currenNr += c;
			}
		}
		if (i >= 0 && i < 7) {
			auto_rauffahren[i] = stunde * 3600 + minute * 60;
		}*/
	}
	if (cmd == "SETCONFIG_ZU") {
		int i = 0;
		int stunde = 0;
		int minute = 0;
		/*while (client.connected() && client.available()) { // loop while the client's connected
			char c = client.read() - '0';
			if (c == '?') {
				i = client.read();
			} else if (c == '&') {
				minute = atoi(currenNr.c_str());
				currenNr = "";
				if (i >= 0 && i < 7) {
					auto_runterfahren[i] = stunde * 3600 + minute * 60;
				}
				i = client.read() - '0';
			} else if (c == '=') {
				currenNr = "";
			} else if (c == '%') {
				c = client.read();
				stunde = atoi(currenNr.c_str());
				currenNr = "";
			} else {
				currenNr += c;
			}
		}
		if (i >= 0 && i < 7) {
			auto_runterfahren[i] = stunde * 3600 + minute * 60;
		}*/
	}
	client.print(TOSTRING(RESPONSE_REDIRECT));
	client.flush();
}

void RolloSchalter::PutSettings(AsyncResponseStream& client, const String& requestHost) {
	client.printf(TOSTRING(HTML_ROLLO_SETTING_FORM_BEGIN), "Auf",
			requestHost.c_str(), (int) &(bezeichnung), "SETCONFIG_AUF");
	for (int i = 0; i < 7; i++) {
		client.printf(TOSTRING(SETTING_INPUT_FIELD), i, i,
				auto_rauffahren[i] / 3600, (auto_rauffahren[i] / 60) % 60);
	}
	client.printf(TOSTRING(HTML_ROLLO_SETTING_FORM_END));

	client.printf(TOSTRING(HTML_ROLLO_SETTING_FORM_BEGIN), "Zu",
			requestHost.c_str(), (int) &(bezeichnung), "SETCONFIG_ZU");
	for (int i = 0; i < 7; i++) {
		client.printf(TOSTRING(SETTING_INPUT_FIELD), i, i,
				auto_runterfahren[i] / 3600, (auto_runterfahren[i] / 60) % 60);
	}
	client.printf(TOSTRING(HTML_ROLLO_SETTING_FORM_END));
}

bool RolloSchalter::Putinfo(char* buffer, int Size) {
	int64_t auf = (SchalterAuf.autoOn - hmMillis()) / 1000;
	int64_t zu = (SchalterAb.autoOn - hmMillis()) / 1000;
	struct tm ti = GetTime();
	snprintf(buffer, Size,
			"<tr><th>%s</th><td>auto up in %02lld:%02lld:%02lld sek </td><td>auto down in %02lld:%02lld:%02lld sek </td></tr><tr><th>%s</th><td>Auto auf Zeit: %02d:%02d </td><td>Auto down Zeit: %02d:%02d </td></tr>",
			bezeichnung.c_str(), auf / 3600, (auf / 60) % 60, auf % 60,
			zu / 3600, (zu / 60) % 60, zu % 60, bezeichnung.c_str(),
			auto_rauffahren[ti.tm_wday] / 3600, (auto_rauffahren[ti.tm_wday] / 60) % 60,
			auto_runterfahren[ti.tm_wday] / 3600, (auto_runterfahren[ti.tm_wday] / 60) % 60);
	return true;
}

GlobalSchalter::GlobalSchalter() {
	this->bezeichnung = "Alle";
}
void GlobalSchalter::CheckIO() {
}
void GlobalSchalter::Switch() {
}
void GlobalSchalter::Init() {
}
void GlobalSchalter::ProcessCommand(std::string cmd, AsyncResponseStream& client) {
	Serial.printf("Schalter: %s cmd:%s\n", bezeichnung.c_str(), cmd.c_str());
	if (cmd == "ganz_auf") {
		ganz_auf();
	}
	if (cmd == "ganz_zu") {
		ganz_zu();
	}

	if (cmd == "an") {
		an();
	}

	if (cmd == "aus") {
		aus();
	}
}
void GlobalSchalter::ganz_auf() {
	for (auto itrRaum : vecRaum) {
		for (auto itr : itrRaum->vecSchaltern) {
			if (this != itr)
				itr->ganz_auf();
		}
	}
}

void GlobalSchalter::ganz_zu() {
	for (auto itrRaum : vecRaum) {
		for (auto itr : itrRaum->vecSchaltern) {
			if (this != itr)
				itr->ganz_zu();
		}
	}
}
void GlobalSchalter::an() {
	for (auto itrRaum : vecRaum) {
		for (auto itr : itrRaum->vecSchaltern) {
			if (this != itr)
				itr->an();
		}
	}
}
void GlobalSchalter::aus() {
	for (auto itrRaum : vecRaum) {
		for (auto itr : itrRaum->vecSchaltern) {
			if (this != itr)
				itr->aus();
		}
	}
}

//Taster begin

TasterSchalter::TasterSchalter(uint8_t in, uint8_t out, std::string bezeichnung) :
		LichtSchalter(in, out, bezeichnung) {
}
TasterSchalter::~TasterSchalter() {

}
void TasterSchalter::CheckIO() {
	if (SchalterKanal.autoOff < hmMillis()) {
		Serial.printf("SchalterKanal autoOff firering %s\n", bezeichnung.c_str());
		aus();
	}
}

void TasterSchalter::ProcessCommand(std::string cmd, AsyncResponseStream& client) {
	if (cmd == "an") {
		an();
		SchalterKanal.autoOff = hmMillis() + (500);
	}
	if (cmd == "aus") {
		aus();
	}
	PutOperations(client, requestHost);
}

void TasterSchalter::an() {
	SchalterKanal.state = LOW;
	SchalterKanal.autoOff = hmMillis() + (500);
	SchalterKanal.lastChangeTime = hmMillis();
	Serial.printf("Schalter an GPIO IN: %d, GPIO OUT: %d, Schalter: %s\n",
			SchalterKanal.in, SchalterKanal.out, bezeichnung.c_str());
	objStatus = Status::AN;
}
//Taster end
