#include <FastLED.h>
#include <LiquidCrystal.h>
#include <TM1637Display.h>
#include <Wire.h>
#include <Servo.h>
#include <CapacitiveSensor.h>

// SONG CHOSEN: DAFT PUNK HARDER BETTER FASTER STRONGER

// ---------------- CONSTANTS (no RAM cost) ----------------
#define WIDTH              16
#define HEIGHT             16
#define NUM_LEDS           256
#define DATA_PIN_TOP       7
#define DATA_PIN_BOTTOM    8
#define BLOCK_SIZE         4
#define BLOCK_ROWS         8
#define BLOCK_COLS         4

#define POINTS_PER_HIT          10
#define POWER_UP_STREAK_TARGET  8
#define POWER_UP_MULTIPLIER     2
#define POWER_UP_DURATION_MS    15000UL
#define MOVE_DELAY              300
#define TOUCH_THRESHOLD         100

// --------I2C---------
const byte AUDIO_ADDR = 0x08;

// -------------------- TM1637 --------------------
const uint8_t TM_CLK = A0;
const uint8_t TM_DIO = A1;
TM1637Display scoreDisplay(TM_CLK, TM_DIO);

// ---------------- HARDWARE ----------------
CRGB ledsTop[NUM_LEDS];
CRGB ledsBottom[NUM_LEDS];

CapacitiveSensor powerUpSensor(9, A3);

Servo myServo;
const int SERVO_PIN      = 10;
const int buttonPins[4]  = {3, 4, 5, 6};
const int startButtonPin = 2;

// ---------------- GAME STATE (true globals — shared across functions) ----------------
int  blockGrid[BLOCK_ROWS][BLOCK_COLS];
int  score        = 0;
bool gameStarted  = false;
bool powerUpReady  = false;
bool powerUpActive = false;
unsigned long powerUpStartTime = 0;

// ---------------- SCORE DISPLAY ----------------
void showScore() {
  if (score < 0)    score = 0;
  if (score > 9999) score = 9999;
  scoreDisplay.showNumberDec(score, true);
}

