/*

	Use board: "ESP 32 Wrover Module"

	0,2,4,15,32,33 are touch pins
	34,35,36,39 are input only

*/
#include <AsyncTCP.h>
#include <AsyncEventSource.h>
#include <AsyncJson.h>
#include <AsyncWebSocket.h>
#include <AsyncWebSynchronization.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include <StringArray.h>
#include <WebAuthentication.h>
#include <WebHandlerImpl.h>
#include <WebResponseImpl.h>

#include <EEPROM.h>
#include <U8x8lib.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#include "Secrets.h"

char ssid1[] = SECRET_SSID1;
char pass1[] = SECRET_PASS1;
char ssid2[] = SECRET_SSID2;
char pass2[] = SECRET_PASS2;

#define	ON				HIGH
#define OFF				LOW

enum Device {
	camera,
	focus,
	flash,
	laser,
	solenoid,
	ringlight
};

enum LaserMode {
	off,
	fireflash,
	firecamera
};

// optocouplers
#define CAMERA_PIN		18
#define FOCUS_PIN		5
#define FLASH_PIN		19
#define NEODATA_PIN		2

// laser detector
#define LASER_PIN		32
#define LASER_DETECT	35

// touch pad
#define TOUCH_FIRE		33

// solenoid
#define SOLENOID_PIN	13

// joystick
#define JOY_UP_PIN		12
#define JOY_DOWN_PIN	14
#define JOY_LEFT_PIN	27
#define JOY_RIGHT_PIN	26
#define JOY_MID_PIN		25

// menu items
#define MENU_DELAY_BEFORE_SHOOTING		1
#define MENU_LASER_MODE					2
#define MENU_DELAY_BEFORE_DROPS			3
#define MENU_DROP_ONE_SIZE				4
#define MENU_USE_TWO_DROPS				5
#define MENU_DROP_TWO_SIZE				6
#define MENU_DELAY_BETWEEN_DROPS		7
#define MENU_DELAY_BEFORE_FLASH			8
#define MENU_DELAY_AFTER_LASER			9
#define MENU_DELAY_AFTER_SHOOTING		10
#define MENU_SCREEN_TIMEOUT				11
#define MENU_SAVE_SETTINGS				12
#define MENU_TEST_CAMERA				13
#define MENU_TEST_FLASH					14
#define MENU_TEST_SOLENOID				15
#define MENU_ALIGN_LASER				16
#define MENU_RING_MODE					17
#define MENU_RING_SECTIONS				18

// always update this if you add an item to the list above
#define NUM_MENU_ITEMS					18

#define NUMPIXELS						60
// configurable values - saved in eeprom
typedef struct {

	int dropOneSize = 95;
	int dropTwoSize = 95;
	int delayBeforeShooting = 1000;
	int delayBeforeDrops = 100;
	int delayBetweenDrops = 100;
	int delayBeforeFlash = 300;
	int delayAfterShooting = 100;
	int delayAfterLaser = 50;
	int flashDuration = 5;
	int cameraDuration = 350;
	int screenTimeout = 5;
	bool useTwoDrops = false;
	LaserMode laserMode = off;
	int numSections = 5;
	
} CONFIGURATION;

CONFIGURATION cfg;

// UI defines
#define	MAXSIDEWAYSDELAY		500

// run time values
bool joystickRight = true;
int lastJoyLeft = HIGH;
int lastJoyRight = HIGH;
int screenTimer = 0;
int menuItem = 1;

bool updateScreen = true;
int delayVal = 0;
int	downwardsDelay = 500;
int	sidewaysDelay = 500;
double accelerationFactor = 1.3;
bool cameraTestIsRunning = false;
bool solenoidTestIsRunning = false;

int lit = 0;
//int counter = 0;
//int colourValue = 0;
//int section = 1;
//int brightness = 1;

Adafruit_NeoPixel pixels(NUMPIXELS, NEODATA_PIN, NEO_GRB + NEO_KHZ800);
uint32_t pixelColours[] = { 
	pixels.Color(0, 0, 0), 
	pixels.Color(10, 10, 10), 
	pixels.Color(20, 20, 20), 
	pixels.Color(40, 40, 40), 
	pixels.Color(80, 80, 80),
	pixels.Color(150, 150, 150),
	pixels.Color(255, 255, 255)
	};

// OLED uses standard pins for SCL and SDA
U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(SCL, SDA, U8X8_PIN_NONE);	 // OLEDs without Reset of the Display

AsyncWebServer server(80);

