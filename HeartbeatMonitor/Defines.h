#define	ON				HIGH
#define OFF				LOW

// UI defines
#define	MAXSIDEWAYSDELAY		501
#define FLASH_DURATION			5
#define CAMERA_DURATION			350

#define WIFI_DELAY				500

#define SEQUENCE_DELAY			10
#define UI_DELAY				100
#define UI_LONG_DELAY			1000

#define NUMPIXELS			60
#define MAXSIZE 30000

#define DEBUG	1

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#define initserial Serial.begin(115200); while(!Serial);
#else
#define debug(x)
#define debugln(x)
#define initserial

#endif
