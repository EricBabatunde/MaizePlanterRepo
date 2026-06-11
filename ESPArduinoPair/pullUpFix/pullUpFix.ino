#define DAT 13
#define CMD 11
#define SEL 10
#define CLK 12

void setup() {
  Serial.begin(115200);
  pinMode(DAT, INPUT_PULLUP); // Use internal pullup
  pinMode(CMD, OUTPUT);
  pinMode(SEL, OUTPUT);
  pinMode(CLK, OUTPUT);
  
  digitalWrite(SEL, HIGH); // Disable controller
  digitalWrite(CLK, HIGH);
}

void loop() {
  // Read Data line continuously
  int val = digitalRead(DAT);
  
  // Print "1" or "0"
  Serial.print(val);
  
  // Every 100 chars, new line
  static int count = 0;
  if(count++ > 100) { Serial.println(); count = 0; }
  
  delay(10);
}