void flashSection(int section, int brightness) {

	if (section < 1 || section > cfg.numSections) {
		for (int i = 0; i < NUMPIXELS; i++) {
			pixels.setPixelColor(i, pixelColours[0]);
		}
	    pixels.show();
	    return;
	}

	brightness = normaliseBrightness(brightness);
	
	section -= 1;
	
	int sectionStart = section * (NUMPIXELS / cfg.numSections);
	int sectionEnd = (section + 1) * (NUMPIXELS / cfg.numSections);
	for (int i = 0; i < NUMPIXELS; i++) {
		if (i >= sectionStart && i < sectionEnd) {
			pixels.setPixelColor(i, pixelColours[brightness]);
		} else {
			pixels.setPixelColor(i, pixelColours[0]);
		}
	}
    pixels.show();
}

int normaliseBrightness(int brightness) {

	if (brightness < 0) {
		return 0;
	}
	
	int numBrights = sizeof(pixelColours) / sizeof(uint32_t) - 1;
	if (brightness > numBrights) {
		return numBrights;
	}
	return brightness;
}

void flashAll(int brightness) {

	brightness = normaliseBrightness(brightness);
	for (int i = 0; i < NUMPIXELS; i++) {
		pixels.setPixelColor(i, pixelColours[brightness]);
	}
    pixels.show();
}

void flashNone() {

	for (int i = 0; i < NUMPIXELS; i++) {
		pixels.setPixelColor(i, pixelColours[0]);
	}
    pixels.show();
}

boolean InitWifiSSID(char *ssid, char *pass) {

	int attempts = 0;

	WiFi.mode(WIFI_OFF);
	delay(100);
	WiFi.mode(WIFI_STA);

	// We start by connecting to a WiFi network
	Serial.print("Connecting to ");
	Serial.println(ssid);
	
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, pass);

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
	Serial.print(ssid);
	Serial.print(" on IP address: ");
	Serial.println(WiFi.localIP());
	delay(100);
	return true;
}

bool MenuItemIsActive() {

	if (!cfg.useTwoDrops) {
		if (menuItem == MENU_DROP_TWO_SIZE || 
			menuItem == MENU_DELAY_BETWEEN_DROPS) {
			return false;
		}
	}

	if (cfg.laserMode == off) {
		if (menuItem == MENU_DELAY_AFTER_LASER || 
			menuItem == MENU_ALIGN_LASER) {
			return false;
		}
	} else {
		if (menuItem == MENU_DELAY_BEFORE_DROPS || 
			menuItem == MENU_DROP_ONE_SIZE ||
			menuItem == MENU_USE_TWO_DROPS ||
			menuItem == MENU_DROP_TWO_SIZE ||
			menuItem == MENU_TEST_SOLENOID ||
			menuItem == MENU_DELAY_BETWEEN_DROPS ||
			menuItem == MENU_DELAY_BEFORE_FLASH) {
			return false;
		}
	}

}

void NextMenuItem(bool goingDown) {

	if (goingDown) {
		menuItem++;
	} else {
		menuItem--;
	}
	
	// loop around
	if (menuItem == 0) {
		menuItem = NUM_MENU_ITEMS;	
	} else if (menuItem > NUM_MENU_ITEMS) {
		menuItem = 1;
	}	

	// do this after looping, in case the first or last one should not be active
	if (!MenuItemIsActive()) {
		NextMenuItem(goingDown);
	}
}

