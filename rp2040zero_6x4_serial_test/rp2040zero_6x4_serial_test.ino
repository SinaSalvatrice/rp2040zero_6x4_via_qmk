#include <Adafruit_NeoPixel.h>
#include <Keyboard.h>
#include <Mouse.h>
#include <RP2040Support.h>

static const uint8_t ROW_PINS[] = {0, 1, 2, 3, 4, 5};
static const uint8_t COL_PINS[] = {6, 7, 9, 8};
static const uint8_t ENCODER_A_PIN = 11;
static const uint8_t ENCODER_B_PIN = 12;
static const uint8_t ENCODER_BUTTON_PIN = 10;
static const uint8_t LED_PIN = 29;
static const uint16_t LED_COUNT = 21;

static const uint8_t MATRIX_ROWS = sizeof(ROW_PINS) / sizeof(ROW_PINS[0]);
static const uint8_t MATRIX_COLS = sizeof(COL_PINS) / sizeof(COL_PINS[0]);

static const uint32_t DEBOUNCE_MS = 12;
static const uint32_t FRAME_MS = 20;
static const uint32_t DOT_HOLD_MS = 250;
static const uint32_t IND_HOLD_MS = 280;
static const uint32_t BUTTON_DEBOUNCE_MS = 15;
static const uint8_t DOT_STEP_PER_TICK = 1;

static const uint8_t INDICATOR_LED_FOR_LAYER[] = {0, 2, 7, 9, 4};
static const int8_t KEY_LED_MAP[MATRIX_ROWS][MATRIX_COLS] = {
  { 0,  1,  2,  3},
  { 4,  5,  6,  7},
  { 8,  9, 10, 11},
  {12, 13, 14, 15},
  {16, 17, 18, 19},
  {-1, 20, -1, -1}
};

enum Layer : uint8_t {
  LAYER_NUMPAD = 0,
  LAYER_EDIT,
  LAYER_NAV,
  LAYER_MACRO,
  LAYER_SETTINGS,
  LAYER_COUNT
};

enum ActionType : uint8_t {
  ACTION_NONE = 0,
  ACTION_KEY,
  ACTION_MOD_KEY,
  ACTION_LAYER_MO,
  ACTION_LAYER_TO,
  ACTION_RGB_TOGGLE,
  ACTION_RGB_MODE_TOGGLE,
  ACTION_RGB_HUE_UP,
  ACTION_RGB_HUE_DOWN,
  ACTION_RGB_SAT_UP,
  ACTION_RGB_SAT_DOWN,
  ACTION_RGB_BRIGHT_UP,
  ACTION_RGB_BRIGHT_DOWN,
  ACTION_RGB_SPEED_UP,
  ACTION_RGB_SPEED_DOWN,
  ACTION_BOOTLOADER
};

struct KeyAction {
  ActionType type;
  uint16_t code;
  uint8_t mod1;
  uint8_t mod2;
};

#define ACTION_NONE_DEF           {ACTION_NONE, 0, 0, 0}
#define ACTION_KEY_DEF(code)      {ACTION_KEY, static_cast<uint16_t>(code), 0, 0}
#define ACTION_MOD_DEF(code,m1,m2){ACTION_MOD_KEY, static_cast<uint16_t>(code), static_cast<uint8_t>(m1), static_cast<uint8_t>(m2)}
#define ACTION_MO_DEF(layer)      {ACTION_LAYER_MO, static_cast<uint16_t>(layer), 0, 0}
#define ACTION_TO_DEF(layer)      {ACTION_LAYER_TO, static_cast<uint16_t>(layer), 0, 0}
#define ACTION_RGB_DEF(kind)      {kind, 0, 0, 0}

