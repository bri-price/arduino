/**************************************************************************
    Souliss - Hello World for Expressif ESP8266 with New WIFI MANAGER 
    and two Capacitive Buttons on pins 12 and 14 with no more wires or resistors.
    
    This is the basic example, create a software push-button on Android
    using SoulissApp (get it from Play Store).  
    
    Load this code on ESP8266 board using the porting of the Arduino core
    for this platform.
*************************************************************************/

***************************************************************************/
/************************     NEW WIFI MANAGER    *************************/
/***************************************************************************/

include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

enum {
  APPLICATION_WEBSERVER = 0,
  ACCESS_POINT_WEBSERVER
};

MDNSResponder mdns;
ESP8266WebServer server(80);
const char* ssid = "esp8266e";	// Use this as the ssid as well
                                // as the mDNS name
const char* passphrase = "esp8266e";
String st;
String content;

boolean start;

/***************************************************************************/
/************************    CAPACITIVE BUTTONS   **************************/
/***************************************************************************/
/*800k - 1MOhm resistor between pins are required*/
#define cap_pin1 14
#define cap_pin2 12

#define DEBUG_CAPSENSE 1

/***************************************************************************/
/****************************   SOULISS    *********************************/
/***************************************************************************/



// Include framework code and libraries
//#include <ESP8266WiFi.h>
#include <EEPROM.h>


// This identify the number of the LED logic
#define MYLEDLOGIC          0               

// **** Define here the right pin for your ESP module **** 
#define	OUTPUTPIN			15


void setup()
{   
  Serial.begin(115200);
  start = setup_esp();
  if(start) Serial.print("TRUE"); else Serial.print("FALSE");
  
  if(start){
    Initialize();



    Set_DimmableLight(MYLEDLOGIC);        // Define a simple LED light logic
	
    pinMode(OUTPUTPIN, OUTPUT);         // Use pin as output 
  }
}
//Souliss_T1n_BrightSwitch
void loop()
{ 
 if (!start) {
   server.handleClient();	// In this example we're not doing too much
 }  
 else 
 { 
    // Here we start to play
    EXECUTEFAST() {                     
        UPDATEFAST();   
        
        FAST_50ms() {   // We process the logic and relevant input and output every 50 milliseconds

//          Serial.print(CapSense(MYLEDLOGIC,Souliss_T1n_ToggleCmd,Souliss_T1n_BrightSwitch,cap_pin1, 2, 1500, 1));
            Serial.print(CapSense(cap_pin1, 2));
            Serial.print("\t");
            Serial.println(CapSense(cap_pin2, 2));
        

           } 
                                   
    }

 }
} 
/*****************************************************************************************************/
/**********************************  SOULISS CAPACITIVE PIN FUNCTION  ********************************/
/*****************************************************************************************************/

int CapSense(int pin, int thresold) {
    
  int cycles = readCapacitivePin(pin); 

  // If pin is on, set the "value"
  if(cycles > thresold)
  {
  	Serial.println("threshold met");
	time = millis();								// Record time
	InPin[pin] = PINSET;
	
	return 1;
  }
 	return 0;

}

/*****************************************************************************************************/
/***********************************NEW FUNCTION 1 CAPACITIVE PIN ************************************/
/*****************************************************************************************************/

uint8_t readCapacitivePin(int pinToMeasure) {
  pinMode(pinToMeasure, OUTPUT);
  digitalWrite(pinToMeasure, LOW);
  delay(1);
  // Prevent the timer IRQ from disturbing our measurement
  noInterrupts();
  // Make the pin an input with the internal pull-up on
  pinMode(pinToMeasure, INPUT_PULLUP);

  // Now see how long the pin to get pulled up. This manual unrolling of the loop
  // decreases the number of hardware cycles between each read of the pin,
  // thus increasing sensitivity.
  uint8_t cycles = 17;
       if (digitalRead(pinToMeasure)) { cycles =  0;}
  else if (digitalRead(pinToMeasure)) { cycles =  1;}
  else if (digitalRead(pinToMeasure)) { cycles =  2;}
  else if (digitalRead(pinToMeasure)) { cycles =  3;}
  else if (digitalRead(pinToMeasure)) { cycles =  4;}
  else if (digitalRead(pinToMeasure)) { cycles =  5;}
  else if (digitalRead(pinToMeasure)) { cycles =  6;}
  else if (digitalRead(pinToMeasure)) { cycles =  7;}
  else if (digitalRead(pinToMeasure)) { cycles =  8;}
  else if (digitalRead(pinToMeasure)) { cycles =  9;}
  else if (digitalRead(pinToMeasure)) { cycles = 10;}
  else if (digitalRead(pinToMeasure)) { cycles = 11;}
  else if (digitalRead(pinToMeasure)) { cycles = 12;}
  else if (digitalRead(pinToMeasure)) { cycles = 13;}
  else if (digitalRead(pinToMeasure)) { cycles = 14;}
  else if (digitalRead(pinToMeasure)) { cycles = 15;}
  else if (digitalRead(pinToMeasure)) { cycles = 16;}

  // End of timing-critical section
  interrupts();

  // Discharge the pin again by setting it low and output
  //  It's important to leave the pins low if you want to 
  //  be able to touch more than 1 sensor at a time - if
  //  the sensor is left pulled high, when you touch
  //  two sensors, your body will transfer the charge between
  //  sensors.
  digitalWrite(pinToMeasure, LOW);
  pinMode(pinToMeasure, OUTPUT);

  return cycles;
}

/************************************************************************************************************/
/***************************************** NEW WIFI MANAGER FUNCTIONS   *************************************/
/************************************************************************************************************/