void InitServer() {
	
	Serial.println("Initialising server");
	PrintToOLED(0, 5, F("	Init server	 "));
	server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request){
		// get the params here

		String paramVal;
		if (request->hasParam("drop1")) {
			paramVal = request->getParam("drop1")->value();
			cfg.dropOneSize = paramVal.toInt();
		}
		
		if (request->hasParam("drop2")) {
			paramVal = request->getParam("drop2")->value();
			cfg.dropTwoSize = paramVal.toInt();
		}
		
		if (request->hasParam("predelay")) {
			paramVal = request->getParam("predelay")->value();
			cfg.delayBeforeShooting = paramVal.toInt();
		}
		
		if (request->hasParam("postdelay")) {
			paramVal = request->getParam("postdelay")->value();
			cfg.delayAfterShooting = paramVal.toInt();
		}
		
		if (request->hasParam("laserdelay")) {
			paramVal = request->getParam("laserdelay")->value();
			cfg.delayAfterLaser = paramVal.toInt();
		}
		
		if (request->hasParam("dropdelay")) {
			paramVal = request->getParam("dropdelay")->value();
			cfg.delayBeforeDrops = paramVal.toInt();
		}
		if (request->hasParam("betweendelay")) {
			paramVal = request->getParam("betweendelay")->value();
			cfg.delayBetweenDrops = paramVal.toInt();
		}
		
		if (request->hasParam("flashdelay")) {
			paramVal = request->getParam("flashdelay")->value();
			cfg.delayBeforeFlash = paramVal.toInt();
		}

		if (request->hasParam("flashduration")) {
			paramVal = request->getParam("flashduration")->value();
			cfg.flashDuration = paramVal.toInt();
		}
		
		if (request->hasParam("screentimeout")) {
			paramVal = request->getParam("screentimeout")->value();
			cfg.screenTimeout = paramVal.toInt();
		}
		
		if (request->hasParam("twodrops")) {
			paramVal = request->getParam("twodrops")->value();
			paramVal.toLowerCase();
			if (paramVal == "true" || paramVal == "yes" || paramVal == "1") {
				cfg.useTwoDrops = true;
			} else {
				cfg.useTwoDrops = false;
			}
		}
		
		if (request->hasParam("laser")) {
			paramVal = request->getParam("laser")->value();
			paramVal.toLowerCase();
			if (paramVal == "flash") {
				cfg.laserMode = fireflash;
			} else if (paramVal == "camera") {
				cfg.laserMode = firecamera;
			} else {
				cfg.laserMode = off;
			}
		}

		request->send(200, "text/plain", "OK");
	});
	
	server.on("/shoot", HTTP_GET, [](AsyncWebServerRequest *request){
		RunSequence();
		request->send(200, "text/plain", "OK");
	});
	
	PrintToOLED(0, 6, F("  Start server  "));
	server.begin();
}

unsigned long eeprom_crc() {
	const unsigned long crc_table[16] = {
		0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
	};

	unsigned long crc = ~0L;

	for (int index = 0; index < 17 ; ++index) {
		byte epv = EEPROM.read(index);
		crc = crc_table[(crc ^ epv) & 0x0f] ^ (crc >> 4);
		crc = crc_table[(crc ^ (epv >> 4)) & 0x0f] ^ (crc >> 4);
		crc = ~crc;
	}
	return crc;
}

void InitConfig() {

	int crcSize = sizeof(unsigned long);
	int eepromSize = sizeof(cfg) + crcSize;

	Serial.println("Initialising settings");

	EEPROM.begin(eepromSize);
	unsigned long calculatedCrc = eeprom_crc();
	unsigned long storedCrc;

	EEPROM.get(eepromSize - crcSize, storedCrc);
	if (storedCrc != calculatedCrc) {
		// stored val not set, so let's store the default values
		SaveSettings();
		return;
	}

	Serial.println("  - CRC OK getting saved values");

	int eepromPtr = 0;
	EEPROM.get(eepromPtr, cfg.dropOneSize);
	eepromPtr += sizeof(cfg.dropOneSize);
	EEPROM.get(eepromPtr, cfg.dropTwoSize);
	eepromPtr += sizeof(cfg.dropTwoSize);
	EEPROM.get(eepromPtr, cfg.delayBeforeShooting);
	eepromPtr += sizeof(cfg.delayBeforeShooting);
	EEPROM.get(eepromPtr, cfg.delayBeforeDrops);
	eepromPtr += sizeof(cfg.delayBeforeDrops);
	EEPROM.get(eepromPtr, cfg.delayBetweenDrops);
	eepromPtr += sizeof(cfg.delayBetweenDrops);
	EEPROM.get(eepromPtr, cfg.delayBeforeFlash);
	eepromPtr += sizeof(cfg.delayBeforeFlash);
	EEPROM.get(eepromPtr, cfg.delayAfterShooting);
	eepromPtr += sizeof(cfg.delayAfterShooting);
	EEPROM.get(eepromPtr, cfg.delayAfterLaser);
	eepromPtr += sizeof(cfg.delayAfterLaser);
	EEPROM.get(eepromPtr, cfg.flashDuration);
	eepromPtr += sizeof(cfg.flashDuration);
	EEPROM.get(eepromPtr, cfg.cameraDuration);
	eepromPtr += sizeof(cfg.cameraDuration);
	EEPROM.get(eepromPtr, cfg.screenTimeout);
	eepromPtr += sizeof(cfg.screenTimeout);
	EEPROM.get(eepromPtr, cfg.useTwoDrops);
	eepromPtr += sizeof(cfg.useTwoDrops);
	
	EEPROM.get(eepromPtr, cfg.laserMode);
	eepromPtr += sizeof(cfg.laserMode);
	EEPROM.get(eepromPtr, cfg.numSections);
	eepromPtr += sizeof(cfg.numSections);

	Serial.print("cameraDuration read as ");
	Serial.println(cfg.cameraDuration);

}