static const KeyAction KEYMAPS[LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS] = {
  {
    {ACTION_NONE_DEF,               ACTION_MO_DEF(LAYER_EDIT),      ACTION_MO_DEF(LAYER_SETTINGS), ACTION_KEY_DEF(KEY_BACKSPACE)},
    {ACTION_KEY_DEF(KEY_NUM_LOCK),  ACTION_KEY_DEF(KEY_KP_ASTERISK), ACTION_KEY_DEF(KEY_KP_SLASH),  ACTION_KEY_DEF(KEY_KP_MINUS)},
    {ACTION_KEY_DEF(KEY_KP_7),      ACTION_KEY_DEF(KEY_KP_8),        ACTION_KEY_DEF(KEY_KP_9),      ACTION_KEY_DEF(KEY_KP_PLUS)},
    {ACTION_KEY_DEF(KEY_KP_4),      ACTION_KEY_DEF(KEY_KP_5),        ACTION_KEY_DEF(KEY_KP_6),      ACTION_NONE_DEF},
    {ACTION_KEY_DEF(KEY_KP_1),      ACTION_KEY_DEF(KEY_KP_2),        ACTION_KEY_DEF(KEY_KP_3),      ACTION_KEY_DEF(KEY_KP_ENTER)},
    {ACTION_NONE_DEF,               ACTION_KEY_DEF(KEY_KP_0),        ACTION_KEY_DEF(KEY_KP_DOT),    ACTION_NONE_DEF}
  },
  {
    {ACTION_NONE_DEF,                       ACTION_TO_DEF(LAYER_NUMPAD),       ACTION_MO_DEF(LAYER_SETTINGS), ACTION_KEY_DEF(KEY_BACKSPACE)},
    {ACTION_NONE_DEF,                       ACTION_NONE_DEF,                    ACTION_MOD_DEF('v', KEY_LEFT_CTRL, 0), ACTION_MOD_DEF('a', KEY_LEFT_CTRL, 0)},
    {ACTION_MOD_DEF('z', KEY_LEFT_CTRL, 0), ACTION_MOD_DEF(KEY_HOME, KEY_LEFT_SHIFT, 0), ACTION_MOD_DEF('r', KEY_LEFT_CTRL, 0), ACTION_MOD_DEF('c', KEY_LEFT_CTRL, 0)},
    {ACTION_MOD_DEF(KEY_LEFT_ARROW, KEY_LEFT_SHIFT, 0), ACTION_MOD_DEF('s', KEY_LEFT_CTRL, 0), ACTION_MOD_DEF(KEY_RIGHT_ARROW, KEY_LEFT_SHIFT, 0), ACTION_NONE_DEF},
    {ACTION_MOD_DEF(KEY_LEFT_ARROW, KEY_LEFT_CTRL, KEY_LEFT_SHIFT), ACTION_MOD_DEF(KEY_END, KEY_LEFT_SHIFT, 0), ACTION_MOD_DEF(KEY_RIGHT_ARROW, KEY_LEFT_CTRL, KEY_LEFT_SHIFT), ACTION_KEY_DEF(KEY_KP_ENTER)},
    {ACTION_NONE_DEF,                       ACTION_KEY_DEF(' '),                ACTION_MOD_DEF('x', KEY_LEFT_CTRL, 0), ACTION_NONE_DEF}
  },
  {
    {ACTION_NONE_DEF, ACTION_TO_DEF(LAYER_NUMPAD), ACTION_MO_DEF(LAYER_SETTINGS), ACTION_NONE_DEF},
    {ACTION_NONE_DEF, ACTION_NONE_DEF, ACTION_NONE_DEF, ACTION_NONE_DEF},
    {ACTION_NONE_DEF, ACTION_NONE_DEF, ACTION_NONE_DEF, ACTION_NONE_DEF},
    {ACTION_MOD_DEF(KEY_LEFT_ARROW, KEY_LEFT_ALT, KEY_LEFT_CTRL), ACTION_NONE_DEF, ACTION_MOD_DEF(KEY_RIGHT_ARROW, KEY_LEFT_ALT, KEY_LEFT_CTRL), ACTION_NONE_DEF},
    {ACTION_MOD_DEF(KEY_LEFT_ARROW, KEY_LEFT_CTRL, KEY_LEFT_GUI), ACTION_NONE_DEF, ACTION_MOD_DEF(KEY_RIGHT_ARROW, KEY_LEFT_CTRL, KEY_LEFT_GUI), ACTION_NONE_DEF},
    {ACTION_NONE_DEF, ACTION_NONE_DEF, ACTION_MOD_DEF(KEY_DELETE, KEY_LEFT_CTRL, KEY_LEFT_ALT), ACTION_NONE_DEF}
  },
  {
    {ACTION_NONE_DEF, ACTION_TO_DEF(LAYER_NUMPAD), ACTION_MO_DEF(LAYER_SETTINGS), ACTION_NONE_DEF},
    {ACTION_NONE_DEF, ACTION_NONE_DEF, ACTION_NONE_DEF, ACTION_NONE_DEF},
    {ACTION_KEY_DEF(KEY_F14), ACTION_KEY_DEF(KEY_F15), ACTION_KEY_DEF(KEY_F16), ACTION_NONE_DEF},
    {ACTION_KEY_DEF(KEY_F17), ACTION_KEY_DEF(KEY_F18), ACTION_KEY_DEF(KEY_F19), ACTION_NONE_DEF},
    {ACTION_KEY_DEF(KEY_F20), ACTION_KEY_DEF(KEY_F21), ACTION_KEY_DEF(KEY_F22), ACTION_NONE_DEF},
    {ACTION_NONE_DEF, ACTION_NONE_DEF, ACTION_NONE_DEF, ACTION_NONE_DEF}
  },
  {
    {ACTION_NONE_DEF, ACTION_TO_DEF(LAYER_NUMPAD), ACTION_MO_DEF(LAYER_SETTINGS), ACTION_BOOTLOADER},
    {ACTION_RGB_DEF(ACTION_RGB_SPEED_UP), ACTION_RGB_DEF(ACTION_RGB_SPEED_DOWN), ACTION_RGB_DEF(ACTION_RGB_HUE_UP), ACTION_RGB_DEF(ACTION_RGB_HUE_DOWN)},
    {ACTION_RGB_DEF(ACTION_RGB_BRIGHT_UP), ACTION_RGB_DEF(ACTION_RGB_BRIGHT_DOWN), ACTION_RGB_DEF(ACTION_RGB_MODE_TOGGLE), ACTION_RGB_DEF(ACTION_RGB_TOGGLE)},
    {ACTION_RGB_DEF(ACTION_RGB_SAT_UP), ACTION_RGB_DEF(ACTION_RGB_SAT_DOWN), ACTION_NONE_DEF, ACTION_NONE_DEF},
    {ACTION_TO_DEF(LAYER_EDIT), ACTION_TO_DEF(LAYER_NAV), ACTION_TO_DEF(LAYER_MACRO), ACTION_NONE_DEF},
    {ACTION_NONE_DEF, ACTION_NONE_DEF, ACTION_NONE_DEF, ACTION_NONE_DEF}
  }
};

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

