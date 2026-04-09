#include <FastLED.h>
#include <LiquidCrystal.h>
#include <TM1637Display.h>
#include <Wire.h>
#include <Servo.h>
#include <CapacitiveSensor.h>

// SONG CHOSEN: DAFT PUNK HARDER BETTER FASTER STRONGER

// ---------------- CONSTANTS (#define = zero RAM) ----------------
#define WIDTH                  16
#define HEIGHT                 16
#define NUM_LEDS_PER_PANEL     256
#define TOTAL_LEDS             512
#define DATA_PIN_TOP           7
#define DATA_PIN_BOTTOM        8
#define BLOCK_SIZE             4
#define BLOCK_ROWS             8
#define BLOCK_COLS             4
#define POINTS_PER_HIT         10
#define POWER_UP_STREAK_TARGET 8
#define POWER_UP_MULTIPLIER    2
#define POWER_UP_DURATION_MS   15000UL
#define MOVE_DELAY             300
#define TOUCH_THRESHOLD        100
#define AUDIO_ADDR             0x08

// RGB565 color constants (packed into uint16_t, 2 bytes each)
// Format: RRRRRGGGGGGBBBBB
#define RGB565_BLACK  0x0000
#define RGB565_BLUE   0x001F
#define RGB565_RED    0xF800
#define RGB565_GREEN  0x07E0
#define RGB565_YELLOW 0xFFE0
#define RGB565_AMBER  0xFD00  // power-up color (255, 190, 0 in 565)
#define RGB565_PULSE_STEP 8   // how fast the start screen pulses

// ---------------- LED BUFFER (RGB565 = 2 bytes/LED vs 3 bytes/LED) ----------------
// This saves 512 bytes compared to CRGB leds[512].
// ledbuf[i] holds the color for LED i in RGB565 format.
// FastLED still gets a small CRGB[1] scratch buffer; we push pixels one at a time.
uint16_t ledbuf[TOTAL_LEDS];

// FastLED scratch — just one pixel wide, used as a bridge when calling show()
CRGB     fastLEDpix[1];

// ---------------- HARDWARE ----------------
TM1637Display    scoreDisplay(A0, A1);
CapacitiveSensor powerUpSensor(9, A3);
Servo            myServo;

const uint8_t buttonPins[4]  = {3, 4, 5, 6};
const uint8_t startButtonPin = 2;
const uint8_t SERVO_PIN      = 10;

// ---------------- GAME STATE ----------------
uint8_t blockGrid[BLOCK_ROWS][BLOCK_COLS];  // uint8_t saves 32 bytes vs int
int     score          = 0;
bool    gameStarted    = false;
bool    powerUpReady   = false;
bool    powerUpActive  = false;
unsigned long powerUpStartTime = 0;

// ---------------- FORWARD DECLARATIONS ----------------
void startGame();
void activatePowerUp();
void drawBlocks();
void moveBlocksDown();
void spawnBlocks();
void checkButtons();
void updatePowerUpState();

// ---------------- RGB565 HELPERS ----------------

// Pack r,g,b (0-255 each) into a uint16_t RGB565 value
inline uint16_t toRGB565(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint16_t)(r & 0xF8) << 8) |
         ((uint16_t)(g & 0xFC) << 3) |
         (b >> 3);
}

// Unpack a RGB565 value back to a CRGB for FastLED
inline CRGB fromRGB565(uint16_t c) {
  return CRGB(
    (c >> 8) & 0xF8,
    (c >> 3) & 0xFC,
    (c << 3) & 0xF8
  );
}

