#define JOY_UP_PIN		12
#define JOY_DOWN_PIN	14		// X
#define JOY_LEFT_PIN	27		// Y
#define JOY_RIGHT_PIN	26		// button
#define JOY_MID_PIN		25

void setup() {
  pinMode(JOY_RIGHT_PIN, INPUT_PULLUP);
  digitalWrite(JOY_RIGHT_PIN, HIGH);
  pinMode(JOY_UP_PIN, OUTPUT);
  digitalWrite(JOY_UP_PIN, HIGH);
  Serial.begin(9600);
}

void loop() {
  Serial.print("Switch:  ");
  Serial.print(digitalRead(JOY_RIGHT_PIN));
  Serial.print("\n");
  Serial.print("X-axis: ");
  Serial.print(analogRead(JOY_DOWN_PIN));
  Serial.print("\n");
  Serial.print("Y-axis: ");
  Serial.println(analogRead(JOY_LEFT_PIN));
  Serial.print("\n\n");
  delay(500);
}