bool matrixStableState[MATRIX_ROWS][MATRIX_COLS] = {};
bool matrixRawState[MATRIX_ROWS][MATRIX_COLS] = {};
uint32_t matrixDebounceMs[MATRIX_ROWS][MATRIX_COLS] = {};
KeyAction activeActions[MATRIX_ROWS][MATRIX_COLS] = {};
uint32_t keyFlashUntil[LED_COUNT] = {};

bool encoderButtonPressed = false;
uint32_t lastButtonChangeMs = 0;
uint8_t encoderPrevState = 0;
int32_t encoderSubSteps = 0;
int32_t encoderDetents = 0;
uint8_t modifierRefCounts[8] = {};

uint8_t baseLayer = LAYER_NUMPAD;
uint8_t heldLayerMask = 0;
uint8_t currentLayer = LAYER_NUMPAD;

uint8_t baseVMax = 25;
uint8_t baseVMin = 1;
uint16_t wanderStepMs = 120;
uint8_t currentSat = 255;
uint8_t currentHue = 149;
uint8_t rgbMode = 0;
bool userRgbOn = true;
uint8_t encDotPos = 0;
uint8_t wanderPos = 0;
uint32_t lastFrameMs = 0;
uint32_t lastTurnMs = 0;
uint32_t wanderMs = 0;
uint32_t indicatorMs = 0;
bool indicatorActive = true;

