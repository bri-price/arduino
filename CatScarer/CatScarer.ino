#include <WiFi.h>
#include <PubSubClient.h>
#include "SR04.h"

#define SOLENOID_PIN			13
#define PIR_PIN					12


#define FAKE_VCC_PIN			14
#define TRIG_PIN				27
#define ECHO_PIN				26
#define FAKE_GND_PIN			25

#define IOT_NAME				"CAT_SCARER"
#define PUBTOPIC				"esp/cat-scarer/set"
#define OUTTOPIC				"esp/cat-scarer"
#define mqtt_server				"192.168.1.247"

int delayTime = 50;

int squirtTime = 3000;
int loopsBetweenSquirts = 200;
int loopsRemaining = 30;

bool initialising = true;
bool firing = false;
bool armed = false;

long minimumRange = 10;
long maximumRange = 210;
long maximumRangeStart = 210;	// never go more than 2m

SR04 sr04 = SR04(ECHO_PIN, TRIG_PIN);
long a;


WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastmsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

#include "Secrets.h"

char ssid1[] = SECRET_SSID1;
char pass1[] = SECRET_PASS1;
char ssid2[] = SECRET_SSID2;
char pass2[] = SECRET_PASS2;

void publishStatus() {
	if (firing) {
		client.publish(OUTTOPIC, "FIRING");
	} else if (initialising) {
		client.publish(OUTTOPIC, "INITIALISING");
	} else if (armed) {
		client.publish(OUTTOPIC, "ARMED");
	} else {
		client.publish(OUTTOPIC, "DISARMED");
	}
}


void handleFire() {
	squirtThatSucka();
}

void handleArm() {
	armed = true;
	publishStatus();
}

void handleDisarm() {
	armed = false;
	publishStatus();
}

void handleInit() {
	initialising = true;
	loopsRemaining = 100;
	maximumRange = maximumRangeStart;
}

void squirtThatSucka() {
	firing = true;
	publishStatus();
	Serial.println("Squirting!!");
	digitalWrite(SOLENOID_PIN, HIGH);
	delay(squirtTime);
	digitalWrite(SOLENOID_PIN, LOW);
	firing = false;
	publishStatus();
}

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

	char payloadString[length + 1];
	for (int i = 0; i < length; i++) {
		payloadString[i] = (char)payload[i];
	}
	payloadString[length] = '\0';

	Serial.print("Message arrived [");
	Serial.print(topic);
	Serial.print("] ");
	Serial.println(payloadString);

	if (strcmp((char *)topic, PUBTOPIC) == 0) {

		if (strcmp((char *)payloadString, "ARM") == 0) {
			Serial.println("ReceivedARM request");
			handleArm();
		} else if (strcmp((char *)payloadString, "DISARM") == 0) {
			Serial.println("Received DISARM request");
			handleDisarm();
		} else if (strcmp((char *)payloadString, "FIRE") == 0) {
			Serial.println("Received FIRE request");
			handleFire();
		} else if (strcmp((char *)payloadString, "INIT") == 0) {
			Serial.println("Received INIT request");
			handleInit();
		}
	}
}

void reconnect() {

	// Loop until we're reconnected
	while (!client.connected()) {
		Serial.print("Attempting MQTT connection...");

		// Create a random client ID
		String clientId = IOT_NAME;

		// Attempt to connect
		if (client.connect(clientId.c_str(), "Brian", "Qu3rcu54!HASS", OUTTOPIC, 0, 0, "OFFLINE" )) {
			Serial.println("connected");
			// ... and resubscribe
			client.subscribe(PUBTOPIC);
			Serial.print("subscribed to topic ");
			Serial.println(PUBTOPIC);

			publishStatus();
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
	pinMode(SOLENOID_PIN, OUTPUT);
	pinMode(PIR_PIN, INPUT);

	pinMode(FAKE_GND_PIN, OUTPUT);
	pinMode(FAKE_VCC_PIN, OUTPUT);
	digitalWrite(FAKE_VCC_PIN,HIGH);
	digitalWrite(FAKE_GND_PIN,LOW);

	int wifiNum = 1;
	bool wifiConnected = false;

	while (wifiConnected == false) {
		wifiNum = 3 - wifiNum;
		wifiConnected = initWifi(wifiNum);
	}

	client.setServer(mqtt_server, 1883);
	client.setCallback(callback);
}

void loop() {

	if (!client.connected()) {
		reconnect();
	}
	client.loop();

	// actual delay = delayDelays x delayTime = 10 x 200 = 2s
	delay(delayTime);

	long a = sr04.Distance();
	Serial.print(a);
	Serial.println("cm");

	if (initialising) {

		Serial.println("initialising...");

		// update the max range if there is something in the way
		if (a < maximumRange && a > minimumRange) {
			maximumRange = a;
		}

		// only check if we're not waiting for the next squirt
		if (loopsRemaining < 1) {

			maximumRange = maximumRange - 10;
			Serial.print("initialising complete... Maxmimum range set to ");
			Serial.print(maximumRange);
			Serial.println("cm");
			initialising = false;
			publishStatus();
		} else {
			loopsRemaining = loopsRemaining - 1;
		}
	} else if (armed) {

		Serial.println("armed...");

		int movement = digitalRead(PIR_PIN);
		Serial.print("Movement = ");
		Serial.println(movement);

		// only check if we're not waiting for the next squirt
		if (loopsRemaining < 1) {
			if (a >= minimumRange && a <= maximumRange && movement == 1) {
				squirtThatSucka();
				loopsRemaining = loopsBetweenSquirts;
			}
		} else {
			loopsRemaining = loopsRemaining - 1;
		}
	}

	// publish status to HASS every minute
	unsigned long now = millis();
	if ((now - lastmsg) >= 60000) {
		lastmsg = now;
		publishStatus();
	}
}
