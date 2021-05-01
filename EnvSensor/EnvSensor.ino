 #include <ESP8266WiFi.h>        // Include the Wi-Fi library
#include <PubSubClient.h>

#include <Wire.h>
#include <SPI.h>
#include "Adafruit_SGP30.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#include <ArduinoJson.h>



#define DHTTYPE    DHT11     // DHT 11
#define IOT_NAME				"ENV-SENSOR"
#define PUBTOPIC				"esp/env-sensor/set"
#define OUTTOPIC				"esp/env-sensor"
#define mqtt_server			"192.168.1.247"
#define DHTPIN		D4
Adafruit_SGP30 sgp;
WiFiClient espClient;
PubSubClient client(espClient);
DHT_Unified dht(DHTPIN, DHTTYPE);


#define	PUBLISH_MILLIS	60000
#define	BASELINE_MILLIS	30000
unsigned long lastMsg = 0;
unsigned long lastBaseline = 0;
char msg[50];
int value = 0;

#include "Secrets.h"

char ssid1[] = SECRET_SSID1;
char pass1[] = SECRET_PASS1;
char ssid2[] = SECRET_SSID2;
char pass2[] = SECRET_PASS2;

uint32_t getAbsoluteHumidity(float temperature, float humidity) {
    // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
    const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
    const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
    return absoluteHumidityScaled;
}

double lastTemp = 0;
double lastHum = 0;
double lastECO2 = 0;
double lastTVOC = 0;


boolean initWifi(int wifiNum) {

	int attempts = 0;

	WiFi.mode(WIFI_OFF);
	delay(100);
	WiFi.mode(WIFI_STA);
	// We start by connecting to a WiFi network
	Serial.println();
	Serial.print("Connecting to ");

	if (wifiNum == 1) {
		Serial.println(ssid1);
	} else {
		Serial.println(ssid2);
	}
	
	WiFi.mode(WIFI_STA);
	if (wifiNum == 1) {
		WiFi.begin(ssid1, pass1);
	} else {
		WiFi.begin(ssid2, pass2);
	}

	while (WiFi.status() != WL_CONNECTED && attempts < 20) {
		delay(500);
		Serial.print(".");
		attempts = attempts + 1;
	}

	if (WiFi.status() != WL_CONNECTED) {
		Serial.println("");
		Serial.println("WiFi failed");
		return false;
	}

	randomSeed(micros());

	Serial.println("");
	Serial.print("WiFi connected to ");
	if (wifiNum == 1) {
		Serial.println(ssid1);
	} else {
		Serial.println(ssid2);
	}
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());
	delay(100);
	return true;
}