static const int8_t ENCODER_TRANSITIONS[16] = {
  0, -1,  1,  0,
  1,  0,  0, -1,
 -1,  0,  0,  1,
  0,  1, -1,  0
};

const __FlashStringHelper *layerName(uint8_t layer) {
  switch (layer) {
    case LAYER_NUMPAD: return F("NUMPAD");
    case LAYER_EDIT: return F("EDIT");
    case LAYER_NAV: return F("NAV");
    case LAYER_MACRO: return F("MACRO");
    case LAYER_SETTINGS: return F("SETTINGS");
    default: return F("UNKNOWN");
  }
}

uint8_t hueForLayer(uint8_t layer) {
  switch (layer) {
    case LAYER_NUMPAD: return 149;
    case LAYER_EDIT: return 64;
    case LAYER_NAV: return 170;
    case LAYER_MACRO: return 213;
    case LAYER_SETTINGS: return 0;
    default: return 149;
  }
}

void setAllRowsHiZ() {
  for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
    pinMode(ROW_PINS[row], INPUT);
  }
}

uint8_t getModifierIndex(uint8_t key) {
  if (key < KEY_LEFT_CTRL || key > KEY_RIGHT_GUI) {
    return 255;
  }
  return key - KEY_LEFT_CTRL;
}

void pressModifier(uint8_t key) {
  if (key == 0) {
    return;
  }
  const uint8_t index = getModifierIndex(key);
  if (index == 255) {
    Keyboard.press(key);
    return;
  }
  if (modifierRefCounts[index] == 0) {
    Keyboard.press(key);
  }
  modifierRefCounts[index]++;
}

void releaseModifier(uint8_t key) {
  if (key == 0) {
    return;
  }
  const uint8_t index = getModifierIndex(key);
  if (index == 255) {
    Keyboard.release(key);
    return;
  }
  if (modifierRefCounts[index] > 0) {
    modifierRefCounts[index]--;
    if (modifierRefCounts[index] == 0) {
      Keyboard.release(key);
    }
  }
}

uint8_t getActiveLayer() {
  uint8_t layer = baseLayer;
  for (uint8_t candidate = 0; candidate < LAYER_COUNT; candidate++) {
    if (heldLayerMask & (1u << candidate)) {
      layer = max(layer, candidate);
    }
  }
  return layer;
}

void refreshLayerState(bool announce) {
  const uint8_t nextLayer = getActiveLayer();
  currentHue = hueForLayer(nextLayer);
  if (nextLayer != currentLayer) {
    currentLayer = nextLayer;
    indicatorActive = true;
    indicatorMs = millis();
    if (announce) {
      Serial.print(F("Layer -> "));
      Serial.println(layerName(currentLayer));
    }
  }
}

uint8_t wave8(uint32_t phase) {
  const float radians = (phase & 0xFF) * (6.2831853f / 255.0f);
  const float normalized = (sinf(radians) + 1.0f) * 0.5f;
  return static_cast<uint8_t>(normalized * 255.0f);
}

uint8_t ditherScaleSin8(uint32_t phase, uint8_t vmax) {
  return static_cast<uint8_t>((static_cast<uint16_t>(wave8(phase)) * vmax) >> 8);
}

uint32_t colorFromHSV(uint8_t h, uint8_t s, uint8_t v) {
  return strip.gamma32(strip.ColorHSV(static_cast<uint16_t>(h) * 257u, s, v));
}

void setLedHSV(uint16_t idx, uint8_t h, uint8_t s, uint8_t v) {
  if (idx >= LED_COUNT) {
    return;
  }
  strip.setPixelColor(idx, colorFromHSV(h, s, v));
}

