int clkPin = 18;
int dtPin = 19;
int swPin = 23;

int counter = 0;
byte oldAB = 0;

int8_t table[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

bool wasPressed = false;
unsigned long pressStartTime = 0;
int longPressThreshold = 500; // ms

void setup() {
  Serial.begin(115200);
  pinMode(clkPin, INPUT);
  pinMode(dtPin, INPUT);
  pinMode(swPin, INPUT_PULLUP);
}

void loop() {
  oldAB <<= 2;
  oldAB |= (digitalRead(clkPin) << 1) | digitalRead(dtPin);
  int8_t change = table[oldAB & 0x0F];

  if (change != 0) {
    counter += change;
    Serial.println(counter / 2);
  }

  bool isPressed = (digitalRead(swPin) == LOW);

  if (isPressed && !wasPressed) {
    pressStartTime = millis();
  }

  if (!isPressed && wasPressed) {
    unsigned long heldFor = millis() - pressStartTime;
    if (heldFor >= 2000) {
      Serial.println("Very long press");
    } else if (heldFor >= 500) {
      Serial.println("Long press");
    } else {
      Serial.println("Short press");
    }
  }

  wasPressed = isPressed;
}