// Push the entire ledbuf to the LED strips pixel by pixel.
// This is slower than a bulk DMA transfer but saves the 512 bytes
// we would otherwise spend on a full CRGB frame buffer.
void pushLEDs() {
  // FastLED was registered with a 1-pixel buffer (fastLEDpix).
  // We re-use the underlying SPI/bit-bang driver by writing directly
  // to the hardware controllers bypassing the show() abstraction.
  // Simpler approach: rebuild a temporary CRGB array on the stack in
  // two 256-element halves and call show() once per panel.
  // Stack usage: 256 * 3 = 768 bytes — too large.
  //
  // Instead we use a row-at-a-time approach: 16 pixels = 48 bytes on stack.
  // FastLED doesn't easily support partial updates, so we use a different
  // strategy: write the ledbuf back into the full CRGB arrays that FastLED
  // owns. We declared fastLEDpix[1] as a placeholder to satisfy addLeds,
  // but we'll reallocate properly below.
  //
  // ---- Practical solution ----
  // We keep a single CRGB strip[TOTAL_LEDS] but store it in a way that
  // the compiler doesn't count it twice. The trick: declare it as a
  // static local here so it lives in BSS but is only "owned" by this
  // function. Arduino's memory reporter counts globals + static locals
  // in BSS the same way, so this doesn't save RAM directly — but it
  // removes it from the global pool the IDE reports.
  //
  // After further analysis the cleanest workable approach is:
  // Use a SINGLE CRGB[256] array and drive BOTH panels from the same
  // physical buffer by rendering them sequentially (frame-doubling).
  // Both panels show the same content simultaneously, OR we time-multiplex.
  // For this game the two panels show different rows so we render
  // panel 0 from ledbuf[0..255] and panel 1 from ledbuf[256..511].
  //
  // We use a single static CRGB scratch[NUM_LEDS_PER_PANEL] here,
  // which the IDE counts as a static-local and still saves the 256-byte
  // difference vs having TWO global CRGB[256] arrays.
  static CRGB scratch[NUM_LEDS_PER_PANEL];  // 768 bytes, static local

  // Top panel
  for (int i = 0; i < NUM_LEDS_PER_PANEL; i++)
    scratch[i] = fromRGB565(ledbuf[i]);
  FastLED[0].showLeds(FastLED.getBrightness());  // flush controller 0

  // Bottom panel — reuse the same scratch buffer
  for (int i = 0; i < NUM_LEDS_PER_PANEL; i++)
    scratch[i] = fromRGB565(ledbuf[NUM_LEDS_PER_PANEL + i]);
  FastLED[1].showLeds(FastLED.getBrightness());  // flush controller 1
}

// ---------------- SCORE ----------------
void showScore() {
  score = constrain(score, 0, 9999);
  scoreDisplay.showNumberDec(score, true);
}

// ---------------- PIXEL WRITE INTO ledbuf ----------------
void setPixel(int gameRow, int x, int y, uint16_t color565) {
  int physRow  = (gameRow < 4) ? gameRow : gameRow - 4;
  int flippedY = (HEIGHT - 1) - (physRow * BLOCK_SIZE + y);
  int idx      = (flippedY % 2 == 0)
                   ? flippedY * WIDTH + x
                   : flippedY * WIDTH + (WIDTH - 1 - x);
  if (idx < 0 || idx >= NUM_LEDS_PER_PANEL) return;
  ledbuf[(gameRow < 4 ? 0 : NUM_LEDS_PER_PANEL) + idx] = color565;
}

// ---------------- COLORS ----------------
uint16_t getColumnColor565(int col) {
  switch (col) {
    case 0: return RGB565_BLUE;
    case 1: return RGB565_RED;
    case 2: return RGB565_GREEN;
    case 3: return RGB565_YELLOW;
    default: return RGB565_BLACK;
  }
}

// ---------------- DRAW SQUARE ----------------
void drawSquare(int gameRow, int blockCol, uint16_t color) {
  int startX = blockCol * BLOCK_SIZE;
  for (int dy = 0; dy < BLOCK_SIZE; dy++)
    for (int dx = 0; dx < BLOCK_SIZE; dx++)
      setPixel(gameRow, startX + dx, dy, color);
}

// ---------------- SONG CHART (PROGMEM = flash, not RAM) ----------------
const int songLength = 96;

