#include <Adafruit_NeoPixel.h>

static const uint8_t ROW_PINS[] = {0, 1, 2, 3, 4, 5};
static const uint8_t COL_PINS[] = {6, 7, 9, 8};
static const uint8_t ENCODER_A_PIN = 11;
static const uint8_t ENCODER_B_PIN = 12;
static const uint8_t ENCODER_BUTTON_PIN = 10;
static const uint8_t LED_PIN = 29;
static const uint16_t LED_COUNT = 21;

static const uint8_t MATRIX_ROWS = sizeof(ROW_PINS) / sizeof(ROW_PINS[0]);
static const uint8_t MATRIX_COLS = sizeof(COL_PINS) / sizeof(COL_PINS[0]);

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

bool keyState[MATRIX_ROWS][MATRIX_COLS] = {};
bool buttonPressed = false;
uint32_t lastButtonChangeMs = 0;
uint8_t encoderPrevState = 0;
int32_t encoderSubSteps = 0;
int32_t encoderDetents = 0;
bool ledAnimationEnabled = true;
uint16_t ledIndex = 0;
uint32_t lastLedFrameMs = 0;

static const int8_t ENCODER_TRANSITIONS[16] = {
  0, -1,  1,  0,
  1,  0,  0, -1,
 -1,  0,  0,  1,
  0,  1, -1,  0
};

void setAllRowsHiZ() {
  for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
    pinMode(ROW_PINS[row], INPUT);
  }
}

void printHeader() {
  Serial.println();
  Serial.println(F("RP2040 Zero 6x4 serial test"));
  Serial.println(F("Matrix: 6x4 ROW2COL"));
  Serial.println(F("Rows: GP0 GP1 GP2 GP3 GP4 GP5"));
  Serial.println(F("Cols: GP6 GP7 GP9 GP8"));
  Serial.println(F("Encoder: A=GP11 B=GP12 Button=GP10"));
  Serial.println(F("WS2812: GP29, 21 LEDs"));
  Serial.println(F("Commands: h=help s=status t=toggle-led-test w=white o=off"));
  Serial.println();
}

void printStatus() {
  Serial.print(F("Button: "));
  Serial.println(buttonPressed ? F("DOWN") : F("UP"));

  Serial.print(F("Encoder detents: "));
  Serial.println(encoderDetents);

  Serial.println(F("Pressed keys:"));
  bool anyPressed = false;
  for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
      if (keyState[row][col]) {
        anyPressed = true;
        Serial.print(F("  R"));
        Serial.print(row);
        Serial.print(F(" C"));
        Serial.println(col);
      }
    }
  }

  if (!anyPressed) {
    Serial.println(F("  none"));
  }
}

void setAllLeds(uint32_t color) {
  for (uint16_t i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void handleSerial() {
  while (Serial.available() > 0) {
    const char command = static_cast<char>(Serial.read());

    switch (command) {
      case 'h':
      case 'H':
        printHeader();
        break;
      case 's':
      case 'S':
        printStatus();
        break;
      case 't':
      case 'T':
        ledAnimationEnabled = !ledAnimationEnabled;
        Serial.print(F("LED test "));
        Serial.println(ledAnimationEnabled ? F("enabled") : F("paused"));
        break;
      case 'w':
      case 'W':
        ledAnimationEnabled = false;
        setAllLeds(strip.Color(32, 32, 32));
        Serial.println(F("LEDs set to white"));
        break;
      case 'o':
      case 'O':
        ledAnimationEnabled = false;
        setAllLeds(0);
        Serial.println(F("LEDs off"));
        break;
      case '\n':
      case '\r':
        break;
      default:
        Serial.print(F("Unknown command: "));
        Serial.println(command);
        break;
    }
  }
}

void scanMatrix() {
  for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
    setAllRowsHiZ();
    pinMode(ROW_PINS[row], OUTPUT);
    digitalWrite(ROW_PINS[row], LOW);
    delayMicroseconds(5);

    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
      const bool pressed = (digitalRead(COL_PINS[col]) == LOW);
      if (pressed != keyState[row][col]) {
        keyState[row][col] = pressed;
        Serial.print(F("Key R"));
        Serial.print(row);
        Serial.print(F(" C"));
        Serial.print(col);
        Serial.print(F(" -> "));
        Serial.println(pressed ? F("DOWN") : F("UP"));
      }
    }
  }

  setAllRowsHiZ();
}

void scanEncoder() {
  const uint8_t a = digitalRead(ENCODER_A_PIN) ? 1 : 0;
  const uint8_t b = digitalRead(ENCODER_B_PIN) ? 1 : 0;
  const uint8_t currentState = (a << 1) | b;

  if (currentState == encoderPrevState) {
    return;
  }

  const uint8_t transition = (encoderPrevState << 2) | currentState;
  encoderPrevState = currentState;
  encoderSubSteps += ENCODER_TRANSITIONS[transition];

  if (encoderSubSteps >= 4) {
    encoderSubSteps = 0;
    encoderDetents++;
    Serial.print(F("Encoder -> CW, detents="));
    Serial.println(encoderDetents);
  } else if (encoderSubSteps <= -4) {
    encoderSubSteps = 0;
    encoderDetents--;
    Serial.print(F("Encoder -> CCW, detents="));
    Serial.println(encoderDetents);
  }
}

void scanButton() {
  const bool rawPressed = (digitalRead(ENCODER_BUTTON_PIN) == LOW);
  const uint32_t now = millis();

  if (rawPressed != buttonPressed && (now - lastButtonChangeMs) > 15) {
    buttonPressed = rawPressed;
    lastButtonChangeMs = now;
    Serial.print(F("Encoder button -> "));
    Serial.println(buttonPressed ? F("DOWN") : F("UP"));
  }
}

void updateLeds() {
  if (!ledAnimationEnabled) {
    return;
  }

  const uint32_t now = millis();
  if ((now - lastLedFrameMs) < 120) {
    return;
  }

  lastLedFrameMs = now;
  strip.clear();
  strip.setPixelColor(ledIndex % LED_COUNT, strip.Color(32, 0, 0));
  strip.setPixelColor((ledIndex + 7) % LED_COUNT, strip.Color(0, 16, 0));
  strip.setPixelColor((ledIndex + 14) % LED_COUNT, strip.Color(0, 0, 16));
  strip.show();

  Serial.print(F("LED frame -> index "));
  Serial.println(ledIndex % LED_COUNT);
  ledIndex++;
}

void setup() {
  Serial.begin(115200);
  const uint32_t start = millis();
  while (!Serial && (millis() - start) < 4000) {
  }

  for (uint8_t col = 0; col < MATRIX_COLS; col++) {
    pinMode(COL_PINS[col], INPUT_PULLUP);
  }
  setAllRowsHiZ();

  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);
  pinMode(ENCODER_BUTTON_PIN, INPUT_PULLUP);
  encoderPrevState = (digitalRead(ENCODER_A_PIN) ? 2 : 0) | (digitalRead(ENCODER_B_PIN) ? 1 : 0);

  strip.begin();
  strip.clear();
  strip.show();

  printHeader();
  printStatus();
}

void loop() {
  handleSerial();
  scanMatrix();
  scanEncoder();
  scanButton();
  updateLeds();
  delay(2);
}