void setLedColor(uint16_t idx, uint8_t r, uint8_t g, uint8_t b) {
  if (idx >= LED_COUNT) {
    return;
  }
  strip.setPixelColor(idx, strip.Color(r, g, b));
}

void flashKeyLed(uint8_t row, uint8_t col) {
  const int8_t led = KEY_LED_MAP[row][col];
  if (led >= 0) {
    keyFlashUntil[led] = millis() + 140;
  }
}

void renderFrame() {
  strip.clear();
  if (!userRgbOn) {
    strip.show();
    return;
  }

  const uint32_t now = millis();
  uint8_t baseV = baseVMin;
  if (baseVMax > baseVMin) {
    baseV = ditherScaleSin8(now / 14, baseVMax - baseVMin) + baseVMin;
  }

  if (rgbMode == 1 || rgbMode == 2) {
    for (uint16_t i = 0; i < LED_COUNT; i++) {
      setLedHSV(i, currentHue, currentSat, baseV);
    }
  }

  if (rgbMode != 2) {
    const uint8_t mainV = ditherScaleSin8(now / 10, 55);
    const uint8_t trailV = ditherScaleSin8(now / 12, 24);
    const uint8_t wp = (wanderPos >= LED_COUNT) ? 0 : wanderPos;
    const uint8_t left = (wp == 0) ? (LED_COUNT - 1) : (wp - 1);
    const uint8_t right = (wp + 1) % LED_COUNT;

    if (currentLayer == LAYER_NUMPAD) {
      const uint8_t rainbowBase = (now / 8) & 0xFF;
      const uint8_t step = static_cast<uint8_t>(256 / LED_COUNT);
      setLedHSV(left, rainbowBase + (left * step), 255, trailV);
      setLedHSV(wp, rainbowBase + (wp * step), 255, mainV);
      setLedHSV(right, rainbowBase + (right * step), 255, trailV);
    } else {
      setLedHSV(left, currentHue, currentSat, trailV);
      setLedHSV(wp, currentHue, currentSat, mainV);
      setLedHSV(right, currentHue, currentSat, trailV);
    }
  }

  if (indicatorActive && (now - indicatorMs) < IND_HOLD_MS) {
    const uint8_t led = INDICATOR_LED_FOR_LAYER[currentLayer];
    setLedHSV(led, currentHue, currentSat, 255);
  } else {
    indicatorActive = false;
  }

  if ((now - lastTurnMs) < DOT_HOLD_MS) {
    const uint8_t dotV = ditherScaleSin8(now / 6, 80);
    setLedHSV(encDotPos % LED_COUNT, currentHue, currentSat, dotV);
  }

  for (uint16_t i = 0; i < LED_COUNT; i++) {
    if (keyFlashUntil[i] > now) {
      setLedColor(i, 255, 255, 255);
    }
  }

  strip.show();
}

void printHeader() {
  Serial.println();
  Serial.println(F("RP2040 Zero 6x4 HID numpad"));
  Serial.println(F("Layers: 0=numpad 1=edit 2=nav 3=macro 4=settings"));
  Serial.println(F("Commands: h=help s=status 0..4=set-base-layer t=rgb-toggle m=rgb-mode b=bootsel o=led-off"));
  Serial.println();
}

void printStatus() {
  Serial.print(F("Base layer: "));
  Serial.println(layerName(baseLayer));
  Serial.print(F("Active layer: "));
  Serial.println(layerName(currentLayer));
  Serial.print(F("Encoder detents: "));
  Serial.println(encoderDetents);
  Serial.print(F("RGB enabled: "));
  Serial.println(userRgbOn ? F("yes") : F("no"));
  Serial.print(F("RGB mode: "));
  Serial.println(rgbMode);
}

