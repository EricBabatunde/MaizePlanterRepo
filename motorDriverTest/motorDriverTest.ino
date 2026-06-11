// BTS7960 Motor Driver Test with ESP32
// Motor connected to B+/B- on BTS7960

// Pin definitions
#define L_PWM 15
#define R_PWM 16
#define L_EN  17
#define R_EN  18

void setup() {
  Serial.begin(115200);

  pinMode(L_PWM, OUTPUT);
  pinMode(R_PWM, OUTPUT);
  pinMode(L_EN, OUTPUT);
  pinMode(R_EN, OUTPUT);

  // Enable both channels
  digitalWrite(L_EN, HIGH);
  digitalWrite(R_EN, HIGH);

  Serial.println("BTS7960 Motor Driver Test Start");
}

void loop() {
  Serial.println("Motor Forward...");
  analogWrite(L_PWM, 180); // speed (0-255)
  analogWrite(R_PWM, 0);
  delay(2000);

  Serial.println("Stop...");
  analogWrite(L_PWM, 0);
  analogWrite(R_PWM, 0);
  delay(1000);

  Serial.println("Motor Backward...");
  analogWrite(L_PWM, 0);
  analogWrite(R_PWM, 180);
  delay(2000);

  Serial.println("Stop...");
  analogWrite(L_PWM, 0);
  analogWrite(R_PWM, 0);
  delay(1000);
}
