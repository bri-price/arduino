/*

This sketch is written to work on an ESP32 dev kit board, for which I have designed a circuit board
Use board: "ESP 32 Wrover Module"

	(Note: 0,2,4,15,32,33 are touch pins, 34,35,36,39 are input only)

The sketch is designed to allow users to shoot a camera and fire a flash, to capture images in a controlled manner. It can be run in one of three modes:

	1. Water dropper mode: a solenoid is opened to create one or two water drops, which are then photographed when they hit a liquid below.
		The sequence is as follows:
			Open camera shutter (should be in Bulb mode)
			Open solenoid, wait, close solenoid, to get a drop of water.
			(If configured, wait, then open and close the solenoid again for a second drop of water)
			Wait
			Fire the flash
			Wait
			Close the camera shutter

		Use of a second drop is set in the configuration. All wait times are set in the configuration.

	2. Laser mode (flash): a laser emitter and sensor are aligned, and when something then breaks the beam, a photograph is taken
		The sequence is as follows:
			Open camera shutter (should be in Bulb mode)
			Wait until the beam is broken
			Wait
			Fire the flash
			Wait
			Close the camera shutter

		Configuration screens allow the user to align the laser with the sensor. All wait times are set in the configuration.
		
	3. Laser mode (camera): a laser emitter and sensor are aligned, and when something then breaks the beam, a photograph is taken
		The sequence is as follows:
			Wait until the beam is broken
			Wait
			Shoot the camera
			Wait

		Configuration screens allow the user to align the laser with the sensor. All wait times are set in the configuration.
	
	4. Sound mode (flash): a photograph is taken when the sound level goes high enough
		The sequence is as follows:
			Open camera shutter (should be in Bulb mode)
			Wait until the sound level goes up
			Wait
			Fire the flash
			Wait
			Close the camera shutter

		Configuration screens allow the sound level to be calibrated to the background level. All wait times are set in the configuration.
		
	5. Sound mode (camera): a photograph is taken when the sound level goes high enough
		The sequence is as follows:
			Wait until the sound level goes up
			Wait
			Shoot the camera
			Wait

		Configuration screens allow the sound level to be calibrated to the background level. All wait times are set in the configuration.
	
	The sequences can be triggered by pressing the button on the joystick, a separate button, or touching a capacitive touch plate (depending on config)

All configuration and mode settings can be done either by using a joystick to set the configuration on an attached OLED screen, or by connecting to the controller via a wifi hotspot and sending the configuration values from a web page.

Configuration options are as follows:

		Trigger Mode: 			toggle between the 5 trigger modes

	Mode 1:
	
		Use Two Drops:			true or false - sets the controller to release one or two drops
		Delay Before Shooting:	delay (in ms) between triggering the sequence to opening the camera shutter
		Delay Before Drops:		delay (in ms) between opening the camera shutter and starting the first drop
		Drop One Size:			time (in ms) to open the solenoid for the first drop
		Delay Between Drops:	delay (in ms) between closing the solenoid after the first drop to opening it for the second
		Drop Two Size:			time (in ms) to open the solenoid for the second drop (if configured)
		Delay Before Flash:		delay (in ms) between closing the solenoid after the final drop, to firing the flash

	Modes 2 & 3:
		Delay After Laser:		delay (in ms) between detecting the laser beam is broken and firing the flash (depending on configuration)

	Modes 4 & 5:	
		Delay After Sound:		delay (in ms) between detecting the sound level increase and firing the flash (depending on configuration)

	All modes:
		Delay After Shooting:	delay (in ms) between firing the flash and closing the shutter
		Enable Touch:			enable the capacitive touch sensor to initiate the sequence
		Enable Button:			enable the standalone button to initiate the sequence
		Enable Joy Button:		enable the joystick button to initiate the sequence

	The configuration screens also have the following options:

		Test Camera:			opens/closes the camera shutter, to test the physical cable connection is working
		Test Flash:				fires the flash, to test the physical cable connection is working
		Test Solenoid:			opens/closes the solenoid, to test the physical cable connection is working

		Align Laser:			helps the user align the laser with the sensor
		Calibrate Mic:			reads the background sound level to calibrate the sensor

		Save Settings:			saves all settings to the ESP32 EEPROM


	
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
#include "SPIFFS.h"

#include <EEPROM.h>
#include <U8x8lib.h>
#include <SPI.h>
#include <Wire.h>

#include "Secrets.h"

// you can put up to 3 SSID/PASSWORD combinations in your secrets.h file and the code will try each in turn
char ssid[3][11] = { SECRET_SSID1, SECRET_SSID2, SECRET_SSID2 };
char password[3][64] = { SECRET_PASS1, SECRET_PASS2, SECRET_PASS2 };

#define	ON				HIGH
#define OFF				LOW

enum Device {
	camera,
	focus,
	flash,
	laser,
	solenoid,
	microphone
};

enum TriggerMode {
	buttonTrigger,
	laserTriggerFlash,
	laserShootCamera,
	soundTriggerFlash,
	soundShootCamera
};

//	0,2,4,15,32,33 are touch pins
//	34,35,36,39 are input only

// optocouplers
#define CAMERA_PIN			5
#define FOCUS_PIN			18
#define FLASH_PIN			19

// laser detector
#define LASER_EMIT_PIN		32
#define LASER_DETECT_PIN	35

// sound sensor
#define SOUND_ANALOG_PIN	34

// touch pad
#define TOUCH_PIN			33

// button
#define BUTTON_PIN			36

// solenoid
#define SOLENOID_PIN		13

// joystick
#define JOY_UP_PIN			12
#define JOY_DOWN_PIN		14
#define JOY_LEFT_PIN		27
#define JOY_RIGHT_PIN		26
#define JOY_MID_PIN			25
#define JOY_RST_PIN			15

// menu items
#define MENU_READY_MODE					0
#define MENU_TRIGGER_MODE				1
// dropper values
#define MENU_DELAY_BEFORE_SHOOTING		2
#define MENU_DELAY_BEFORE_DROPS			3
#define MENU_DROP_ONE_SIZE				4
#define MENU_USE_TWO_DROPS				5
#define MENU_DROP_TWO_SIZE				6
#define MENU_DELAY_BETWEEN_DROPS		7
#define MENU_DELAY_BEFORE_FLASH			8
// laser trigger
#define MENU_DELAY_AFTER_LASER			9
// mic trigger
#define MENU_DELAY_AFTER_SOUND			10
// common value
#define MENU_DELAY_AFTER_SHOOTING		11
#define MENU_ALIGN_LASER				12
#define MENU_CALIBRATE_MIC				13
#define MENU_SAVE_SETTINGS				14
#define MENU_TEST_CAMERA				15
#define MENU_TEST_FLASH					16
#define MENU_TEST_SOLENOID				17

#define MENU_ENABLE_TOUCH				18
#define MENU_ENABLE_BUTTON				19
#define MENU_ENABLE_JOYBUTTON			20
#define MENU_DEBUG_MIC					21


// always update this if you add an item to the list above
#define NUM_MENU_ITEMS					21

// configurable values - saved in eeprom
typedef struct {

	TriggerMode triggerMode = buttonTrigger;
	bool useTwoDrops = false;
	int delayBeforeShooting = 1000;
	int delayBeforeDrops = 100;
	int dropOneSize = 95;
	int delayBetweenDrops = 100;
	int dropTwoSize = 95;
	int delayBeforeFlash = 300;
	int delayAfterLaser = 50;
	int delayAfterSound = 0;
	int delayAfterShooting = 100;
	int lastWifiNum = 0;
	bool enableTouch = false;
	bool enableButton = false;
	bool enableJoyButton = true;
	
} CONFIGURATION;

CONFIGURATION cfg;

// UI defines
#define	MAXSIDEWAYSDELAY		500
#define FLASH_DURATION			5
#define CAMERA_DURATION			350

// run time values
bool joystickRight = true;
bool lastJoyLeft = false;
bool lastJoyRight = false;
int menuItem = 0;

bool updateScreen = true;
int delayVal = 0;
int	downwardsDelay = 500;
int	sidewaysDelay = 500;
double accelerationFactor = 1.3;
bool cameraTestIsRunning = false;
bool solenoidTestIsRunning = false;
bool waitingToShoot = false;
bool waitingToTestCamera = false;
bool waitingToTestFlash = false;
bool waitingToTestSolenoid = false;

bool connectedToWifi = false;
int soundCalibrationLevel = 0;
int touchThreshold = 20;

#define DIGITAL_JOYSTICK	1
#define ANALOG_JOYSTICK		2

int joystickType = DIGITAL_JOYSTICK;

// OLED uses standard pins for SCL and SDA
U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(SCL, SDA, U8X8_PIN_NONE);	 // OLEDs without Reset of the Display

// Run a web server on port 80
AsyncWebServer server(80);

boolean InitSPIFFS() {
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return false;
  }
  return true;
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

	// wait for up to 10 seconds to get connected
	while (WiFi.status() != WL_CONNECTED && attempts < 15) {
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
	connectedToWifi = true;
	return true;
}

bool MenuItemIsActive() {

	// determines whether the newly selected menu item
	// should be shown, based on other configured options
	if (!cfg.useTwoDrops) {
		if (menuItem == MENU_DROP_TWO_SIZE || 
			menuItem == MENU_DELAY_BETWEEN_DROPS) {
			return false;
		}
	}

	if (cfg.triggerMode == laserTriggerFlash || cfg.triggerMode == laserShootCamera) {
		if (menuItem == MENU_DELAY_BEFORE_DROPS || 
			menuItem == MENU_DROP_ONE_SIZE ||
			menuItem == MENU_USE_TWO_DROPS ||
			menuItem == MENU_DROP_TWO_SIZE ||
			menuItem == MENU_CALIBRATE_MIC ||
			menuItem == MENU_DEBUG_MIC ||
			menuItem == MENU_DELAY_AFTER_SOUND ||
			menuItem == MENU_TEST_SOLENOID ||
			menuItem == MENU_DELAY_BETWEEN_DROPS ||
			menuItem == MENU_DELAY_BEFORE_FLASH) {
			return false;
		}
	} else if (cfg.triggerMode == soundTriggerFlash || cfg.triggerMode == soundShootCamera) {
		if (menuItem == MENU_DELAY_BEFORE_DROPS || 
			menuItem == MENU_DROP_ONE_SIZE ||
			menuItem == MENU_USE_TWO_DROPS ||
			menuItem == MENU_DROP_TWO_SIZE ||
			menuItem == MENU_ALIGN_LASER ||
			menuItem == MENU_DELAY_AFTER_LASER ||
			menuItem == MENU_TEST_SOLENOID ||
			menuItem == MENU_DELAY_BETWEEN_DROPS ||
			menuItem == MENU_DELAY_BEFORE_FLASH) {
			return false;
		}
	} else {
		if (menuItem == MENU_DELAY_AFTER_LASER || 
			menuItem == MENU_DELAY_AFTER_SOUND ||
			menuItem == MENU_CALIBRATE_MIC ||
			menuItem == MENU_DEBUG_MIC ||
			menuItem == MENU_ALIGN_LASER) {
			return false;
		}
	}
	return true;
}

void NextMenuItem(int menuDirection) {

	// move to the next menu item either up or down. Skip any that should
	// not be active, and loop round if it goes past the first or last
	Serial.print("Menu item from ");
	Serial.print(menuItem);
	Serial.print(" to ");

	menuItem += menuDirection;

	// loop around
	if (menuItem == -1) {
		menuItem = NUM_MENU_ITEMS;	
	} else if (menuItem > NUM_MENU_ITEMS) {
		menuItem = 0;
	}	
	Serial.println(menuItem);

	// do this after looping, in case the first or last one should not be active
	if (!MenuItemIsActive()) {
		NextMenuItem(menuDirection);
	}
}

void handleCors(AsyncWebServerResponse *response)
{
	response->addHeader("Access-Control-Allow-Origin", "*");
}

void InitServer() {
	
	Serial.println("Initialising server");
	PrintToOLED(0, 5, F("	Init server	 "));

	server.on("/", HTTP_GET, [] (AsyncWebServerRequest *request) {
		request->send(SPIFFS, "/index.html", "text/html");
	});
	server.on("/breadbun.css", HTTP_GET, [] (AsyncWebServerRequest *request) {
		request->send(SPIFFS, "/breadbun.css", "text/css");
	});
	server.on("/index.css", HTTP_GET, [] (AsyncWebServerRequest *request) {
		request->send(SPIFFS, "/index.css", "text/css");
	});
	server.on("/autotrigger.js", HTTP_GET, [] (AsyncWebServerRequest *request) {
		request->send(SPIFFS, "/autotrigger.js", "text/script");
	});
	server.on("/jquery-3.3.1.min.js", HTTP_GET, [] (AsyncWebServerRequest *request) {
		request->send(SPIFFS, "/jquery-3.3.1.min.js", "text/script");
	});
	server.on("/blank.png", HTTP_GET, [] (AsyncWebServerRequest *request) {
		request->send(SPIFFS, "/blank.png", "text/script");
	});
	server.on("/check.png", HTTP_GET, [] (AsyncWebServerRequest *request) {
		request->send(SPIFFS, "/check.png", "text/script");
	});
	server.on("/index.html", HTTP_GET, [] (AsyncWebServerRequest *request) {
		request->send(SPIFFS, "/index.html", "text/script");
	});

	server.on("/test/camera", HTTP_POST, [] (AsyncWebServerRequest *request) {
		AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
		response->addHeader("Access-Control-Allow-Origin", "*");
		request->send(response);
		waitingToTestCamera = true;
	});

	server.on("/test/flash", HTTP_POST, [] (AsyncWebServerRequest *request) {
		AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
		response->addHeader("Access-Control-Allow-Origin", "*");
		request->send(response);
		waitingToTestFlash = true;
	});
	
	server.on("/test/solenoid", HTTP_POST, [] (AsyncWebServerRequest *request) {
		AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
		response->addHeader("Access-Control-Allow-Origin", "*");
		request->send(response);
		waitingToTestSolenoid = true;
	});


    server.on("/shoot", HTTP_POST, [] (AsyncWebServerRequest *request) {
		AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
		response->addHeader("Access-Control-Allow-Origin", "*");
		request->send(response);
		waitingToShoot = true;
	});

    server.on("/shoot", HTTP_GET, [] (AsyncWebServerRequest *request) {
		AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
		response->addHeader("Access-Control-Allow-Origin", "*");
		request->send(response);
		waitingToShoot = true;
	});

	server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
		DynamicJsonDocument data(1024);
      	AsyncResponseStream *response = request->beginResponseStream("application/json");
		response->addHeader("Access-Control-Allow-Origin", "*");
      	
		data["triggerMode"] = (int)cfg.triggerMode;
		data["useTwoDrops"] = cfg.useTwoDrops;
		data["delayBeforeShooting"] = cfg.delayBeforeShooting;
		data["delayBeforeDrops"] = cfg.delayBeforeDrops;
		data["dropOneSize"] = cfg.dropOneSize;
		data["delayBetweenDrops"] = cfg.delayBetweenDrops;
		data["dropTwoSize"] = cfg.dropTwoSize;
		data["delayBeforeFlash"] = cfg.delayBeforeFlash;
		data["delayAfterLaser"] = cfg.delayAfterLaser;
		data["delayAfterSound"] = cfg.delayAfterSound;
		data["delayAfterShooting"] = cfg.delayAfterShooting;
		data["enableTouch"] = cfg.enableTouch;
		data["enableButton"] = cfg.enableButton;
		data["enableJoyButton"] = cfg.enableJoyButton;

		serializeJson(data, *response);
		request->send(response);
		Serial.println("API call GET /settings");
	});

	server.on("/settings", HTTP_POST, [] (AsyncWebServerRequest *request) {

		int params = request->params();
		Serial.printf("%d params sent in\n", params);
		for (int i = 0; i < params; i++) {
			AsyncWebParameter *p = request->getParam(i);
			if (p->isPost()) {
				if (p->name() == "body") {

					StaticJsonDocument<1024> doc;
					deserializeJson(doc, (char *)p->value().c_str());
		
					if (doc.containsKey("triggerMode") ) {
						cfg.triggerMode = doc["triggerMode"];
					}
					if (doc.containsKey("useTwoDrops") ) {
						cfg.useTwoDrops = doc["useTwoDrops"];
					}
					if (doc.containsKey("delayBeforeShooting") ) {
						cfg.delayBeforeShooting = doc["delayBeforeShooting"];
					}
					if (doc.containsKey("delayBeforeDrops") ) {
						cfg.delayBeforeDrops = doc["delayBeforeDrops"];
					}
					if (doc.containsKey("dropOneSize") ) {
						cfg.dropOneSize = doc["dropOneSize"];
					}
					if (doc.containsKey("delayBetweenDrops") ) {
						cfg.delayBetweenDrops = doc["delayBetweenDrops"];
					}
					if (doc.containsKey("dropTwoSize") ) {
						cfg.dropTwoSize = doc["dropTwoSize"];
					}
					if (doc.containsKey("delayBeforeFlash") ) {
						cfg.delayBeforeFlash = doc["delayBeforeFlash"];
					}
					if (doc.containsKey("delayAfterLaser") ) {
						cfg.delayAfterLaser = doc["delayAfterLaser"];
					}
					if (doc.containsKey("delayAfterSound") ) {
						cfg.delayAfterSound = doc["delayAfterSound"];
					}
					if (doc.containsKey("delayAfterShooting") ) {
						cfg.delayAfterShooting = doc["delayAfterShooting"];
					}
					if (doc.containsKey("enableTouch") ) {
						cfg.enableTouch = doc["enableTouch"];
					}
					if (doc.containsKey("enableButton") ) {
						cfg.enableButton = doc["enableButton"];
					}
					if (doc.containsKey("enableJoyButton") ) {
						cfg.enableJoyButton = doc["enableJoyButton"];
					}
		
					SaveSettings();
					updateScreen = true;
				}
				AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Done");
				response->addHeader("Access-Control-Allow-Origin", "*");
				request->send(response);
				waitingToShoot = true;
				return;
			}
		}
		AsyncWebServerResponse *response = request->beginResponse(400, "text/plain", "No body");
		response->addHeader("Access-Control-Allow-Origin", "*");
		request->send(response);
	});

	server.on("/settings", HTTP_POST, [] (AsyncWebServerRequest *request) {

		int params = request->params();
		Serial.printf("%d params sent in\n", params);
		for (int i = 0; i < params; i++) {
			AsyncWebParameter *p = request->getParam(i);
			if (p->isPost()) {
				if (p->name() == "body") {

					StaticJsonDocument<1024> doc;
					deserializeJson(doc, (char *)p->value().c_str());
		
					if (doc.containsKey("triggerMode") ) {
						cfg.triggerMode = doc["triggerMode"];
					}
					if (doc.containsKey("useTwoDrops") ) {
						cfg.useTwoDrops = doc["useTwoDrops"];
					}
					if (doc.containsKey("delayBeforeShooting") ) {
						cfg.delayBeforeShooting = doc["delayBeforeShooting"];
					}
					if (doc.containsKey("delayBeforeDrops") ) {
						cfg.delayBeforeDrops = doc["delayBeforeDrops"];
					}
					if (doc.containsKey("dropOneSize") ) {
						cfg.dropOneSize = doc["dropOneSize"];
					}
					if (doc.containsKey("delayBetweenDrops") ) {
						cfg.delayBetweenDrops = doc["delayBetweenDrops"];
					}
					if (doc.containsKey("dropTwoSize") ) {
						cfg.dropTwoSize = doc["dropTwoSize"];
					}
					if (doc.containsKey("delayBeforeFlash") ) {
						cfg.delayBeforeFlash = doc["delayBeforeFlash"];
					}
					if (doc.containsKey("delayAfterLaser") ) {
						cfg.delayAfterLaser = doc["delayAfterLaser"];
					}
					if (doc.containsKey("delayAfterSound") ) {
						cfg.delayAfterSound = doc["delayAfterSound"];
					}
					if (doc.containsKey("delayAfterShooting") ) {
						cfg.delayAfterShooting = doc["delayAfterShooting"];
					}
					if (doc.containsKey("enableTouch") ) {
						cfg.enableTouch = doc["enableTouch"];
					}
					if (doc.containsKey("enableButton") ) {
						cfg.enableButton = doc["enableButton"];
					}
					if (doc.containsKey("enableJoyButton") ) {
						cfg.enableJoyButton = doc["enableJoyButton"];
					}
		
					SaveSettings();
					updateScreen = true;
					AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Done");
					response->addHeader("Access-Control-Allow-Origin", "*");
					request->send(response);
					return;
				}
			}
		}

		AsyncWebServerResponse *response = request->beginResponse(400, "text/plain", "No body");
		response->addHeader("Access-Control-Allow-Origin", "*");
		request->send(response);
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
	int eepromSize = sizeof(cfg);
	for (int index = 0; index < eepromSize ; ++index) {
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
	EEPROM.get(eepromPtr, cfg.triggerMode);
	eepromPtr += sizeof(cfg.triggerMode);
	EEPROM.get(eepromPtr, cfg.useTwoDrops);
	eepromPtr += sizeof(cfg.useTwoDrops);
	EEPROM.get(eepromPtr, cfg.delayBeforeShooting);
	eepromPtr += sizeof(cfg.delayBeforeShooting);
	EEPROM.get(eepromPtr, cfg.delayBeforeDrops);
	eepromPtr += sizeof(cfg.delayBeforeDrops);
	EEPROM.get(eepromPtr, cfg.dropOneSize);
	eepromPtr += sizeof(cfg.dropOneSize);
	EEPROM.get(eepromPtr, cfg.delayBetweenDrops);
	eepromPtr += sizeof(cfg.delayBetweenDrops);
	EEPROM.get(eepromPtr, cfg.dropTwoSize);
	eepromPtr += sizeof(cfg.dropTwoSize);
	EEPROM.get(eepromPtr, cfg.delayBeforeFlash);
	eepromPtr += sizeof(cfg.delayBeforeFlash);
	EEPROM.get(eepromPtr, cfg.delayAfterLaser);
	eepromPtr += sizeof(cfg.delayAfterLaser);
	EEPROM.get(eepromPtr, cfg.delayAfterSound);
	eepromPtr += sizeof(cfg.delayAfterSound);
	EEPROM.get(eepromPtr, cfg.delayAfterShooting);
	eepromPtr += sizeof(cfg.delayAfterShooting);
	EEPROM.get(eepromPtr, cfg.lastWifiNum);
	eepromPtr += sizeof(cfg.lastWifiNum);
	EEPROM.get(eepromPtr, cfg.enableTouch);
	eepromPtr += sizeof(cfg.enableTouch);
	EEPROM.get(eepromPtr, cfg.enableButton);
	eepromPtr += sizeof(cfg.enableButton);
	EEPROM.get(eepromPtr, cfg.enableJoyButton);
	eepromPtr += sizeof(cfg.enableJoyButton);
}

void SaveSettings() {
	
	Serial.println("Saving settings");

	int eepromPtr = 0;
	EEPROM.put(eepromPtr, cfg.triggerMode);
	eepromPtr += sizeof(cfg.triggerMode);
	EEPROM.put(eepromPtr, cfg.useTwoDrops);
	eepromPtr += sizeof(cfg.useTwoDrops);
	EEPROM.put(eepromPtr, cfg.delayBeforeShooting);
	eepromPtr += sizeof(cfg.delayBeforeShooting);
	EEPROM.put(eepromPtr, cfg.delayBeforeDrops);
	eepromPtr += sizeof(cfg.delayBeforeDrops);
	EEPROM.put(eepromPtr, cfg.dropOneSize);
	eepromPtr += sizeof(cfg.dropOneSize);
	EEPROM.put(eepromPtr, cfg.delayBetweenDrops);
	eepromPtr += sizeof(cfg.delayBetweenDrops);
	EEPROM.put(eepromPtr, cfg.dropTwoSize);
	eepromPtr += sizeof(cfg.dropTwoSize);
	EEPROM.put(eepromPtr, cfg.delayBeforeFlash);
	eepromPtr += sizeof(cfg.delayBeforeFlash);
	EEPROM.put(eepromPtr, cfg.delayAfterLaser);
	eepromPtr += sizeof(cfg.delayAfterLaser);
	EEPROM.put(eepromPtr, cfg.delayAfterSound);
	eepromPtr += sizeof(cfg.delayAfterSound);
	EEPROM.put(eepromPtr, cfg.delayAfterShooting);
	eepromPtr += sizeof(cfg.delayAfterShooting);
	EEPROM.put(eepromPtr, cfg.lastWifiNum);
	eepromPtr += sizeof(cfg.lastWifiNum);
	EEPROM.put(eepromPtr, cfg.enableTouch);
	eepromPtr += sizeof(cfg.enableTouch);
	EEPROM.put(eepromPtr, cfg.enableButton);
	eepromPtr += sizeof(cfg.enableButton);
	EEPROM.put(eepromPtr, cfg.enableJoyButton);
	eepromPtr += sizeof(cfg.enableJoyButton);
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

	if (joystickType == DIGITAL_JOYSTICK) {

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
		
		pinMode(JOY_RST_PIN, INPUT_PULLUP);
		digitalWrite(JOY_RST_PIN, HIGH);

	} else {

		pinMode(JOY_RIGHT_PIN, INPUT_PULLUP);
		digitalWrite(JOY_RIGHT_PIN, HIGH);
		
		pinMode(JOY_UP_PIN, OUTPUT);
		digitalWrite(JOY_UP_PIN, HIGH);
	}
	
	pinMode(LASER_DETECT_PIN, INPUT);
	pinMode(LASER_EMIT_PIN, OUTPUT);
	pinMode(BUTTON_PIN, INPUT_PULLUP);
	Turn(laser,OFF);
}

void InitSerial() {
	Serial.begin(9600);
	delay(1000);
	Serial.println("");
	Serial.println("Initialised serial");
	delay(1000);
}

void InitWifi() {
	
	int wifiNum = cfg.lastWifiNum;
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
			Serial.println("WiFi failed after 6 attempts");
			break;
		}
	}
}

void setup() {

	InitSerial();
	Serial.println("Init Config");
	InitConfig();
	Serial.println("Init GPIO");
	InitGPIO();
	Serial.println("Init OLED");
	InitOLED();

	if (joystickType == DIGITAL_JOYSTICK) {
		Serial.println("Init Wifi");
		InitWifi();
		Serial.println("Init Server");
		InitServer();
	}

	Serial.println("Init SPIFFS");
	InitSPIFFS();
	
	delay(400);
	Serial.println("ResetOLED");
	ResetOLED();
	Serial.println("Setup complete");
}

void loop() {

	// first, handle updating the screen from the last rotary	use
	if (updateScreen) {
		updateScreen = false;
		PrintToOLED(0, 0, F("DSLR TRIGGER 1.6"));
		if (connectedToWifi) {
			PrintToOLEDFullLine(0, 1, WiFi.localIP().toString());
		} else {
			PrintToOLEDFullLine(0, 1, "disconnected");
		}

		if (menuItem > 0) {
			String m = String("Option " + String(menuItem) + " ");
			PrintToOLEDFullLine(3, 3, m);
		}
		switch (menuItem) {
			case MENU_READY_MODE:
				PrintToOLED(0, 3, F("     READY      "));
				PrintToOLED(0, 4, F("                "));
				PrintToOLED(0, 6, F("                "));
				break;
			case MENU_TRIGGER_MODE:
				PrintToOLED(0, 4, F("Trigger Mode    "));
				if (cfg.triggerMode == laserTriggerFlash) {
					PrintToOLEDFullLine(0, 6, F("Laser - Flash   "));
				} else if (cfg.triggerMode == laserShootCamera) {
					PrintToOLEDFullLine(0, 6, F("Laser - Camera  "));
				} else if (cfg.triggerMode == soundTriggerFlash) {
					PrintToOLEDFullLine(0, 6, F("Sound - Flash   "));
				} else if (cfg.triggerMode == soundShootCamera) {
					PrintToOLEDFullLine(0, 6, F("Sound - Camera  "));
				} else {
					PrintToOLEDFullLine(0, 6, F("Button          ")); 
				} 
				break;
			case MENU_USE_TWO_DROPS:
				PrintToOLED(0, 4, F("Use two drops   "));
				if (cfg.useTwoDrops) {
					PrintToOLEDFullLine(4, 6, F("Yes"));
				} else {
					PrintToOLEDFullLine(4, 6, F("No ")); 
				} 
				break;
			case MENU_ENABLE_TOUCH:
				PrintToOLED(0, 4, F("Enable touch pad"));
				if (cfg.enableTouch) {
					PrintToOLEDFullLine(4, 6, F("Yes"));
				} else {
					PrintToOLEDFullLine(4, 6, F("No ")); 
				} 
				break;
			case MENU_ENABLE_BUTTON:
				PrintToOLED(0, 4, F("Enable button   "));
				if (cfg.enableButton) {
					PrintToOLEDFullLine(4, 6, F("Yes"));
				} else {
					PrintToOLEDFullLine(4, 6, F("No ")); 
				} 
				break;
			case MENU_ENABLE_JOYBUTTON:
				PrintToOLED(0, 4, F("Enable joystick "));
				if (cfg.enableJoyButton) {
					PrintToOLEDFullLine(4, 6, F("Yes"));
				} else {
					PrintToOLEDFullLine(4, 6, F("No ")); 
				} 
				break;
			case MENU_DELAY_BEFORE_SHOOTING:
				PrintToOLED(0, 4, F("Pre-shoot delay "));
				PrintToOLEDFullLine(4, 6, cfg.delayBeforeShooting);
				break;
			case MENU_DELAY_BEFORE_DROPS:
				PrintToOLED(0, 4, F("Pre-drop delay "));
				PrintToOLEDFullLine(4, 6, cfg.delayBeforeDrops);
				break;
			case MENU_DROP_ONE_SIZE:
				PrintToOLED(0, 4, F("First drop size "));
				PrintToOLEDFullLine(4, 6, cfg.dropOneSize);
				break;
			case MENU_DELAY_BETWEEN_DROPS:
				PrintToOLED(0, 4, F("Inter drop delay"));
				PrintToOLEDFullLine(4, 6, cfg.delayBetweenDrops);
				break;
			case MENU_DROP_TWO_SIZE:
				PrintToOLED(0, 4, F("Second drop size"));
				PrintToOLEDFullLine(4, 6, cfg.dropTwoSize);
				break;
			case MENU_DELAY_BEFORE_FLASH:
				PrintToOLED(0, 4, F("Pre-flash delay "));
				PrintToOLEDFullLine(4, 6, cfg.delayBeforeFlash);
				break;
			case MENU_DELAY_AFTER_LASER:
				PrintToOLED(0, 4, F("Post-laser delay"));
				PrintToOLEDFullLine(4, 6, cfg.delayAfterLaser);
				break;
			case MENU_DELAY_AFTER_SOUND:
				PrintToOLED(0, 4, F("Post-sound delay"));
				PrintToOLEDFullLine(4, 6, cfg.delayAfterSound);
				break;
			case MENU_DELAY_AFTER_SHOOTING:
				PrintToOLED(0, 4, F("Post shoot delay"));
				PrintToOLEDFullLine(4, 6, cfg.delayAfterShooting);
				break;
			case MENU_ALIGN_LASER:
				PrintToOLED(0, 4, F("Align Laser     "));
				PrintToOLEDFullLine(4, 6, " ");
				break;
			case MENU_CALIBRATE_MIC:
				PrintToOLED(0, 4, F("Calibrate Mic   "));
				PrintToOLEDFullLine(4, 6, " ");
				break;
			case MENU_DEBUG_MIC:
				PrintToOLED(0, 4, F("Debug Mic   "));
				PrintToOLEDFullLine(4, 6, " ");
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
		}
	}

	// if we've used the joystick, there will be a delay that we want to
	// do AFTER we update the OLED, but only once. This value will get changed
	// every time we use the joystick, as sometimes it will speed up
	if (delayVal > 0) {
		delay(delayVal);
		delayVal = 0;
	}

	bool thisTouch = false;
	bool thisButton = false;
	bool thisJoyUp = false;
	bool thisJoyDown = false;
	bool thisJoyLeft = false;
	bool thisJoyRight = false;
	bool thisMidState = false;
	bool thisRstState = false;

	if (joystickType == DIGITAL_JOYSTICK) {
		int iJoyUp = digitalRead(JOY_UP_PIN);
		int iJoyDown = digitalRead(JOY_DOWN_PIN);
		int iJoyLeft = digitalRead(JOY_LEFT_PIN);
		int iJoyRight = digitalRead(JOY_RIGHT_PIN);

		thisJoyUp = (iJoyUp == LOW);
		thisJoyDown = (iJoyDown == LOW);
		thisJoyLeft = (iJoyLeft == LOW);
		thisJoyRight = (iJoyRight == LOW);

		if (cfg.enableJoyButton) {
			int imidState = digitalRead(JOY_MID_PIN);
			thisMidState = (imidState == LOW);
		}
		if (cfg.enableTouch) {
			int iTouch = touchRead(TOUCH_PIN);
			thisTouch = (iTouch < touchThreshold);
		}
		if (cfg.enableButton) {
			int iButton = digitalRead(BUTTON_PIN);
			thisButton = (iButton == LOW);
		}
		int irstState = digitalRead(JOY_RST_PIN);
		thisRstState = (irstState == LOW);
	
	} else {

		int iJoyLeftRight = analogRead(JOY_LEFT_PIN);
		int iJoyUpDown = analogRead(JOY_DOWN_PIN);

		thisJoyUp = (iJoyUpDown < 10);
		thisJoyDown = (iJoyUpDown > 4080);
		thisJoyLeft = (iJoyLeftRight > 4080);
		thisJoyRight = (iJoyLeftRight < 10);

		if (cfg.enableJoyButton) {
			int imidState = digitalRead(JOY_RIGHT_PIN);
			thisMidState = (imidState == LOW);
		}
		if (cfg.enableTouch) {
			int iTouch = digitalRead(TOUCH_PIN);
			thisTouch = (iTouch == LOW);
		}
		if (cfg.enableButton) {
			int iButton = digitalRead(BUTTON_PIN);
			thisButton = (iButton == LOW);
		}
		delay(100);
	}

	if (waitingToTestCamera) {
		waitingToTestCamera = false;
		TestCamera();
	}

	if (waitingToTestFlash) {
		waitingToTestFlash = false;
		TestFlash();
	}

	if (waitingToTestSolenoid) {
		waitingToTestSolenoid = false;
		TestSolenoid();
	}

	if (thisJoyLeft || thisJoyRight) {

		if (thisJoyLeft)
			Serial.println("joy left");
		else 
			Serial.println("joy right");

		// if the joystick has moved sideways, we change the setting value
		// also, the screen will need to be updated
		updateScreen = true;					

		joystickRight = thisJoyRight;
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
			case MENU_READY_MODE:
				break;
			case MENU_TRIGGER_MODE:
				cfg.triggerMode = (TriggerMode)NormaliseIntVal((int)cfg.triggerMode, 1, 0, 4, true);
				break;
			case MENU_USE_TWO_DROPS:
				cfg.useTwoDrops = !cfg.useTwoDrops;
				break;
			case MENU_ENABLE_TOUCH:
				cfg.enableTouch = !cfg.enableTouch;
				break;
			case MENU_ENABLE_BUTTON:
				cfg.enableButton = !cfg.enableButton;
				break;
			case MENU_ENABLE_JOYBUTTON:
				cfg.enableJoyButton = !cfg.enableJoyButton;
				break;
			case MENU_DELAY_BEFORE_SHOOTING:
				cfg.delayBeforeShooting = NormaliseIntVal(cfg.delayBeforeShooting, 10, 0, 5000, false);
				break;
			case MENU_DELAY_BEFORE_DROPS:
				cfg.delayBeforeDrops = NormaliseIntVal(cfg.delayBeforeDrops, 10, 0, 5000, false);
				break;
			case MENU_DROP_ONE_SIZE:
				cfg.dropOneSize = NormaliseIntVal(cfg.dropOneSize, 1, 10, 300, false);
				break;
			case MENU_DELAY_BETWEEN_DROPS:
				cfg.delayBetweenDrops = NormaliseIntVal(cfg.delayBetweenDrops, 1, 10, 2000, false);
				break;
			case MENU_DROP_TWO_SIZE:
				cfg.dropTwoSize = NormaliseIntVal(cfg.dropTwoSize, 1, 10, 300, false);
				break;
			case MENU_DELAY_BEFORE_FLASH:
				cfg.delayBeforeFlash = NormaliseIntVal(cfg.delayBeforeFlash, 1, 10, 2000, false);
				break;
			case MENU_DELAY_AFTER_LASER:
				cfg.delayAfterLaser = NormaliseIntVal(cfg.delayAfterLaser, 1, 0, 2000, false);
				break;
			case MENU_DELAY_AFTER_SOUND:
				cfg.delayAfterSound = NormaliseIntVal(cfg.delayAfterSound, 1, 0, 2000, false);
				break;
			case MENU_DELAY_AFTER_SHOOTING:
				cfg.delayAfterShooting = NormaliseIntVal(cfg.delayAfterShooting, 1, 0, 5000, false);
				break;
			case MENU_ALIGN_LASER:
				RunLaserAlignment();
				break;
			case MENU_CALIBRATE_MIC:
				RunMicrophoneCalibration();
				break;
			case MENU_DEBUG_MIC:
				ShowMicrophoneOutput();
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
		}
		lastJoyLeft = thisJoyLeft;
		lastJoyRight = thisJoyRight;
		delayVal = sidewaysDelay;

	} else if (thisJoyUp || thisJoyDown) {

		if (thisJoyUp)
			Serial.println("joy up");
		else 
			Serial.println("joy down");

		updateScreen = true;

		NextMenuItem(thisJoyUp ? -1 : 1);
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
		if (thisMidState || thisButton || thisTouch || waitingToShoot) {

			if (thisMidState)
				Serial.println("joy middle");
			else if (thisButton)
				Serial.println("button");
			else if (thisTouch)
				Serial.println("touch");

			waitingToShoot = false;
			RunSequence();
		}

		if (thisRstState) {
			SaveSettings();
			PrintToOLEDFullLine(4, 6, "saved");
			updateScreen = false;
			thisRstState = false;
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
	PrintToOLED(0, 0, F("DSLR TRIGGER 1.6"));
	PrintToOLED(0, 1, F("  Initializing  "));
}

void ResetOLED() {
	ClearScreen();
	PrintToOLED(0, 0, F("DSLR TRIGGER 1.6"));
}

void ClearScreen() {
	u8x8.clearDisplay();
}

// this is just here to make the rest of the sequence code look more readable
void Turn(Device device, int onOff) {
	switch (device) {
		case camera:
			digitalWrite(CAMERA_PIN, onOff); 
			break;
		case focus:
			digitalWrite(FOCUS_PIN, onOff); 
			break;
		case flash:
			digitalWrite(FLASH_PIN, onOff); 
			break;
		case laser:
			digitalWrite(LASER_EMIT_PIN, onOff); 
			break;
		case solenoid:
			digitalWrite(SOLENOID_PIN, onOff); 
			break;
	}
}

void RunSequence() {
	if (cfg.triggerMode == buttonTrigger) {
		RunDropperSequence();
	} else if (cfg.triggerMode == laserTriggerFlash || cfg.triggerMode == laserShootCamera) {
		RunLaserSequence();
	} else {
		RunListenerSequence();
	}
}

void RunDropperSequence() {

	Serial.println("Running dropper sequence");

	ClearScreen();						// turn the screen off when shooting

	delay(cfg.delayBeforeShooting);		// wait then turn on the camera
	Serial.println("Turn camera on");
	Turn(camera,ON);

	delay(cfg.delayBeforeDrops);		// wait before doing the first drop

	Serial.println("Turn solenoid on");
	Turn(solenoid,ON);
	delay(cfg.dropOneSize);				// open the solenoid for the configured amount of time for the first drop
	Serial.println("Turn solenoid off");
	Turn(solenoid,OFF);

	if (cfg.useTwoDrops) {
		delay(cfg.delayBetweenDrops);	// wait for the next drop
		
		Serial.println("Turn solenoid on");
		Turn(solenoid,ON);				// open the solenoid for the configured amount of time for the second drop
		delay(cfg.dropTwoSize);			
		Serial.println("Turn solenoid off");
		Turn(solenoid,OFF);
	}

	delay(cfg.delayBeforeFlash);		// wait before firing the flash
	
	Serial.println("Fire flash");
	Turn(flash,ON);						// turn on the flash for the configured amount of time
	delay(FLASH_DURATION);
	Turn(flash,OFF);

	delay(cfg.delayAfterShooting);		// wait before turning off the camera
	Serial.println("Turn camera off");
	Turn(camera,OFF);

	delay(10);
	Serial.println("Done sequence");
	updateScreen = true;
}

void RunLaserSequence() {

	Serial.println("Running laser sequence");
	
	delay(cfg.delayBeforeShooting);		// wait before startig

	bool isArmed = false;
	bool isTriggered = false;
	bool isCancelled = false;

	PrintToOLED(0, 6, F("  Align laser   "));
	Turn(laser,ON);

	// wait for laser to be aligned or user to cancel
	while (isArmed == false && isCancelled == false) {

		int midState = digitalRead(JOY_MID_PIN);
		if (midState == LOW) {
			isCancelled = true;
		} else {

		 	int detected = digitalRead(LASER_DETECT_PIN);				// read Laser sensor
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

	ClearScreen();					// turn the screen off when shooting

	if (cfg.triggerMode == laserTriggerFlash) {
		Turn(camera,ON);
	}

	// system is armed - now wait for it to be triggered - no delays on this bit
	while (isTriggered == false && isCancelled == false) {

	 	int detected = digitalRead(LASER_DETECT_PIN);				// read Laser sensor
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

	if (cfg.delayAfterLaser > 0) {
		delay(cfg.delayAfterLaser);											// wait the flash delay time to trigger flash
	}

	if (cfg.triggerMode == laserTriggerFlash) {

		Turn(flash,ON);
		delay(FLASH_DURATION);											// keep flash trigger pin high long enough to trigger flash
		Turn(flash,OFF);

		delay(cfg.delayAfterShooting);										// wait the flash delay time to trigger flash
		Turn(camera,OFF);

	} else {

		Turn(camera,ON);
		delay(CAMERA_DURATION);											// keep camera trigger pin high long enough to trigger camera
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
		
	 	int detected = digitalRead(LASER_DETECT_PIN);				// read Laser sensor
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

void RunListenerSequence() {

	Serial.println("Running listener sequence");
	Serial.print("soundCalibrationLevel = ");
	Serial.println(soundCalibrationLevel);
	
	delay(cfg.delayBeforeShooting);		// wait before startig

	bool isArmed = false;
	bool isTriggered = false;
	bool isCancelled = false;

	ClearScreen();					// turn the screen off when shooting

	if (cfg.triggerMode == soundTriggerFlash) {
		Turn(camera,ON);
	}

	// system is armed - now wait for it to be triggered - no delays on this bit
	while (isTriggered == false && isCancelled == false) {

	 	int detected = analogRead(SOUND_ANALOG_PIN);				// read Laser sensor
		if (detected < soundCalibrationLevel) {
			Serial.print("detectedLevel = ");
			Serial.println(detected);
			isTriggered = true;
		} else {
			int midState = digitalRead(JOY_MID_PIN);
			if (midState == LOW) {
				isCancelled = true;										// user can cancel sequence using the same button as to start
			}
		}
	}

	if (isCancelled) {
		Turn(camera,OFF);
		updateScreen = true;
		return;
	}

	if (cfg.delayAfterSound > 0) {
		delay(cfg.delayAfterSound);											// wait the flash delay time to trigger flash
	}
	
	if (cfg.triggerMode == soundTriggerFlash) {

		Turn(flash,ON);
		delay(FLASH_DURATION);											// keep flash trigger pin high long enough to trigger flash
		Turn(flash,OFF);

		delay(cfg.delayAfterShooting);										// wait the flash delay time to trigger flash
		Turn(camera,OFF);

	} else {	// soundTriggerCamera

		Turn(camera,ON);
		delay(CAMERA_DURATION);											// keep camera trigger pin high long enough to trigger camera
		Turn(camera,OFF);
	}


	delay(100);														// keeps the system in a pause mode to avoid false triggers
	Serial.println("Done sequence");

	updateScreen = true;
}

void RunMicrophoneCalibration() {

	Serial.println("Running microphone calibration");
	ClearScreen();													// want the screen off when shooting
	
	bool isArmed = false;
	bool isCancelled = false;
	PrintToOLED(0, 3, F(" Calibrate mike "));

	int calibrationCount = 0;
	soundCalibrationLevel = 0;
	// getting ready
	while (isCancelled == false && calibrationCount < 10000) {

		int midState = digitalRead(JOY_MID_PIN);
		if (midState == LOW) {
			isCancelled = true;
		}
		
	 	int detected = analogRead(SOUND_ANALOG_PIN);				// read Laser sensor
	 	if (soundCalibrationLevel == 0) {
			soundCalibrationLevel = detected;
	 	} else {
	 		if (detected < soundCalibrationLevel) {
				soundCalibrationLevel = detected;
	 		}
	 	}

		calibrationCount++;
	}
	soundCalibrationLevel -= 80;
		Serial.print("   New calibration = ");
		Serial.println(soundCalibrationLevel);

	ClearScreen();													// want the screen off when shooting
	updateScreen = true;
}

void ShowMicrophoneOutput() {

	Serial.println("Debugging microphone output");
	ClearScreen();													// want the screen off when shooting
	
	bool isArmed = false;
	bool isCancelled = false;
	PrintToOLED(0, 3, F(" Debug mike "));

	// getting ready
	while (isCancelled == false) {

		int midState = digitalRead(JOY_MID_PIN);
		if (midState == LOW) {
			isCancelled = true;
		}
		
	 	int detected = analogRead(SOUND_ANALOG_PIN);				// read Laser sensor
	 	Serial.print("Sound level = ");
	 	Serial.println(detected);
		delay(10);
	}

	ClearScreen();													// want the screen off when shooting
	updateScreen = true;
}

void TestCamera() {
	Serial.println("Testing camera");
	Turn(camera,(cameraTestIsRunning ? OFF: ON));
	cameraTestIsRunning = !cameraTestIsRunning;
}

void TestSolenoid() {
	Serial.println("Testing solenoid");
	Turn(solenoid,(solenoidTestIsRunning ? OFF: ON));
	solenoidTestIsRunning = !solenoidTestIsRunning;
}

void TestFlash() {
	Turn(flash,ON);
	delay(FLASH_DURATION);	// keep flash trigger pin high long enough to trigger flash
	Turn(flash,OFF);
}

int NormaliseIntVal(int v, int stepamount, int minimum, int maximum, bool loopValues) {

//	int newValue = v;
//	if (joystickRight) {
//		newValue += stepamount;
//	} else {
//		newValue -= stepamount;
//	}

	int newValue = v + (stepamount * (joystickRight ? 1 : -1));
	
	if (newValue < minimum) {
//		newValue = maximum;
		newValue = (loopValues ? maximum : minimum);
	} else if (newValue > maximum) {
		newValue = (loopValues ? minimum : maximum);
//		newValue = maximum;
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

void PrintToOLED(int col, int row, String message) {
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