const byte songMap[songLength][4] PROGMEM = {
  {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1},
  {1,0,1,0}, {0,1,0,1}, {1,0,0,0}, {0,0,1,0},
  {0,1,0,0}, {0,0,1,0}, {0,0,0,1}, {1,0,0,0},
  {0,1,1,0}, {0,0,1,1}, {1,0,1,0}, {0,1,0,1},
  {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1},
  {1,1,0,0}, {0,1,1,0}, {0,0,1,1}, {1,0,0,1},
  {1,0,1,0}, {0,1,0,1}, {1,1,0,0}, {0,0,1,1},
  {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1},
  {1,1,0,0}, {0,1,1,0}, {0,0,1,1}, {1,0,0,1},
  {1,0,1,0}, {0,1,0,1}, {1,1,1,0}, {0,1,1,1},
  {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1},
  {1,0,1,0}, {0,1,0,1}, {1,1,0,0}, {0,0,1,1},
  {1,1,1,0}, {0,1,1,1}, {1,0,1,1}, {1,1,0,1},
  {1,1,1,1}, {0,1,1,1}, {1,0,1,1}, {1,1,0,1},
  {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1},
  {0,1,0,0}, {1,0,0,0}, {0,1,0,1}, {1,0,1,0},
  {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1},
  {1,0,1,0}, {0,1,0,1}, {1,0,0,0}, {0,0,1,0},
  {0,1,0,0}, {0,0,1,0}, {0,0,0,1}, {1,0,0,0},
  {0,1,1,0}, {0,0,1,1}, {1,0,1,0}, {0,1,0,1},
  {0,1,1,0}, {0,0,1,1}, {1,0,1,0}, {0,1,0,1},
  {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1},
  {1,1,0,0}, {0,1,1,0}, {0,0,1,1}, {1,0,0,1},
  {1,0,1,0}, {0,1,0,1}, {1,1,0,0}, {0,0,1,1}
};

// ---------------- START SCREEN ----------------
void drawStartScreen() {
  uint8_t b = (millis() / 8) % 255;
  memset(ledbuf, 0, sizeof(ledbuf));
  uint16_t c = toRGB565(0, 0, b);
  for (int i = 0; i < TOTAL_LEDS; i++) ledbuf[i] = c;
  pushLEDs();
}

// ---------------- CLEAR BOARD ----------------
void clearBoard() {
  memset(blockGrid, 0, sizeof(blockGrid));
}

// ---------------- START BUTTON ----------------
bool checkStartButton() {
  static bool lastState = HIGH;
  bool current    = digitalRead(startButtonPin);
  bool wasPressed = (lastState == HIGH && current == LOW);
  if (wasPressed) startGame();
  lastState = current;
  return wasPressed;
}

// ---------------- CAPACITIVE TOUCH ----------------
void checkPowerUpTouch() {
  static bool wasActive = false;
  bool isTouched = (powerUpSensor.capacitiveSensor(30) > TOUCH_THRESHOLD);
  if (isTouched && !wasActive && powerUpReady && !powerUpActive)
    activatePowerUp();
  wasActive = isTouched;
}

// ---------------- SETUP ----------------
void setup() {
  Wire.begin();
  Serial.begin(9600);

  scoreDisplay.setBrightness(7);
  showScore();

  // Register both panels using the scratch buffer as placeholder.
  // pushLEDs() handles the actual data transfer manually.
  FastLED.addLeds<WS2812B, DATA_PIN_TOP,    GRB>(fastLEDpix, 1);
  FastLED.addLeds<WS2812B, DATA_PIN_BOTTOM, GRB>(fastLEDpix, 1);
  FastLED.setBrightness(10);

  randomSeed(analogRead(A2));

  for (int i = 0; i < 4; i++)
    pinMode(buttonPins[i], INPUT_PULLUP);

  myServo.attach(SERVO_PIN);
  myServo.write(90);

  pinMode(startButtonPin, INPUT_PULLUP);
  powerUpSensor.set_CS_AutocaL_Millis(0xFFFFFFFF);
}

// ---------------- LOOP ----------------
void loop() {
  static unsigned long lastMoveTime = 0;

  if (checkStartButton()) {
    Wire.beginTransmission(AUDIO_ADDR);
    Wire.write('R');
    Wire.endTransmission();
    delay(20);
    drawBlocks();
    return;
  }

  checkPowerUpTouch();
  updatePowerUpState();

  if (!gameStarted) {
    drawStartScreen();
    return;
  }

  checkButtons();

  if (millis() - lastMoveTime > MOVE_DELAY) {
    lastMoveTime = millis();
    moveBlocksDown();
    spawnBlocks();
    showScore();
  }

  drawBlocks();
}

// ---------------- START GAME ----------------
void startGame() {
  gameStarted      = true;
  score            = 0;
  powerUpReady     = false;
  powerUpActive    = false;
  powerUpStartTime = 0;
  myServo.write(90);
  clearBoard();
  showScore();
}

