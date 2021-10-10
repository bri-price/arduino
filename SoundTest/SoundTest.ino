
int	digitalPin = D1; //define switch port
int	analogPin = A0;     // select the input  pin for  the potentiometer 
int	sensorValue = 0;  // variable to  store  the value  coming  from  the sensor
int calibrationValue = 500;
bool calibrated = false;

void  setup()
{
	pinMode(digitalPin,INPUT);//define switch as a output port
//	pinMode(analogPin,INPUT);//define switch as a output port
	Serial.begin(9600);
}

int counter = 0;

void  loop()
{
	int digitalVal = digitalRead(digitalPin);//read the value of the digital interface 3 assigned to val 
	int analogVal = analogRead(analogPin);
	counter++;
	if (calibrated == true) {
		if (counter > 2000) {
			Serial.print("Analog = ");
			Serial.println(analogVal);
			counter = 0;
		}
		if (analogVal < calibrationValue)
		{
			Serial.print("Analog = ");
			Serial.print(analogVal);
			Serial.println("  Shoot!");
			delay(3000);
		}
		if (digitalVal == HIGH)
		{
			Serial.print("Digital = HIGH");
			Serial.println("  Shoot!");
			delay(3000);
		}
	} else {
		if (counter == 40) {
			Serial.print("Calibrating from ");
			Serial.println(analogVal);
			calibrationValue = analogVal;
		}
		if (counter > 40) {
			calibrationValue = ((analogVal * 3) + calibrationValue) / 4;
			Serial.println(calibrationValue);
		}
		if (counter > 100) {
			calibrationValue = calibrationValue - 5;
			Serial.print("Calibrated to ");
			Serial.println(calibrationValue);
			calibrated = true;
		}
		delay(100);
	}
}
