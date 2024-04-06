// MQTT Settings
#define HOSTNAME                      "heart-monitor"
#define MQTT_SERVER                   "192.168.1.247"
#define STATE_TOPIC                   "home/scale/weight"
#define STATE_RAW_TOPIC               "home/scale/raw"
#define AVAILABILITY_TOPIC            "home/scale/available"
#define TARE_TOPIC                    "home/scale/tare"
#define CAL_TOPIC                     "home/scale/calibrate"
#define mqtt_username                 "brian"
#define mqtt_password                 "Qu3rcu54!HASS"

const char host[] = "http://192.168.1.40";
const char port[] = "82";
const char path[] = "/api/iot/heartdata";

const char serverUrl[] = "http://192.168.1.40:82/api/iot/heartdata";


// HX711 Pins
const int LOADCELL_DOUT_PIN = 2; // Remember these are ESP GPIO pins, they are not the physical pins on the board.
const int LOADCELL_SCK_PIN = 3;
int calibration_factor = -23980; // Defines calibration factor we'll use for calibrating.
//int calibration_factor = -21485; // Defines calibration factor we'll use for calibrating.
