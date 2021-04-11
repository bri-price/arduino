/**************************************************************************
 * 
 * ESP8266 NodeMCU stepper motor control with rotary encoder.
 * This is a free software with NO WARRANTY.
 * https://simple-circuit.com/
 *
 *************************************************************************/
 
// include Arduino stepper motor library
#include <AccelStepper.h>
#include <ESP8266WiFi.h>        // Include the Wi-Fi library
#include <ESP8266WebServer.h>
ESP8266WebServer server(80);

#define AUTORUN
#define FULLSTEP 4

// RELEASE VALUES
const char *ssid = "Prometheus"; // The name of the Wi-Fi network that will be created
const char *password = "quercushs";   // The password required to connect to it, leave blank for an open network

// DEBUG VALUES
//const char *ssid = "Nebula1"; // The name of the Wi-Fi network that will be created
//const char *password = "ironmaiden88";   // The password required to connect to it, leave blank for an open network

 
// number of steps per one revolution is 2048 ( = 4096 half steps)
#define STEPS  2048
 
// stepper motor control pins
#define IN1               D1
#define IN2               D2
#define IN3               D3
#define IN4               D4
 
#define redPin            D5
#define greenPin          D6
#define bluePin           D7

#define cameraShootPin    D8
#define cameraFocusPin    D9

#define LowFull           40
#define MediumFull        100
#define HighFull          220

#define LowPart           20
#define MediumPart        40
#define HighPart          90

// initialize stepper library
AccelStepper stepper(FULLSTEP, IN4, IN2, IN3, IN1);

// main loop stuff
int loopDelay = 100;

// motor stuff
int stepSpeed =  -170;
int oldStepSpeed = -170;
bool stepActive = false;

// LED stuff
int ledBrightness = 2;
enum State {
  unknown,
  poweredOn,
  connectingHotspot,
  connectedHotspot,
  connectedApp,
  runningMotor,
  exposingCamera,
  runningExposing,
  autoCameraControl
};

enum RunMode {
  manual,
  automaticExposure,
  fullyAutomatic
};

enum ManualExposureCommand {
  none,
  startExposure,
  stopExposure,
  shootOnce
};

int currentBlink = 0;
int changeBlink = 10;


enum ShootingState {
  inactive,
  waitStart,
  shooting,
  shootGap,
  waitStop,
  complete,
  cancelling
};


State state = poweredOn;

// shooting stuff

ShootingState shootingState = inactive;
int numShots = 5;
int shotDuration = 100;
int shotGap = 20;
int startGap = 50;
bool isShooting = false;
int countShots = 0;

ManualExposureCommand manualExposure = none;       //
RunMode runMode = manual;
int exposureDuration = 1000;  // exposure duration in ms if in bulb mode
//int timeBetweenExposures = 2000;  // delay in ms between ending one exposure and starting the next
int runBeforeExposing = 5000;     // amount of time to run the motor before starting the first exposure
int runAfterExposing = 1000;      // amount of time to run the motor after ending the final exposure

int currentExposureDuration = 0;
int currentExposureNumber = 0;

const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<html>
  <head>
  </head>
  <style>
input[type=submit]
{
padding:4px;
font-size:12px;
}
input[type=text]{
padding: 8px;
}
label{
font-size:12px;
}
h2{
font-size:15px;
}
p{
font-size:12px;
}
div#envelope{
width: 80%;
margin: 10px 30% 10px 11%;
}
</style>
  <body>
  <form action="/action_page">
    <div class="row">
      <div class="col-25">
        <label for="loopDelay">Delay</label>
      </div>
      <div class="col-75">
        <input type='text' name='Delay' id='loopDelay'> 
      </div>
    </div>
    <div class="row">
     <div class="col-25">
        <label for="stepSpeed">Speed</label>
      </div>
      <div class="col-75">
       <input type='text' name='Speed' id='stepSpeed'> 
      </div>
     </div>
      <div class="row">
    <br><input type="submit" value="Submit">
      </div>
  </form> 
  </body>
</html>
)=====";

int lastRed = 0;
int lastGreen = 0;
int lastBlue = 0;

void SetLEDColour(int red, int green, int blue) {

  if (red != lastRed || green != lastGreen || blue != lastBlue) {
    analogWrite(redPin, red);
    analogWrite(greenPin, green);
    analogWrite(bluePin, blue);
    lastRed = red;
    lastGreen = green;
    lastBlue = blue;
  }
}

bool blinkRunning = false;
bool blinkOn = false;
unsigned long blinkTimeout = 0;
unsigned long blinkStartMillis;
unsigned long blinkTriggerMillis;


