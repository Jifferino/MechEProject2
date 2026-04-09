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
#define TOTAL_LEDS             512   // both panels in one array
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

// ---------------- HARDWARE ----------------
// Single array for both panels — saves 768 bytes vs two separate arrays.
// Indices 0..255   = top panel    (game rows 0-3)
// Indices 256..511 = bottom panel (game rows 4-7)
CRGB leds[TOTAL_LEDS];

TM1637Display scoreDisplay(A0, A1);
CapacitiveSensor powerUpSensor(9, A3);
Servo myServo;

const int buttonPins[4]  = {3, 4, 5, 6};
const int startButtonPin = 2;
const int SERVO_PIN      = 10;

// ---------------- GAME STATE ----------------
// blockGrid uses uint8_t instead of int — saves 32 bytes (8×4 grid, 1 vs 2 bytes per cell)
uint8_t blockGrid[BLOCK_ROWS][BLOCK_COLS];
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

// ---------------- SCORE ----------------
void showScore() {
  score = constrain(score, 0, 9999);
  scoreDisplay.showNumberDec(score, true);
}

// ---------------- PIXEL ROUTING ----------------
// Top panel:    leds[0..255],   game rows 0-3
// Bottom panel: leds[256..511], game rows 4-7
void setPixel(int gameRow, int x, int y, CRGB color) {
  int physRow  = (gameRow < 4) ? gameRow : gameRow - 4;
  int flippedY = (HEIGHT - 1) - (physRow * BLOCK_SIZE + y);
  int idx      = (flippedY % 2 == 0)
                   ? flippedY * WIDTH + x
                   : flippedY * WIDTH + (WIDTH - 1 - x);
  if (idx < 0 || idx >= NUM_LEDS_PER_PANEL) return;
  // Offset into the combined array
  leds[(gameRow < 4 ? 0 : NUM_LEDS_PER_PANEL) + idx] = color;
}

// ---------------- COLORS ----------------
CRGB getColumnColor(int col) {
  switch (col) {
    case 0: return CRGB::Blue;
    case 1: return CRGB::Red;
    case 2: return CRGB::Green;
    case 3: return CRGB::Yellow;
    default: return CRGB::Black;
  }
}

// ---------------- DRAW SQUARE ----------------
void drawSquare(int gameRow, int blockCol, CRGB color) {
  int startX = blockCol * BLOCK_SIZE;
  for (int dy = 0; dy < BLOCK_SIZE; dy++)
    for (int dx = 0; dx < BLOCK_SIZE; dx++)
      setPixel(gameRow, startX + dx, dy, color);
}

// ---------------- SONG CHART (PROGMEM = flash, not RAM) ----------------
const int songLength = 64;

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
  {0,1,0,0}, {1,0,0,0}, {0,1,0,1}, {1,0,1,0}
};

// ---------------- START SCREEN ----------------
void drawStartScreen() {
  uint8_t b    = (millis() / 8) % 255;
  CRGB pulseColor = CRGB(0, 0, b);
  fill_solid(leds, TOTAL_LEDS, pulseColor);
  FastLED.show();
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

  // Both panels registered on their own data pins, but sharing the leds[] array
  // with a 256-LED offset for the bottom panel
  FastLED.addLeds<WS2812B, DATA_PIN_TOP,    GRB>(leds,                    NUM_LEDS_PER_PANEL);
  FastLED.addLeds<WS2812B, DATA_PIN_BOTTOM, GRB>(leds + NUM_LEDS_PER_PANEL, NUM_LEDS_PER_PANEL);
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
    FastLED.show();
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
  FastLED.show();
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
}

// ---------------- DRAW BLOCKS ----------------
void drawBlocks() {
  fill_solid(leds, TOTAL_LEDS, CRGB::Black);

  if (powerUpReady) {
    uint8_t pulse = (millis() / 4) % 255;
    fill_solid(leds, TOTAL_LEDS, CRGB(pulse / 2, pulse / 4, 0));
  }

  const CRGB powerUpColor = CRGB(255, 190, 0);

  for (int r = 0; r < BLOCK_ROWS; r++)
    for (int c = 0; c < BLOCK_COLS; c++)
      if (blockGrid[r][c])
        drawSquare(r, c, powerUpActive ? powerUpColor : getColumnColor(c));
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