// ---------------- XY / PIXEL ROUTING ----------------
void setPixel(int gameRow, int x, int y, CRGB color) {
  int physRow  = (gameRow < 4) ? gameRow : gameRow - 4;
  int flippedY = (HEIGHT - 1) - (physRow * BLOCK_SIZE + y);
  int idx      = (flippedY % 2 == 0)
                   ? flippedY * WIDTH + x
                   : flippedY * WIDTH + (WIDTH - 1 - x);

  if (idx < 0 || idx >= NUM_LEDS) return;

  if (gameRow < 4)
    ledsTop[idx]    = color;
  else
    ledsBottom[idx] = color;
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

// ---------------- DRAW BLOCK ----------------
void drawSquare(int gameRow, int blockCol, CRGB color) {
  int startX = blockCol * BLOCK_SIZE;
  for (int dy = 0; dy < BLOCK_SIZE; dy++)
    for (int dx = 0; dx < BLOCK_SIZE; dx++)
      setPixel(gameRow, startX + dx, dy, color);
}

// ---------------- SONG CHART ----------------
const int songLength = 64;

const byte songMap[songLength][4] = {
  // Intro groove
  {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1},
  {1,0,1,0}, {0,1,0,1}, {1,0,0,0}, {0,0,1,0},
  // melody entrance
  {0,1,0,0}, {0,0,1,0}, {0,0,0,1}, {1,0,0,0},
  {0,1,1,0}, {0,0,1,1}, {1,0,1,0}, {0,1,0,1},
  // verse groove
  {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1},
  {1,1,0,0}, {0,1,1,0}, {0,0,1,1}, {1,0,0,1},
  // buildup
  {1,0,1,0}, {0,1,0,1}, {1,1,0,0}, {0,0,1,1},
  {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1},
  // chorus rhythm
  {1,1,0,0}, {0,1,1,0}, {0,0,1,1}, {1,0,0,1},
  {1,0,1,0}, {0,1,0,1}, {1,1,1,0}, {0,1,1,1},
  // repeat groove
  {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1},
  {1,0,1,0}, {0,1,0,1}, {1,1,0,0}, {0,0,1,1},
  // ending
  {1,1,1,0}, {0,1,1,1}, {1,0,1,1}, {1,1,0,1},
  {1,1,1,1}, {0,1,1,1}, {1,0,1,1}, {1,1,0,1},
  {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1},
  {0,1,0,0}, {1,0,0,0}, {0,1,0,1}, {1,0,1,0},
 
};

// ---------------- START SCREEN ----------------
void drawStartScreen() {
  uint8_t brightness = (millis() / 8) % 255;
  CRGB pulseColor    = CRGB(0, 0, brightness);
  fill_solid(ledsTop,    NUM_LEDS, pulseColor);
  fill_solid(ledsBottom, NUM_LEDS, pulseColor);
  FastLED.show();
}

// ---------------- CLEAR BOARD ----------------
void clearBoard() {
  for (int r = 0; r < BLOCK_ROWS; r++)
    for (int c = 0; c < BLOCK_COLS; c++)
      blockGrid[r][c] = 0;
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

  FastLED.addLeds<WS2812B, DATA_PIN_TOP,    GRB>(ledsTop,    NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN_BOTTOM, GRB>(ledsBottom, NUM_LEDS);
  FastLED.setBrightness(10);

  randomSeed(analogRead(0));

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

// Streak state is only needed by registerCorrectHit() and resetStreak(),
// so it lives as a static local shared via a small helper.
static int& streakRef() {
  static int streak = 0;
  return streak;
}

void registerCorrectHit() {
  if (powerUpReady || powerUpActive) return;
  if (++streakRef() >= POWER_UP_STREAK_TARGET) {
    streakRef()  = 0;
    powerUpReady = true;
  }
}

void resetStreak() {
  streakRef() = 0;
}

int pointsForHit() {
  return POINTS_PER_HIT * (powerUpActive ? POWER_UP_MULTIPLIER : 1);
}

// ---------------- MOVE BLOCKS ----------------
void moveBlocksDown() {
  static int totalBlocks = 0;   // only meaningful inside this function

  for (int c = 0; c < BLOCK_COLS; c++)
    if (blockGrid[BLOCK_ROWS - 1][c] == 1)
      totalBlocks++;

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
    blockGrid[0][c] = songMap[beatIndex][c];

  beatIndex++;
}

// ---------------- DRAW BLOCKS ----------------
void drawBlocks() {
  fill_solid(ledsTop,    NUM_LEDS, CRGB::Black);
  fill_solid(ledsBottom, NUM_LEDS, CRGB::Black);

  if (powerUpReady) {
    uint8_t pulse = (millis() / 4) % 255;
    CRGB bg = CRGB(pulse / 2, pulse / 4, 0);
    fill_solid(ledsTop,    NUM_LEDS, bg);
    fill_solid(ledsBottom, NUM_LEDS, bg);
  }

  const CRGB powerUpColor = CRGB(255, 190, 0);   // local constant — only used here

  for (int r = 0; r < BLOCK_ROWS; r++)
    for (int c = 0; c < BLOCK_COLS; c++)
      if (blockGrid[r][c] == 1)
        drawSquare(r, c, powerUpActive ? powerUpColor : getColumnColor(c));
}

// ---------------- BUTTON LOGIC ----------------
void checkButtons() {
  static bool lastState[4]  = {HIGH, HIGH, HIGH, HIGH};
  static int  servoAngle    = 90;   // persists between calls, only needed here
  const  int  bottomRow     = BLOCK_ROWS - 1;

  for (int c = 0; c < 4; c++) {
    bool current = digitalRead(buttonPins[c]);

    if (lastState[c] == HIGH && current == LOW) {
      if (blockGrid[bottomRow][c] == 1) {
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