void SaveSettings() {
	
	Serial.println("Saving settings");

	int eepromPtr = 0;
	EEPROM.put(eepromPtr, cfg.dropOneSize);
	eepromPtr += sizeof(cfg.dropOneSize);
	EEPROM.put(eepromPtr, cfg.dropTwoSize);
	eepromPtr += sizeof(cfg.dropTwoSize);
	EEPROM.put(eepromPtr, cfg.delayBeforeShooting);
	eepromPtr += sizeof(cfg.delayBeforeShooting);
	EEPROM.put(eepromPtr, cfg.delayBeforeDrops);
	eepromPtr += sizeof(cfg.delayBeforeDrops);
	EEPROM.put(eepromPtr, cfg.delayBetweenDrops);
	eepromPtr += sizeof(cfg.delayBetweenDrops);
	EEPROM.put(eepromPtr, cfg.delayBeforeFlash);
	eepromPtr += sizeof(cfg.delayBeforeFlash);
	EEPROM.put(eepromPtr, cfg.delayAfterShooting);
	eepromPtr += sizeof(cfg.delayAfterShooting);
	EEPROM.put(eepromPtr, cfg.delayAfterLaser);
	eepromPtr += sizeof(cfg.delayAfterLaser);
	EEPROM.put(eepromPtr, cfg.flashDuration);
	eepromPtr += sizeof(cfg.flashDuration);
	EEPROM.put(eepromPtr, cfg.cameraDuration);
	eepromPtr += sizeof(cfg.cameraDuration);
	EEPROM.put(eepromPtr, cfg.screenTimeout);
	eepromPtr += sizeof(cfg.screenTimeout);
	EEPROM.put(eepromPtr, cfg.useTwoDrops);
	eepromPtr += sizeof(cfg.useTwoDrops);
	EEPROM.put(eepromPtr, cfg.laserMode);
	eepromPtr += sizeof(cfg.laserMode);
	EEPROM.put(eepromPtr, cfg.numSections);
	eepromPtr += sizeof(cfg.numSections);

	

	EEPROM.commit();
	
	unsigned long calculatedCrc = eeprom_crc();
	EEPROM.put(sizeof(cfg), calculatedCrc);
	EEPROM.commit();
}

void InitGPIO() {

	Serial.println("Initialising GPIO pins");

	pinMode(SOLENOID_PIN, OUTPUT);
	Turn(solenoid,OFF);

	pinMode(FOCUS_PIN, OUTPUT);
	Turn(focus,OFF);

	pinMode(CAMERA_PIN, OUTPUT);
	Turn(camera,OFF);

	pinMode(FLASH_PIN, OUTPUT);
	Turn(flash,OFF);

	pinMode(JOY_UP_PIN, INPUT_PULLUP);
	digitalWrite(JOY_UP_PIN, HIGH);

	pinMode(JOY_DOWN_PIN, INPUT_PULLUP);
	digitalWrite(JOY_DOWN_PIN, HIGH);
	
	pinMode(JOY_LEFT_PIN, INPUT_PULLUP);
	digitalWrite(JOY_LEFT_PIN, HIGH);
	
	pinMode(JOY_RIGHT_PIN, INPUT_PULLUP);
	digitalWrite(JOY_RIGHT_PIN, HIGH);
	
	pinMode(JOY_MID_PIN, INPUT_PULLUP);
	digitalWrite(JOY_MID_PIN, HIGH);

	pinMode(LASER_DETECT, INPUT);

	pinMode(LASER_PIN, OUTPUT);
	Turn(laser,OFF);

	pinMode(NEODATA_PIN, OUTPUT);
}

void InitSerial() {
	Serial.begin(9600);
	delay(1000);
	Serial.println("");
	Serial.println("Initialised serial");
}

void InitWifi() {
	
	bool wifiOne = true;
	bool wifiConnected = false;

	while (!wifiConnected) {
		wifiConnected = wifiOne ? InitWifiSSID(ssid1,pass1) : InitWifiSSID(ssid2,pass2);
		wifiOne = !wifiOne;
	}
}