void SetLEDState() {

  int full = MediumFull;
  int part = MediumPart;

  if (ledBrightness == 3) {
    full = HighFull;
    part = HighPart;
  } else if (ledBrightness == 1) {
    full = LowFull;
    part = LowPart;
  }

  switch (state) {
    case poweredOn:
      SetLEDColour(full, 0, 0);
      break;
    case connectingHotspot:
      SetLEDColour(full, 0, full);
      break;
    case connectedHotspot:
      SetLEDColour(full, part, 0);
      break;
    case connectedApp:
      SetLEDColour(0, 0, full);
      break;
    case runningMotor:
      SetLEDColour(0, full, 0);
      break;
    case exposingCamera:
      SetLEDColour(0, full, full);
      break;
    case runningExposing:
      SetLEDColour(part, full, full);
      break;
    case autoCameraControl:
      BlinkLED(part);
      break;
    case unknown:
    default:
       SetLEDColour(0, 0, 0);
  }
}

void BlinkLED(int part)
{
  if (blinkRunning)
  {
    if (blinkOn)
    {
//      Serial.println("      blink high");
      SetLEDColour(0, part, 0);
    }
    else
    {
//      Serial.println("      blink low");
      SetLEDColour(0, 0, 0);
    }
  }
  else
  {
//      Serial.println("      don't blink");
    SetLEDColour(0, part, 0);
  }
}  

void initCameraSchedule(int stg, int shg, int shd, int nums) {
  shotDuration = shd;
  shotGap = shg;
  startGap = stg;
  numShots = nums;
}


void startCameraSchedule() {
  state = autoCameraControl;
  shootInterrupt();
}

bool timerRunning = false;
unsigned long timerTimeout = 0;
unsigned long timerStartMillis;
unsigned long timerTriggerMillis;

void initTimer(int tenths) {
  unsigned long timerTimeout = tenths * 100;
  timerStartMillis = millis();
  timerTriggerMillis = timerStartMillis + timerTimeout;
  timerRunning = true;
}

void checkTimer() {
  if (timerRunning == false) {
    return;  
  }
  unsigned long currentMillis = millis();
  if (currentMillis >= timerTriggerMillis) {
    timerRunning = false;
    shootInterrupt();
  }
}




void initBlinker(int tenths) {
  blinkTimeout = tenths * 100;
  blinkStartMillis = millis();
  blinkTriggerMillis = blinkStartMillis + blinkTimeout;
  blinkRunning = true;
  blinkOn = true;
}

void stopBlinker() {
  blinkRunning = false;
  blinkOn = false;
}

void checkBlinker() {
  if (blinkRunning == false) {
    return;  
  }
  unsigned long currentMillis = millis();
  if (currentMillis >= blinkTriggerMillis) {
    blinkOn = !blinkOn;
    blinkStartMillis = millis();
    blinkTriggerMillis = blinkStartMillis + blinkTimeout;
  }
}

void shootInterrupt() {

  switch (shootingState) {
    case cancelling:
      stopCamera();
      shootingState = inactive;
      stepActive = false;
      break;
    case inactive:
      if (startGap > 0) {
        initBlinker(5);
        initTimer(startGap);
        shootingState = waitStart;
      } else {
        stopBlinker();
        startCamera();
        // and set the interrupt for the duration
        initTimer(shotDuration);
        countShots = countShots + 1;
        shootingState = shooting;
      }
      break;
    case waitStart:
      stopBlinker();
      // the initial wait is over - shoot the camera
      startCamera();
      // and set the interrupt for the duration
      initTimer(shotDuration);
      countShots = countShots + 1;
      shootingState = shooting;
      break;
    case waitStop:
      shootingState = inactive;
      stopBlinker();
      stepActive = false;
      state = connectedApp;
      break;
    case shooting:
      // first thing - turn off the camera
      stopCamera();
      if (countShots == numShots) {
        // we've done them all
        if (startGap > 0) {
          initTimer(startGap);
          initBlinker(5);
          shootingState = waitStop;
        } else {
          stopBlinker();
          shootingState = inactive;
          stepActive = false;
          state = connectedApp;
        }
      } else {
        shootingState = shootGap;
        initBlinker(2);
        initTimer(shotGap);
      }
      
      break;
    case shootGap:
      startCamera();
      stopBlinker();
      // and set the interrupt for the duration
      initTimer(shotDuration);
      countShots = countShots + 1;
      shootingState = shooting;
      break;
    case complete:
      Serial.println("  was complete - doing nothing");
      break;
    default:
       break;
  }
}