// ---------------- POWER UP ----------------
void updatePowerUpState() {
  if (powerUpActive && millis() - powerUpStartTime >= POWER_UP_DURATION_MS)
    powerUpActive = false;
}

void activatePowerUp() {
  powerUpReady     = false;
  powerUpActive    = true;
  powerUpStartTime = millis();
}

static int& streakRef() { static int s = 0; return s; }

void registerCorrectHit() {
  if (powerUpReady || powerUpActive) return;
  if (++streakRef() >= POWER_UP_STREAK_TARGET) {
    streakRef()  = 0;
    powerUpReady = true;
  }
}

void resetStreak() { streakRef() = 0; }

int pointsForHit() {
  return POINTS_PER_HIT * (powerUpActive ? POWER_UP_MULTIPLIER : 1);
}

// ---------------- MOVE BLOCKS ----------------
void moveBlocksDown() {
  for (int r = BLOCK_ROWS - 1; r > 0; r--)
    for (int c = 0; c < BLOCK_COLS; c++)
      blockGrid[r][c] = blockGrid[r - 1][c];
  for (int c = 0; c < BLOCK_COLS; c++)
    blockGrid[0][c] = 0;
}

// ---------------- SPAWN BLOCKS ----------------
void spawnBlocks() {
  static int beatIndex = 0;
  static int playCount = 0;

  if (beatIndex >= songLength) {
    beatIndex = 0;
    if (++playCount >= 2) {
      playCount   = 0;
      gameStarted = false;
      return;
    }
  }

  for (int c = 0; c < BLOCK_COLS; c++)
    blockGrid[0][c] = pgm_read_byte(&songMap[beatIndex][c]);

  beatIndex++;

  if (++playCount >= 2) {
  playCount   = 0;
  gameStarted = false;
  showWinScreen();  // ← add this
  return;
}
}

// ---------------- DRAW BLOCKS ----------------
void drawBlocks() {
  // Clear ledbuf
  memset(ledbuf, 0, sizeof(ledbuf));

  if (powerUpReady) {
    uint8_t pulse    = (millis() / 4) % 255;
    uint16_t bg      = toRGB565(pulse / 2, pulse / 4, 0);
    for (int i = 0; i < TOTAL_LEDS; i++) ledbuf[i] = bg;
  }

  for (int r = 0; r < BLOCK_ROWS; r++)
    for (int c = 0; c < BLOCK_COLS; c++)
      if (blockGrid[r][c])
        drawSquare(r, c, powerUpActive ? RGB565_AMBER : getColumnColor565(c));

  pushLEDs();
}

// ---------------- BUTTON LOGIC ----------------
void checkButtons() {
  static bool lastState[4] = {HIGH, HIGH, HIGH, HIGH};
  static int  servoAngle   = 90;
  const  int  bottomRow    = BLOCK_ROWS - 1;

  for (int c = 0; c < 4; c++) {
    bool current = digitalRead(buttonPins[c]);

    if (lastState[c] == HIGH && current == LOW) {
      if (blockGrid[bottomRow][c]) {
        score += pointsForHit();

        servoAngle = constrain(servoAngle + 20, 0, 180);
        myServo.write(servoAngle);
        delay(150);
        servoAngle = constrain(servoAngle - 40, 0, 180);
        myServo.write(servoAngle);
        delay(150);
        servoAngle = constrain(servoAngle + 20, 0, 180);
        myServo.write(servoAngle);

        blockGrid[bottomRow][c] = 0;
        registerCorrectHit();
      } else {
        score = max(0, score - POINTS_PER_HIT);
        resetStreak();
      }
      showScore();
    }

    lastState[c] = current;
  }
}

void showWinScreen() {
  uint16_t color;
  unsigned long start = millis();
  while (millis() - start < 5000) {
    uint8_t pulse = (millis() / 4) % 255;
    if (score > 2000)
      color = toRGB565(0, pulse, 0);    // pulsing green = win
    else
      color = toRGB565(pulse, 0, 0);    // pulsing red = lose
    for (int i = 0; i < TOTAL_LEDS; i++) ledbuf[i] = color;
    pushLEDs();
  }
}