void setup() {

	InitSerial();
	InitConfig();
	InitGPIO();
	InitOLED();
	InitWifi();
	InitServer();

	delay(400);
	ResetOLED();
	Serial.println("Setup complete");
}

void loop() {

	// first, handle updating the screen from the last rotary	use
	if (updateScreen) {
		updateScreen = false;
		PrintToOLED(0, 0, F("DSLR AUTOTRIGGER"));
		String m = String("Option " + String(menuItem) + " ");
		PrintToOLEDFullLine(3, 2, m);
		switch (menuItem) {
			//#define MENU_RING_MODE					17
			//#define MENU_RING_SECTIONS				18
			case MENU_RING_SECTIONS:
				PrintToOLED(0, 4, F("Num sections    "));
				PrintToOLEDFullLine(4, 6, cfg.numSections);
				break;
			case MENU_DELAY_BEFORE_SHOOTING:
				PrintToOLED(0, 4, F("Pre-shoot delay "));
				PrintToOLEDFullLine(4, 6, cfg.delayBeforeShooting);
				break;
			case MENU_LASER_MODE:
				PrintToOLED(0, 4, F("Use laser       "));
				if (cfg.laserMode == fireflash) {
					PrintToOLEDFullLine(2, 6, F("Yes - Flash"));
				} else if (cfg.laserMode == firecamera) {
					PrintToOLEDFullLine(2, 6, F("Yes - Camera"));
				} else {
					PrintToOLEDFullLine(2, 6, F("No ")); 
				} 
				break;
			case MENU_DELAY_BEFORE_DROPS:
				PrintToOLED(0, 4, F("Pre-drop delay "));
				PrintToOLEDFullLine(4, 6, cfg.delayBeforeDrops);
				break;
			case MENU_DROP_ONE_SIZE:
				PrintToOLED(0, 4, F("First drop size "));
				PrintToOLEDFullLine(4, 6, cfg.dropOneSize);
				break;
			case MENU_USE_TWO_DROPS:
				PrintToOLED(0, 4, F("Use two drops   "));
				if (cfg.useTwoDrops) {
					PrintToOLEDFullLine(4, 6, F("Yes"));
				} else {
					PrintToOLEDFullLine(4, 6, F("No ")); 
				} 
				break;
			case MENU_DROP_TWO_SIZE:
				PrintToOLED(0, 4, F("Second drop size"));
				PrintToOLEDFullLine(4, 6, cfg.dropTwoSize);
				break;
			case MENU_DELAY_BETWEEN_DROPS:
				PrintToOLED(0, 4, F("Inter drop delay"));
				PrintToOLEDFullLine(4, 6, cfg.delayBetweenDrops);
				break;
			case MENU_DELAY_BEFORE_FLASH:
				PrintToOLED(0, 4, F("Pre-flash delay "));
				PrintToOLEDFullLine(4, 6, cfg.delayBeforeFlash);
				break;
			case MENU_DELAY_AFTER_LASER:
				PrintToOLED(0, 4, F("Post-laser delay"));
				PrintToOLEDFullLine(4, 6, cfg.delayAfterLaser);
				break;
			case MENU_DELAY_AFTER_SHOOTING:
				PrintToOLED(0, 4, F("Post shoot delay"));
				PrintToOLEDFullLine(4, 6, cfg.delayAfterShooting);
				break;
			case MENU_SCREEN_TIMEOUT:
				PrintToOLED(0, 4, F("Screen timeout  "));
				PrintToOLEDFullLine(4, 6, cfg.screenTimeout);
				break;
			case MENU_SAVE_SETTINGS:
				PrintToOLED(0, 4, F("Save settings ? "));
				PrintToOLEDFullLine(4, 6, " ");
				break;
			case MENU_TEST_CAMERA:
				PrintToOLED(0, 4, F("Test Camera     "));
				if (cameraTestIsRunning) {
					PrintToOLEDFullLine(4, 6, "On");
				} else {
					PrintToOLEDFullLine(4, 6, "Off");
				}
				break;
			case MENU_TEST_FLASH:
				PrintToOLED(0, 4, F("Test Flash      "));
				PrintToOLEDFullLine(4, 6, " ");
				break;
			case MENU_TEST_SOLENOID:
				PrintToOLED(0, 4, F("Test Solenoid   "));
				if (solenoidTestIsRunning) {
					PrintToOLEDFullLine(4, 6, "On");
				} else {
					PrintToOLEDFullLine(4, 6, "Off");
				}
				break;
			case MENU_ALIGN_LASER:
				PrintToOLED(0, 4, F("Align Laser     "));
				PrintToOLEDFullLine(4, 6, " ");
				break;
		}
	}

	// if we've used the joystick, there will be a delay that we want to
	// do AFTER we update the OLED, but only once. This value will get changed
	// every time we use the joystick, as sometimes it will speed up
	if (delayVal > 0) {
		delay(delayVal);
		delayVal = 0;
	}

	int thisTouch = analogRead(TOUCH_FIRE);
	int thisJoyUp = digitalRead(JOY_UP_PIN);
	int thisJoyDown = digitalRead(JOY_DOWN_PIN);
	int thisJoyLeft = digitalRead(JOY_LEFT_PIN);
	int thisJoyRight = digitalRead(JOY_RIGHT_PIN);

	if (thisJoyLeft == LOW || thisJoyRight == LOW) {
		if (thisJoyLeft == LOW) {
			Serial.println("joy left");
		} else {
			Serial.println("joy right");
		}
		// if the joystick has moved sideways, we change the setting value

		// joystick has been used, so keep the screen on
		screenTimer = cfg.screenTimeout * 1000;

		// also, the screen will need to be updated
		updateScreen = true;					

		joystickRight = (thisJoyRight == LOW);
		if (thisJoyLeft == lastJoyLeft && thisJoyRight == lastJoyRight) {
			// the joystick is the same position as last time, so speed up
			if (sidewaysDelay > 10) {
				sidewaysDelay = sidewaysDelay - 100;
			}
		} else {
			// otherwise, it has changed, so set the delay back to the max
			sidewaysDelay = MAXSIDEWAYSDELAY;
		}

		// modify the selected item
		switch (menuItem) {
			case MENU_RING_SECTIONS:
				cfg.numSections = NormaliseIntVal(cfg.numSections, 1, 1, NUMPIXELS);
			case MENU_DELAY_BEFORE_SHOOTING:
				cfg.delayBeforeShooting = NormaliseIntVal(cfg.delayBeforeShooting, 10, 0, 5000);
				break;
			case MENU_LASER_MODE:
				if (cfg.laserMode == off) {
					cfg.laserMode = fireflash;
				} else if (cfg.laserMode == fireflash) {
					cfg.laserMode = firecamera;
				} else {
					cfg.laserMode = off;
				}
				break;
			case MENU_DELAY_BEFORE_DROPS:
				cfg.delayBeforeDrops = NormaliseIntVal(cfg.delayBeforeDrops, 10, 0, 5000);
				break;
			case MENU_DROP_ONE_SIZE:
				cfg.dropOneSize = NormaliseIntVal(cfg.dropOneSize, 1, 10, 300);
				break;
			case MENU_USE_TWO_DROPS:
				cfg.useTwoDrops = !cfg.useTwoDrops;
				break;
			case MENU_DROP_TWO_SIZE:
				cfg.dropTwoSize = NormaliseIntVal(cfg.dropTwoSize, 1, 10, 300);
				break;
			case MENU_DELAY_BETWEEN_DROPS:
				cfg.delayBetweenDrops = NormaliseIntVal(cfg.delayBetweenDrops, 1, 10, 2000);
				break;
			case MENU_DELAY_BEFORE_FLASH:
				cfg.delayBeforeFlash = NormaliseIntVal(cfg.delayBeforeFlash, 1, 10, 2000);
				break;
			case MENU_DELAY_AFTER_LASER:
				cfg.delayAfterLaser = NormaliseIntVal(cfg.delayAfterLaser, 1, 10, 2000);
				break;
			case MENU_DELAY_AFTER_SHOOTING:
				cfg.delayAfterShooting = NormaliseIntVal(cfg.delayAfterShooting, 1, 0, 5000);
				break;
			case MENU_SCREEN_TIMEOUT:
				cfg.screenTimeout = NormaliseIntVal(cfg.screenTimeout, 1, 1, 20);
				break;
			case MENU_SAVE_SETTINGS:
				SaveSettings();
				PrintToOLEDFullLine(4, 6, "saved");
				updateScreen = false;
				break;
			case MENU_TEST_CAMERA:
				TestCamera();
				break;
			case MENU_TEST_FLASH:
				TestFlash();
				break;
			case MENU_TEST_SOLENOID:
				TestSolenoid();
				break;
			case MENU_ALIGN_LASER:
				RunLaserAlignment();
				break;
		}
		lastJoyLeft = thisJoyLeft;
		lastJoyRight = thisJoyRight;
		delayVal = sidewaysDelay;

	} else if (thisJoyUp == LOW || thisJoyDown == LOW) {

		// joystick has been used, so keep the screen on
		screenTimer = cfg.screenTimeout * 1000;
		// also, the screen will need to be updated
		updateScreen = true;

		NextMenuItem(thisJoyDown == LOW);
		delayVal = downwardsDelay;

		cameraTestIsRunning = false;
		solenoidTestIsRunning = false;

		Turn(camera,OFF);
		Turn(focus,OFF);
		Turn(solenoid,OFF);
		Turn(laser,OFF);
		Turn(flash,OFF);

	} else {

		// check if the joystick button has been pressed, to initiate the process

		int midState = digitalRead(JOY_MID_PIN);
		if (midState == LOW) {
			RunSequence();
		}
		if (cameraTestIsRunning == false) {
			Turn(camera,OFF);
		}
		Turn(focus,OFF);
		Turn(laser,OFF);
		Turn(flash,OFF);
		if (solenoidTestIsRunning == false) {
			Turn(solenoid,OFF);
		}
	}
}