void ICACHE_RAM_ATTR enc_read();
void setup(void)
{
  Serial.begin(9600);
  Serial.println('\n');
  Serial.println("Sky Tracker v2.0");
  Serial.println('\n');
  Serial.println('\n');
  Serial.println("Setting pin modes");
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  pinMode(cameraShootPin, OUTPUT);
  pinMode(cameraFocusPin, OUTPUT);
  
  Serial.println("Setting LED state");
  SetLEDState();
  
  Serial.println("Setting stepper values");
  // max speed appears to be 699
  stepper.setMaxSpeed(2000.0);
  stepper.setAcceleration(50.0);
  stepper.setSpeed(0);
  stepper.moveTo(2038);
  
  delay(10);
  WiFi.hostname("SkyTracker");
  WiFi.begin(ssid, password);             // Connect to the network
  Serial.print("Connecting to ");
  Serial.print(ssid); Serial.println(" ...");
  int i = 0;
  int maxAttempts = 30;
  while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect

    if (state == connectingHotspot) {
      state = poweredOn;
    } else {
      state = connectingHotspot;
    }

    SetLEDState();
    delay(500);
    if (state == poweredOn) {
      Serial.print(++i); 
      Serial.print(' ');
    }
  }

  Serial.println('\n');
  Serial.println("Connection established!");  
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer

  state = connectedHotspot;
  SetLEDState();

  server.on("/", handleRootPath); 
  server.on("/init", handleInit); 
  server.on("/deinit", handleDeInit); 
  server.on("/action", handleAction); //form action is handled here
  server.on("/camera/start", handleCameraStart); 
  server.on("/camera/stop", handleCameraStop); 
  server.on("/camera/fire", handleCameraFire); 
  server.on("/camera/auto", handleCameraAuto); 
  server.on("/camera/cancel", handleCameraCancel); 

  server.begin();
  Serial.println ( "HTTP server started" );
}

// when using Bulb mode - this starts the exposure
void startCamera()
{
  digitalWrite(cameraShootPin, HIGH);
}

// when using Bulb mode - this stop the exposure
void stopCamera()
{
  digitalWrite(cameraShootPin, LOW);
}

// for when not using Bulb mode - holds it on for 1/10th second
void shootCamera()
{
  digitalWrite(cameraShootPin, HIGH);
  delay(100);
  digitalWrite(cameraShootPin, LOW);
}

// attempt to focus the camera - no feedback of course
void focusCamera()
{
  digitalWrite(cameraFocusPin, HIGH);
  delay(1000);
  digitalWrite(cameraFocusPin, LOW);
}

void loop()
{
  server.handleClient(); 
  stepper.setSpeed(stepSpeed);
  if (stepSpeed != oldStepSpeed) {
    Serial.print("Motor speed changed from ");
    Serial.print(oldStepSpeed);
    Serial.print(" to ");
    Serial.println(stepSpeed);
    oldStepSpeed = stepSpeed;
  }


  // only run the motor if active flag is set
  if (stepActive == true) {
    stepper.runSpeed();
    Serial.println(stepSpeed);
  }


  if (manualExposure == startExposure) {
    startCamera();
  } else if (manualExposure == stopExposure) {
    stopCamera();
  } else if (manualExposure == shootOnce) {
    shootCamera();
  }
  manualExposure = none;

  checkTimer();
  checkBlinker();

  SetLEDState();
  delay(loopDelay);
}

/*
 * 
 *  API CALLS
 * 
 * 
 */

/*
 *  Route: /
 *  Params: none
 *  Action: returns web page
 */
void handleRootPath() {
  Serial.println ( "Client Refresh" );
  server.send_P(200, "text/html", MAIN_page, sizeof(MAIN_page) ); //Send web page
}

/*
 *  Route: /init
 *  Params: led=n   set LED brightness
 *  Action: sets app state to connected
 */
void handleInit() { // Handler. 192.168.XXX.XXX/init

  Serial.println("Server init received");
  loopDelay = 0;
  state = connectedApp;

  if (server.hasArg("led")) {
    ledBrightness = (server.arg("led")).toInt(); //Converts the string to integer.
    Serial.print("ledBrightness = ");
    Serial.println(ledBrightness);
    SetLEDState();
  }
  
  server.send(200, "text/html", "Initialization done"); // App is expecting a 200
}

/*
 *  Route: /deinit
 *  Params: none
 *  Action: sets app state to disconnected
 */
void handleDeInit() {// Handler. 192.168.XXX.XXX/deinit

  Serial.println("Server deinit received");
  loopDelay = 0;
  state = connectedHotspot;
  stepActive = false;
  server.send(200, "text/html", "Denitialization done"); // App is expecting a 200
}

