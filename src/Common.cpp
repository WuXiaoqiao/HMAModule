/*
 * Common.cpp
 *
 *  Created on: 25.10.2017
 *      Author: Xiaoqiao
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <WiFi.h>
#include "Esp.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "WiFiUdp.h"
#include "Common.h"
#include "apps/sntp/sntp.h"
#include "esp_timer.h"
#include "esp_spi_flash.h"

#include <string.h>
#include <time.h>


#ifndef TAG
#define TAG "COMMON"
#endif

void initialize_sntp(void)
{
	Serial.printf("Initializing SNTP\n");
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, (char *)"at.pool.ntp.org");
	sntp_init();
}

void obtain_time()
{
	initialize_sntp();

	// wait for time to be set
	time_t now = 0;
	struct tm timeinfo;
	memset(&timeinfo, 0, sizeof(timeinfo));
	int retry = 0;
	const int retry_count = 10;
	while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count)
	{
		Serial.printf("Waiting for system time to be set... (%d/%d)\n", retry, retry_count);
		vTaskDelay(2000 / portTICK_PERIOD_MS);
		time(&now);
		localtime_r(&now, &timeinfo);
	}
	char strftime_buf[64];
	// Set timezone to Eastern Standard Time and print local time

	// setenv("TZ", "UTC-1:00", 1);
	setenv("TZ", "UTC-1DST-2,M3.5.0/-2,M10.5.0/-1", 1);
	tzset();
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	Serial.printf("The current date/time in Austria: %s\n", strftime_buf);
}

int64_t hmMillis()
{
	return (esp_timer_get_time() / 1000);
}
//24:0A:C4:0E:14:50
uint8_t macAddressHT[] = {0x78, 0x21, 0x84, 0xA0, 0x15, 0x5C}; // HT
uint8_t macAddressWT[] = {0x24, 0x0A, 0xC4, 0x0E, 0x03, 0x3C}; //Wohnungstür
esp_now_peer_info_t peerInfoWT;
esp_now_peer_info_t peerInfoHT;

void intEspNow()
{
	Serial.printf("intEspNow\n");
	esp_wifi_set_protocol((wifi_interface_t)ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);
	if (esp_now_init() != ESP_OK)
	{
		ESP_LOGE(TAG, "Error initializing ESP-NOW");
		return;
	}
	esp_wifi_set_ps(WIFI_PS_NONE);
	esp_now_register_recv_cb(OnDataRecv);
	memcpy(peerInfoWT.peer_addr, macAddressWT, 6);
	peerInfoWT.channel = 7;
	peerInfoWT.encrypt = false;

	// Add peer
	if (esp_now_add_peer(&peerInfoWT) != ESP_OK)
	{
		ESP_LOGI(TAG,"Failed to add peer");
		return;
	}
    memcpy(peerInfoHT.peer_addr, macAddressHT, 6);
	peerInfoHT.channel = 7;
	peerInfoHT.encrypt = false;

	// Add peer
	if (esp_now_add_peer(&peerInfoHT) != ESP_OK)
	{
		ESP_LOGI(TAG,"Failed to add peer");
		return;
	}
}