void InitOLED() {

	Serial.println("Initialising OLED");
	u8x8.begin();
	u8x8.setFont(u8x8_font_chroma48medium8_r);
	PrintToOLED(0, 0, F("DSLR AUTOTRIGGER"));
	PrintToOLED(0, 1, F("  Initializing  "));
}

void ResetOLED() {
	ClearScreen();
	PrintToOLED(0, 0, F("DSLR AUTOTRIGGER"));
}

void ClearScreen() {
	u8x8.clearDisplay();
}

void RunSequence() {
	if (cfg.laserMode == off) {
		RunDropperSequence();
	} else {
		RunLaserSequence();
	}
}

void RunDropperSequence() {

	Serial.println("Running dropper sequence");
	ClearScreen();													// want the screen off when shooting
	delay(cfg.delayBeforeShooting);

	Turn(camera,ON);
	delay(cfg.delayBeforeDrops);

	Turn(solenoid,ON);
	delay(cfg.dropOneSize);												// hold the value open for the specified time (milliseconds) 
	Turn(solenoid,OFF);

	if (cfg.useTwoDrops) {
		delay(cfg.delayBetweenDrops);									// keeps valve closed for the time between drips
		Turn(solenoid,ON);
		delay(cfg.dropTwoSize);											// keeps valve open for ValveOpen time (milliseconds)
		Turn(solenoid,OFF);
	}

	delay(cfg.delayBeforeFlash);										// wait the flash delay time to trigger flash
	Turn(flash,ON);
	delay(cfg.flashDuration);											// keep flash trigger pin high long enough to trigger flash
	Turn(flash,OFF);

	delay(cfg.delayAfterShooting);										// wait the flash delay time to trigger flash
	Turn(camera,OFF);

	delay(100);														// keeps the system in a pause mode to avoid false triggers
	Serial.println("Done sequence");
	updateScreen = true;
}