/*
 *  Route: /action
 *  Params: speed=  sets stepper motor speed  (negative for reverse direction)
 *          led=    sets LED brightenss
 *          active= sets motor on or off
 *  Action: sets app state to disconnected
 */
void handleAction() {// Handler. 192.168.XXX.XXX/Init?Dir=HIGH&Delay=5&Steps=200 (One turn clockwise in one second)

  loopDelay = 0;
  Serial.println("Server action received");
  String message = "Action with: ";

  if (server.hasArg("speed")) {
    stepSpeed = (server.arg("speed")).toInt(); //Converts the string to integer.
    message += " Speed: ";
    message += server.arg("speed");
  }
  
  if (server.hasArg("active")) {
    int stepActiveInt = (server.arg("active")).toInt(); //Converts the string to integer.
    stepActive = (stepActiveInt == 1);
    message += " Active: ";
    message += server.arg("active");
  }

  if (server.hasArg("led")) {
    ledBrightness = (server.arg("led")).toInt(); //Converts the string to integer.
    SetLEDState();
  }

  Serial.print("stepActive = ");
  Serial.println(stepActive);
  Serial.print("stepSpeed = ");
  Serial.println(stepSpeed);
  
  if (stepActive == true) {
    state = runningMotor;
  } else {
    state = connectedApp;
  }
  String s = "<a href='/'> Go Back </a>";
  message += s;
  server.send(200, "text/html", message); //It's better to return something so the browser don't get frustrated+ 
}

/*
 *  Route: /camera/start
 *  Params: none
 *  Action: starts camera exposure (camera must be in B mode)
 */
void handleCameraStart() {

  Serial.println("Camera start received");
  manualExposure = startExposure;
  server.send(200, "text/html", "Camera start"); //It's better to return something so the browser don't get frustrated+ 
}

/*
 *  Route: /camera/stop
 *  Params: none
 *  Action: stops camera exposure (camera must be in B mode)
 */
void handleCameraStop() {

  Serial.println("Camera stop received");
  manualExposure = stopExposure; // setting 2 means that in the loop it will force it to close the camera shoot output
  server.send(200, "text/html", "Camera stop"); //It's better to return something so the browser don't get frustrated+ 
}

/*
 *  Route: /camera/fire
 *  Params: none
 *  Action: fires camera once (when not in B mode)
 */
void handleCameraFire() {

  Serial.println("Camera fire received");
  manualExposure = shootOnce; // setting 2 means that in the loop it will force it to close the camera shoot output
  server.send(200, "text/html", "Camera fire"); //It's better to return something so the browser don't get frustrated+ 
}

/*
 *  Route: /camera/auto
 *  Params: number=     sets numbers of exposures
 *          exposure=   sets duration of exposure (in tenths of a second)
 *          preshoot=   sets time to run motor before and after schedule (in tenths of a second)
 *          gap=        sets the gap between shots (in tenths of a second)
 *  Action: sets camera schedule and starts it running
 */
void handleCameraAuto() {

  bool driveMotor = false;

  if (server.hasArg("number")) {
    numShots = (server.arg("number")).toInt();
    if (numShots < 1) {
      numShots = 1;
    }
  }

  if (server.hasArg("exposure")) {
    shotDuration = (server.arg("exposure")).toInt() + 2; //Converts the string to integer.
  }

  if (server.hasArg("preshoot")) {
    startGap = (server.arg("preshoot")).toInt(); //Converts the string to integer.
    driveMotor = true;
  }

  if (server.hasArg("gap")) {
    shotGap = (server.arg("gap")).toInt(); //Converts the string to integer.
  }

  if (server.hasArg("motor")) {
    int dm = (server.arg("motor")).toInt(); //Converts the string to integer.
    driveMotor = (dm == 1);
  }

  manualExposure = none;
  countShots = 0;
  if (driveMotor == true) {
    stepActive = true;
    runMode = fullyAutomatic;
  } else {
    runMode = automaticExposure;
  }

  state = autoCameraControl;
  SetLEDState();
  startCameraSchedule();

  String message = "<a href='/'> Go Back </a>";
  server.send(200, "text/html", message); //It's better to return something so the browser don't get frustrated+ 
}

/*
 *  Route: /camera/cancel
 *  Params: none
 *  Action: cancels camera schedule and stops it running
 */
void handleCameraCancel() {

  bool driveMotor = false;

  stopCamera();
  shootingState = cancelling;
  state = connectedApp;
  countShots = 0;

  SetLEDState();

  String message = "Cancelled camera schedule";
  server.send(200, "text/html", message); //It's better to return something so the browser don't get frustrated+ 
}

//}

// end of code.