void handleRgbAction(ActionType type) {
  switch (type) {
    case ACTION_RGB_TOGGLE:
      userRgbOn = !userRgbOn;
      Serial.print(F("RGB -> "));
      Serial.println(userRgbOn ? F("on") : F("off"));
      break;
    case ACTION_RGB_MODE_TOGGLE:
      rgbMode = (rgbMode + 1) % 3;
      Serial.print(F("RGB mode -> "));
      Serial.println(rgbMode);
      break;
    case ACTION_RGB_HUE_UP:
      currentHue += 8;
      Serial.println(F("RGB hue++"));
      break;
    case ACTION_RGB_HUE_DOWN:
      currentHue -= 8;
      Serial.println(F("RGB hue--"));
      break;
    case ACTION_RGB_SAT_UP:
      currentSat = (currentSat <= 247) ? currentSat + 8 : 255;
      Serial.println(F("RGB saturation++"));
      break;
    case ACTION_RGB_SAT_DOWN:
      currentSat = (currentSat >= 8) ? currentSat - 8 : 0;
      Serial.println(F("RGB saturation--"));
      break;
    case ACTION_RGB_BRIGHT_UP:
      if (baseVMax < 100) {
        baseVMax += 2;
      }
      Serial.println(F("RGB brightness++"));
      break;
    case ACTION_RGB_BRIGHT_DOWN:
      if (baseVMax > 2) {
        baseVMax -= 2;
      }
      Serial.println(F("RGB brightness--"));
      break;
    case ACTION_RGB_SPEED_UP:
      if (wanderStepMs > 20) {
        wanderStepMs -= 10;
      }
      Serial.println(F("RGB speed++"));
      break;
    case ACTION_RGB_SPEED_DOWN:
      if (wanderStepMs < 1000) {
        wanderStepMs += 10;
      }
      Serial.println(F("RGB speed--"));
      break;
    default:
      break;
  }
  indicatorActive = true;
  indicatorMs = millis();
}

void applyActionPress(const KeyAction &action, uint8_t row, uint8_t col) {
  flashKeyLed(row, col);

  switch (action.type) {
    case ACTION_NONE:
      break;
    case ACTION_KEY:
      Keyboard.press(static_cast<uint8_t>(action.code));
      break;
    case ACTION_MOD_KEY:
      pressModifier(action.mod1);
      pressModifier(action.mod2);
      Keyboard.press(static_cast<uint8_t>(action.code));
      break;
    case ACTION_LAYER_MO:
      heldLayerMask |= (1u << action.code);
      refreshLayerState(true);
      break;
    case ACTION_LAYER_TO:
      baseLayer = static_cast<uint8_t>(action.code);
      refreshLayerState(true);
      break;
    case ACTION_RGB_TOGGLE:
    case ACTION_RGB_MODE_TOGGLE:
    case ACTION_RGB_HUE_UP:
    case ACTION_RGB_HUE_DOWN:
    case ACTION_RGB_SAT_UP:
    case ACTION_RGB_SAT_DOWN:
    case ACTION_RGB_BRIGHT_UP:
    case ACTION_RGB_BRIGHT_DOWN:
    case ACTION_RGB_SPEED_UP:
    case ACTION_RGB_SPEED_DOWN:
      handleRgbAction(action.type);
      break;
    case ACTION_BOOTLOADER:
      Serial.println(F("Rebooting to bootloader..."));
      delay(25);
      rp2040.rebootToBootloader();
      break;
  }
}

void applyActionRelease(const KeyAction &action) {
  switch (action.type) {
    case ACTION_KEY:
      Keyboard.release(static_cast<uint8_t>(action.code));
      break;
    case ACTION_MOD_KEY:
      Keyboard.release(static_cast<uint8_t>(action.code));
      releaseModifier(action.mod2);
      releaseModifier(action.mod1);
      break;
    case ACTION_LAYER_MO:
      heldLayerMask &= ~(1u << action.code);
      refreshLayerState(true);
      break;
    default:
      break;
  }
}