void callback(char* topic, byte* payload, unsigned int length) {

	char payloadString[length+1];
  for (int i = 0; i < length; i++) {
    payloadString[i] = (char)payload[i];
  }
  payloadString[length] = '\0';

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(payloadString);

  if (strcmp((char *)topic, PUBTOPIC) == 0) {

  }
  
}
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    // Create a random client ID
    String clientId = IOT_NAME;
    
    // Attempt to connect
    if (client.connect(clientId.c_str(),"Brian","Qu3rcu54!HASS")) {
      Serial.println("connected");
      // ... and resubscribe
      client.subscribe(PUBTOPIC);
      Serial.print("subscribed to topic ");
      Serial.println(PUBTOPIC);

      client.publish(OUTTOPIC, "DISARMED");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void setup() {
 	// put your setup code here, to run once:
	Serial.begin(115200);


	Serial.println("Environment Monitor v0.2");
	Serial.println('\n');
	Serial.println('\n');
	Serial.println('\n');

	Serial.println (F("Initialising SGP30 Gas Sensor..."));
	if (! sgp.begin()){
		Serial.println("Sensor not found :(");
		while (1);
	}

	Serial.print("      Found SGP30 serial #");
	Serial.print(sgp.serialnumber[0], HEX);
	Serial.print(sgp.serialnumber[1], HEX);
	Serial.println(sgp.serialnumber[2], HEX);

	delay(500);
	Serial.println(F("Initialising DHT11 Unified Sensor..."));
	dht.begin();
	
	// Print temperature sensor details.
	sensor_t sensor;
	dht.temperature().getSensor(&sensor);
	Serial.println(F("------------------------------------"));
	Serial.println(F("Temperature Sensor"));
	Serial.print  (F("      Sensor Type: ")); Serial.println(sensor.name);
	Serial.print  (F("      Driver Ver:  ")); Serial.println(sensor.version);
	Serial.print  (F("      Unique ID:   ")); Serial.println(sensor.sensor_id);
	Serial.print  (F("      Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("°C"));
	Serial.print  (F("      Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("°C"));
	Serial.print  (F("      Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("°C"));
	Serial.println(F("------------------------------------"));
	
	// Print humidity sensor details.
	dht.humidity().getSensor(&sensor);
	Serial.println(F("Humidity Sensor"));
	Serial.print  (F("      Sensor Type: ")); Serial.println(sensor.name);
	Serial.print  (F("      Driver Ver:  ")); Serial.println(sensor.version);
	Serial.print  (F("      Unique ID:   ")); Serial.println(sensor.sensor_id);
	Serial.print  (F("      Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("%"));
	Serial.print  (F("      Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("%"));
	Serial.print  (F("      Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("%"));
	Serial.println(F("------------------------------------"));
	
	
	int wifiNum = 2;
	bool wifiConnected = false;

	while (wifiConnected == false) {
		wifiNum = 3 - wifiNum;
		wifiConnected = initWifi(wifiNum);
	}

	client.setServer(mqtt_server, 1883);
	client.setCallback(callback);
}

int counter = 0;
unsigned long lastMillis = 0;
unsigned long lastLongMillis = 0;
long interval = 30000;
long longinterval = 60000;
boolean shuttingDown = false;

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

	if (!sgp.IAQmeasure()) {
		Serial.println("Measurement failed");
		return;
	}

	if (!sgp.IAQmeasureRaw()) {
		Serial.println("Raw Measurement failed");
		return;
	}
 
	unsigned long now = millis();
	if (now - lastBaseline > BASELINE_MILLIS) {
		lastBaseline = now;
		// we're only going to do this every 30 seconds
		counter = 0;

		uint16_t TVOC_base, eCO2_base;
		if (! sgp.getIAQBaseline(&eCO2_base, &TVOC_base)) {
			Serial.println("Failed to get baseline readings");
			return;
		}
	}

	sensors_event_t t_event;
	dht.temperature().getEvent(&t_event);
	if (isnan(t_event.temperature)) {
		Serial.println(F("Error reading temperature!"));
 	}

	sensors_event_t h_event;
	dht.humidity().getEvent(&h_event);
	if (isnan(h_event.relative_humidity)) {
		Serial.println(F("Error reading humidity!"));
	}

	lastTemp = t_event.temperature;
	lastHum = h_event.relative_humidity;
	lastECO2 = sgp.eCO2;
	lastTVOC = sgp.TVOC;

	Serial.print(lastTemp);
	Serial.print(",");
	Serial.print(lastHum);
	Serial.print(",");
	Serial.print(lastTVOC);
	Serial.print(",");
	Serial.println(lastECO2);

	if (now - lastMsg > PUBLISH_MILLIS) {

		Serial.println("Publishing");
		lastMsg = now;
	
		char tempString[8];
		dtostrf(lastTemp, 1, 2, tempString);
		Serial.print("Temperature: ");
		Serial.println(tempString);
		client.publish("esp32/env-sensor/temperature", tempString, true);

		char humString[8];
		dtostrf(lastHum, 1, 2, humString);
		Serial.print("Humidity: ");
		Serial.println(humString);
		client.publish("esp32/env-sensor/humidity", humString, true);

		char eco2String[8];
		dtostrf(lastECO2, 1, 2, eco2String);
		Serial.print("eCO2: ");
		Serial.println(eco2String);
		client.publish("esp32/env-sensor/eco2", eco2String, true);

		char tvocString[8];
		dtostrf(lastTVOC, 1, 2, tvocString);
		Serial.print("TVOC: ");
		Serial.println(tvocString);
		client.publish("esp32/env-sensor/tvoc", tvocString, true);

	}

	delay(1000);
}
