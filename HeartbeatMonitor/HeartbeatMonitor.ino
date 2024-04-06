#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include "Secrets.h"
#include "Config.h"
#include "Variables.h"
#include "Defines.h"
#include "PinDefinitions.h"

static const int MAX_PAYLOAD_SIZE = 30000;

int dataSize = 0;
static uint16_t binaryData[MAX_PAYLOAD_SIZE];

int sensorValue = 0;  // value read from the pot
int outputValue = 0;  // value output to the PWM (analog out)

unsigned long lastTime = 0;
unsigned long timerDelay = 30000;
unsigned long warmupDelay = 10000;
//unsigned long timerDelay = 5000;

unsigned long lastPulseTime = 0;
bool warmup = true;

boolean InitWifiSSID(char *ssid, char *pass) {

	int attempts = 0;

	WiFi.mode(WIFI_OFF);
	delay(100);
	WiFi.mode(WIFI_STA);

	// We start by connecting to a WiFi network
	debug("Connecting to ");
	debugln(ssid);
	
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, pass);

	// wait for up to 10 seconds to get connected
	while (WiFi.status() != WL_CONNECTED && attempts < 15) {
		delay(500);
		debug(".");
		attempts = attempts + 1;
	}

	if (WiFi.status() != WL_CONNECTED) {
		debugln("");
		debugln("WiFi failed");
		return false;
	}

	randomSeed(micros());

	debugln("");
	debug("WiFi connected to ");
	debug(ssid);
	debug(" on IP address: ");
	debugln(WiFi.localIP());
	delay(100);
	connectedToWifi = true;
	return true;
}

void InitWifi() {
	
	int wifiNum = 0;
	bool wifiConnected = false;
	int attempts = 0;

	while (!wifiConnected) {
		wifiConnected = InitWifiSSID(ssid[wifiNum],password[wifiNum]);

		if (wifiConnected == false) {
			attempts++;
			wifiNum = wifiNum + 1;
			if (wifiNum == 3) {
				wifiNum = 0;
			}
		}
		if (attempts > 5) {
			debugln("WiFi failed after 6 attempts");
			break;
		}
	}
}
void InitGPIO() {
  pinMode(digitalLoMinusPin, INPUT);
  pinMode(digitalLoPlusPin, INPUT);
	debugln("GPIO pins initialised");
}

void InitSerial() {
	initserial
	debugln("");
	debugln("Initialised serial");
}

// Number of milliseconds to wait without receiving any data before we give up
const int kNetworkTimeout = 30*1000;
// Number of milliseconds to wait if no data is available before trying again
const int kNetworkDelay = 1000;

void postBinaryDataToAPI(const uint16_t* data, size_t length) {

  HTTPClient http;

  http.begin(serverUrl);
	http.setTimeout(15000);

  http.addHeader("Content-Type", "application/octet-stream");
	http.addHeader("User-Agent", "ESP32");

  int httpResponseCode = http.sendRequest("POST", (uint8_t *)data, length * sizeof(uint16_t));

  if (httpResponseCode > 0) {
    debug("HTTP Response code: ");
    debugln(httpResponseCode);
    String response = http.getString();
    debugln(response);
  } else {
    debug("Error code: ");
    debugln(httpResponseCode);
    String response = http.getString();
    debugln(response);
  }

  // End HTTP connection
  http.end();
}

void setup() {

	InitSerial();

	debugln("Initialising GPIO");
	InitGPIO();

	debugln("Initialising Wifi");
	InitWifi();
}

void loop() {

  if (warmup == true) {

    if ((millis() - lastTime) > warmupDelay) {
      dataSize = 0;
      lastTime = millis();
      warmup = false;
    }

  } else {

    if ((digitalRead(digitalLoMinusPin) == 1) || (digitalRead(digitalLoPlusPin) == 1)) {
      // alert for pad(s) off
      binaryData[dataSize] = 0;
    } else {
      sensorValue = analogRead(analogInPin);
      binaryData[dataSize] = sensorValue;
    }

    // post after a certain time, or if buffer is full
    if ((millis() - lastTime) > timerDelay || dataSize == (MAX_PAYLOAD_SIZE - 1)) {
      postBinaryDataToAPI(binaryData, dataSize);
      dataSize = 0;
      lastTime = millis();
    } else {
      dataSize = dataSize + 1;
    }
  }
  delay(10);
}