void handleKeyEvent(uint8_t row, uint8_t col, bool pressed) {
  if (pressed) {
    const KeyAction action = KEYMAPS[getActiveLayer()][row][col];
    activeActions[row][col] = action;
    Serial.print(F("Key R"));
    Serial.print(row);
    Serial.print(F(" C"));
    Serial.print(col);
    Serial.print(F(" -> DOWN on "));
    Serial.println(layerName(getActiveLayer()));
    applyActionPress(action, row, col);
  } else {
    Serial.print(F("Key R"));
    Serial.print(row);
    Serial.print(F(" C"));
    Serial.println(F(" -> UP"));
    applyActionRelease(activeActions[row][col]);
    activeActions[row][col] = ACTION_NONE_DEF;
  }
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
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
        baseLayer = static_cast<uint8_t>(command - '0');
        refreshLayerState(true);
        break;
      case 't':
      case 'T':
        handleRgbAction(ACTION_RGB_TOGGLE);
        break;
      case 'm':
      case 'M':
        handleRgbAction(ACTION_RGB_MODE_TOGGLE);
        break;
      case 'b':
      case 'B':
        Serial.println(F("Serial command -> bootloader"));
        delay(25);
        rp2040.rebootToBootloader();
        break;
      case 'o':
      case 'O':
        userRgbOn = false;
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
  const uint32_t now = millis();

  for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
    setAllRowsHiZ();
    pinMode(ROW_PINS[row], OUTPUT);
    digitalWrite(ROW_PINS[row], LOW);
    delayMicroseconds(5);

    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
      const bool pressed = (digitalRead(COL_PINS[col]) == LOW);
      if (pressed != matrixRawState[row][col]) {
        matrixRawState[row][col] = pressed;
        matrixDebounceMs[row][col] = now;
      }

      if ((now - matrixDebounceMs[row][col]) >= DEBOUNCE_MS && matrixStableState[row][col] != matrixRawState[row][col]) {
        matrixStableState[row][col] = matrixRawState[row][col];
        handleKeyEvent(row, col, matrixStableState[row][col]);
      }
    }
  }

  setAllRowsHiZ();
}

void handleEncoderTick(bool clockwise) {
  encDotPos = (encDotPos + (clockwise ? DOT_STEP_PER_TICK : LED_COUNT - (DOT_STEP_PER_TICK % LED_COUNT))) % LED_COUNT;
  lastTurnMs = millis();
  encoderDetents += clockwise ? 1 : -1;
  Mouse.move(0, 0, clockwise ? 1 : -1);
  Serial.print(F("Encoder -> "));
  Serial.print(clockwise ? F("CW") : F("CCW"));
  Serial.print(F(", detents="));
  Serial.println(encoderDetents);
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
    handleEncoderTick(true);
  } else if (encoderSubSteps <= -4) {
    encoderSubSteps = 0;
    handleEncoderTick(false);
  }
}

void scanButton() {
  const bool rawPressed = (digitalRead(ENCODER_BUTTON_PIN) == LOW);
  const uint32_t now = millis();

  if (rawPressed != encoderButtonPressed && (now - lastButtonChangeMs) >= BUTTON_DEBOUNCE_MS) {
    encoderButtonPressed = rawPressed;
    lastButtonChangeMs = now;
    Serial.print(F("Encoder button -> "));
    Serial.println(encoderButtonPressed ? F("DOWN") : F("UP"));

    if (encoderButtonPressed) {
      userRgbOn = !userRgbOn;
      indicatorActive = true;
      indicatorMs = now;
    }
  }
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

  Keyboard.begin();
  Mouse.begin();

  currentLayer = getActiveLayer();
  currentHue = hueForLayer(currentLayer);
  wanderMs = millis();
  lastFrameMs = millis();
  indicatorMs = millis();

  printHeader();
  printStatus();
}

void loop() {
  handleSerial();
  scanMatrix();
  scanEncoder();
  scanButton();

  const uint32_t now = millis();
  if ((now - wanderMs) >= wanderStepMs) {
    wanderMs = now;
    wanderPos = (wanderPos + 1) % LED_COUNT;
  }

  if ((now - lastFrameMs) >= FRAME_MS) {
    lastFrameMs = now;
    renderFrame();
  }

  delay(2);
}