void Turn(Device device, int onOff) {
	switch (device) {
		case camera:
			digitalWrite(CAMERA_PIN, onOff); break;
		case focus:
			digitalWrite(FOCUS_PIN, onOff); break;
		case flash:
			digitalWrite(FLASH_PIN, onOff); break;
		case laser:
			digitalWrite(LASER_PIN, onOff); break;
		case solenoid:
			digitalWrite(SOLENOID_PIN, onOff); break;
	}
}

void RunLaserSequence() {

	Serial.println("Running laser sequence");
	ClearScreen();													// want the screen off when shooting
	
	delay(cfg.delayBeforeShooting);

	bool isArmed = false;
	bool isTriggered = false;
	bool isCancelled = false;

	PrintToOLED(0, 6, F("  Align laser   "));
	Turn(laser,ON);

	while (isArmed == false && isCancelled == false) {

		int midState = digitalRead(JOY_MID_PIN);
		if (midState == LOW) {
			isCancelled = true;
		} else {

		 	int detected = digitalRead(LASER_DETECT);				// read Laser sensor
			if (detected == HIGH) {
				isArmed = true;
			} else {
				delay(100);
			}
		}
	}

	if (isCancelled) {
		updateScreen = true;
		Turn(laser,OFF);
		return;
	}

	PrintToOLED(0, 6, F(" Laser aligned  "));
	delay(1000);

	ClearScreen();													// want the screen off when shooting

	if (cfg.laserMode == fireflash) {
		Turn(camera,ON);
	}

	// system is armed - now wait for it to be triggered - no delays on this bit
	while (isTriggered == false && isCancelled == false) {

	 	int detected = digitalRead(LASER_DETECT);				// read Laser sensor
		if (detected == LOW) {
			isTriggered = true;
		} else {
			int midState = digitalRead(JOY_MID_PIN);
			if (midState == LOW) {
				isCancelled = true;										// user can cancel sequence using the same button as to start
			}
		}
	}

	Turn(laser,OFF);

	if (isCancelled) {
		Turn(camera,OFF);
		updateScreen = true;
		return;
	}

	delay(cfg.delayAfterLaser);											// wait the flash delay time to trigger flash

	if (cfg.laserMode == fireflash) {

		Turn(flash,ON);
		delay(cfg.flashDuration);											// keep flash trigger pin high long enough to trigger flash
		Turn(flash,OFF);

		delay(cfg.delayAfterShooting);										// wait the flash delay time to trigger flash
		Turn(camera,OFF);

	} else {

		Turn(camera,ON);
		delay(cfg.cameraDuration);											// keep camera trigger pin high long enough to trigger camera
		Turn(camera,OFF);
	}


	delay(100);														// keeps the system in a pause mode to avoid false triggers
	Serial.println("Done sequence");

	updateScreen = true;
}

void RunLaserAlignment() {

	Serial.println("Running laser alignment");
	ClearScreen();													// want the screen off when shooting
	
	bool isArmed = false;
	bool isCancelled = false;
	PrintToOLED(0, 3, F("  Align laser   "));

	Turn(laser,ON);

	// getting ready
	while (isCancelled == false) {

		int midState = digitalRead(JOY_MID_PIN);
		if (midState == LOW) {
			isCancelled = true;
		}
		
	 	int detected = digitalRead(LASER_DETECT);				// read Laser sensor
		Serial.print("Laser pin is ");
		if (detected == HIGH) {
			Serial.println("HIGH");
			PrintToOLED(0, 6, F(" Laser aligned  "));
		} else {
			Serial.println("LOW");
			PrintToOLED(0, 6, F(" -------------  "));
		}

		if (isCancelled == false) {
			delay(100);
		}
	}

	Turn(laser,OFF);
	ClearScreen();													// want the screen off when shooting
	updateScreen = true;
}

void TestCamera() {
	Serial.println("Testing camera");

	if (cameraTestIsRunning) {
		Turn(camera,OFF);
	} else {
		Turn(camera,ON);
	}
	cameraTestIsRunning = !cameraTestIsRunning;
}

void TestSolenoid() {
	Serial.println("Testing solenoid");

	if (solenoidTestIsRunning) {
		Turn(solenoid,OFF);
	} else {
		Turn(solenoid,ON);
	}
	solenoidTestIsRunning = !solenoidTestIsRunning;
}

void TestFlash() {

	Turn(flash,ON);
	delay(cfg.flashDuration);	// keep flash trigger pin high long enough to trigger flash
	Turn(flash,OFF);
}

int NormaliseIntVal(int v, int stepamount, int minimum, int maximum) {

	int newValue = v;
	if (joystickRight) {
		newValue += stepamount;
	} else {
		newValue -= stepamount;
	}
	if (newValue < minimum) {
		newValue = minimum;
	} else if (newValue > maximum) {
		newValue = maximum;
	}
	return newValue;
}


void PrintToOLED(int col, int row, int ival) {
	String s = String(ival);
	const char *mes = s.c_str();
	PrintToOLED(col, row, mes);
}

void PrintToOLEDFullLine(int col, int row, int ival) {
	String s = String(ival);
	PrintToOLEDFullLine(col, row, s);
}

void PrintToOLEDFullLine(int col, int row, const char * message) {
	String s = String(message);
	PrintToOLEDFullLine(col, row, s);
}

void PrintToOLED(int col, int row, String	message) {
	const char *mes = message.c_str();
	PrintToOLED(col, row, mes);
}

void PrintToOLED(int col, int row, const char * message) {
	 u8x8.drawString(col, row, message);
}

void PrintToOLEDFullLine(int col, int row, String	message) {
	String blank = "                ";
	int charsAvailable = 16 - col;

	for (int i = 0; i < message.length() && i < charsAvailable; i++) {
		blank.setCharAt(i + col, message.charAt(i));
	}

	const char *mes = blank.c_str();
	PrintToOLED(0, row, mes);
}
