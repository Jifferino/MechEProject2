#include <FastLED.h>
#include <LiquidCrystal.h>
#include <TM1637Display.h>
#include <Wire.h>
#include <Servo.h>
#include <CapacitiveSensor.h>

//SONG CHOSEN: DAFT PUNK HARDER BETTER FASTER STRONGER

// ---------------- LED MATRIX ----------------
#define WIDTH  16
#define HEIGHT 16
#define NUM_LEDS 256          // LEDs per panel

#define DATA_PIN_TOP    7     // top panel    (rows 0-3  in game space)
#define DATA_PIN_BOTTOM 8     // bottom panel (rows 4-7  in game space)

// Two separate LED arrays — one per physical panel
CRGB ledsTop[NUM_LEDS];
CRGB ledsBottom[NUM_LEDS];

#define BLOCK_SIZE 4
#define BLOCK_ROWS 8          // doubled: 4 rows per panel × 2 panels
#define BLOCK_COLS 4

// --------I2C---------
const byte AUDIO_ADDR = 0x08;

// -------------------- TM1637 (4-pin) score display --------------------
const uint8_t TM_CLK = A0;
const uint8_t TM_DIO = A1;
TM1637Display scoreDisplay(TM_CLK, TM_DIO);

// ---------------- BUTTONS ----------------
// Blue, Red, Green, Yellow
CapacitiveSensor powerUpSensor(9, A3);

const long TOUCH_THRESHOLD = 100;
const int buttonPins[4]  = {3, 4, 5, 6};
const int startButtonPin = 2;

// ---------------- GAME ----------------
int blockGrid[BLOCK_ROWS][BLOCK_COLS];
int score       = 0;
int streakCount = 0;
int totalBlocks = 0;

unsigned long lastMoveTime = 0;
int moveDelay = 300;

bool gameStarted = false;

const int   POINTS_PER_HIT         = 10;
const int   POWER_UP_STREAK_TARGET = 8;
const int   POWER_UP_MULTIPLIER    = 2;
const unsigned long POWER_UP_DURATION_MS = 15000UL;

bool  powerUpReady     = false;
bool  powerUpActive    = false;
unsigned long powerUpStartTime = 0;
const CRGB POWER_UP_READY_COLOR = CRGB(255, 190, 0);

void showScore() {
  if (score < 0)    score = 0;
  if (score > 9999) score = 9999;
  scoreDisplay.showNumberDec(score, true);
}

// ------------ GRASSHOPPER SERVO --------------
Servo myServo;
const int SERVO_PIN = 10;
int servoAngle = 90;

// ---------------- XY MAPPING (dual panel) ----------------
// Game rows 0-3  → top panel    (ledsTop)
// Game rows 4-7  → bottom panel (ledsBottom)
//
// Within each panel the physical row index is:
//   top panel:    physRow = gameRow          (0-3)
//   bottom panel: physRow = gameRow - 4      (0-3)
//
// Each panel is a 16×16 serpentine matrix where row 0 is
// the BOTTOM of the physical panel. We flip so that game
// row 0 (spawn row) appears at the top of the top panel
// and game row 7 (hit row) appears at the bottom of the
// bottom panel.

// Returns an index into ledsTop[] or ledsBottom[].
// The caller decides which array to write based on gameRow.
int panelXY(int x, int physRow) {
  // Flip so row 0 of each panel is the top edge
  int flipped = (HEIGHT / BLOCK_SIZE) - 1 - physRow;   // 0-3 → 3-0
  // Map block-space row/col to pixel-space
  int pixelRow = flipped * BLOCK_SIZE;   // first pixel row of this block row
  // Serpentine: even pixel rows go left→right, odd go right→left
  // (We index per pixel inside drawSquare, so here we just provide
  //  the base index for the top-left pixel of the block.)
  // Actual per-pixel mapping is handled in drawSquare via setPixel().
  return pixelRow * WIDTH + x * BLOCK_SIZE;
}

// Low-level pixel writer — routes to the correct panel array.
void setPixel(int gameRow, int x, int y, CRGB color) {
  // x, y are pixel coordinates within the full 16×32 virtual canvas
  // gameRow tells us which panel
  int flippedY;
  if (gameRow < 4) {
    // Top panel: game rows 0-3
    int physRow = gameRow;                     // 0-3
    flippedY = (HEIGHT - 1) - (physRow * BLOCK_SIZE + y);
    int pixelIndex;
    if (flippedY % 2 == 0)
      pixelIndex = flippedY * WIDTH + x;
    else
      pixelIndex = flippedY * WIDTH + (WIDTH - 1 - x);
    if (pixelIndex >= 0 && pixelIndex < NUM_LEDS)
      ledsTop[pixelIndex] = color;
  } else {
    // Bottom panel: game rows 4-7
    int physRow = gameRow - 4;                 // 0-3
    flippedY = (HEIGHT - 1) - (physRow * BLOCK_SIZE + y);
    int pixelIndex;
    if (flippedY % 2 == 0)
      pixelIndex = flippedY * WIDTH + x;
    else
      pixelIndex = flippedY * WIDTH + (WIDTH - 1 - x);
    if (pixelIndex >= 0 && pixelIndex < NUM_LEDS)
      ledsBottom[pixelIndex] = color;
  }
}

// ---------------- COLORS ----------------
CRGB getColumnColor(int col) {
  if (col == 0) return CRGB::Blue;
  if (col == 1) return CRGB::Red;
  if (col == 2) return CRGB::Green;
  if (col == 3) return CRGB::Yellow;
  return CRGB::Black;
}

// ---------------- DRAW BLOCK ----------------
void drawSquare(int gameRow, int blockCol, CRGB color) {
  int startX = blockCol * BLOCK_SIZE;
  for (int dy = 0; dy < BLOCK_SIZE; dy++) {
    for (int dx = 0; dx < BLOCK_SIZE; dx++) {
      setPixel(gameRow, startX + dx, dy, color);
    }
  }
}

// ---------------- SONG CHART ----------------
// Lanes: 0=Blue, 1=Red, 2=Green, 3=Yellow

const int songLength = 128;

const byte songMap[songLength][4] = {
  // Intro groove
  //1
  {1,0,0,0},
  {0,1,0,0},
  {0,0,1,0},
  {0,0,0,1},

  {1,0,1,0},
  {0,1,0,1},
  {1,0,0,0},
  {0,0,1,0},

  // melody entrance
  {0,1,0,0},
  {0,0,1,0},
  {0,0,0,1},
  {1,0,0,0},

  {0,1,1,0},
  {0,0,1,1},
  {1,0,1,0},
  {0,1,0,1},

  // verse groove
  {1,0,0,0},
  {0,1,0,0},
  {0,0,1,0},
  {0,0,0,1},

  //6
  {1,1,0,0},
  {0,1,1,0},
  {0,0,1,1},
  {1,0,0,1},

  // buildup
  {1,0,1,0},
  {0,1,0,1},
  {1,1,0,0},
  {0,0,1,1},

  {1,0,0,0},
  {0,1,0,0},
  {0,0,1,0},
  {0,0,0,1},

  // chorus rhythm
  {1,1,0,0},
  {0,1,1,0},
  {0,0,1,1},
  {1,0,0,1},

  {1,0,1,0},
  {0,1,0,1},
  {1,1,1,0},
  {0,1,1,1},

  //11
  // repeat groove
  {1,0,0,0},
  {0,1,0,0},
  {0,0,1,0},
  {0,0,0,1},

  {1,0,1,0},
  {0,1,0,1},
  {1,1,0,0},
  {0,0,1,1},

  // ending
  {1,1,1,0},
  {0,1,1,1},
  {1,0,1,1},
  {1,1,0,1},

  {1,1,1,1},
  {0,1,1,1},
  {1,0,1,1},
  {1,1,0,1},

  {1,0,0,0},
  {0,1,0,0},
  {0,0,1,0},
  {0,0,0,1},

  //16
  {0,1,0,0},
  {1,0,0,0},
  {0,1,0,1},
  {1,0,1,0},

  {1,0,0,0},
  {0,1,0,0},
  {0,0,1,0},
  {0,0,0,1},

  {0,0,1,0},
  {0,1,0,0},
  {0,1,1,0},
  {1,1,0,0},

  {0,0,0,1},
  {0,0,1,0},
  {0,1,0,0},
  {1,0,0,0},

  {1,0,0,0},
  {0,1,0,0},
  {0,0,1,0},
  {0,0,0,1},

  //21
  {1,0,1,0},
  {0,1,0,1},
  {1,0,1,0},
  {0,1,0,1},

  {0,1,1,1},
  {0,1,1,0},
  {0,1,0,0},
  {0,0,0,0},

  {1,1,0,0},
  {0,0,1,1},
  {0,1,1,0},
  {1,0,0,1},

  {1,0,0,0},
  {0,1,0,0},
  {0,0,1,0},
  {0,0,0,1},

  {1,0,0,0},
  {1,1,0,0},
  {1,1,1,0},
  {1,1,1,1},

  //26
  {0,0,0,1},
  {0,0,1,1},
  {0,1,1,1},
  {1,1,1,1},

  {1,0,1,0},
  {0,1,0,1},
  {1,0,1,0},
  {0,1,0,1},

  {0,1,1,0},
  {1,1,0,0},
  {1,0,0,1},
  {0,0,1,1},

  {1,0,0,0},
  {1,0,0,0},
  {1,0,0,0},
  {1,0,0,0},

  {0,1,0,0},
  {0,1,0,0},
  {0,1,0,0},
  {0,1,0,0},

  //31
  {0,0,1,0},
  {0,0,1,0},
  {0,0,1,0},
  {0,0,1,0},

  {0,0,0,1},
  {0,0,0,1},
  {0,0,0,1},
  {0,0,0,1}
  //end
};

int beatIndex = 0;

// ---------------- START SCREEN ----------------
void drawStartScreen() {
  // Clear both panels
  fill_solid(ledsTop,    NUM_LEDS, CRGB::Black);
  fill_solid(ledsBottom, NUM_LEDS, CRGB::Black);

  uint8_t brightness = (millis() / 8) % 255;
  CRGB pulseColor = CRGB(0, 0, brightness);
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

// ---------------- CAPACITIVE TOUCH POWER-UP ----------------
void checkPowerUpTouch() {
  static bool wasActive = false;
  long sensorValue = powerUpSensor.capacitiveSensor(30);
  bool isTouched   = (sensorValue > TOUCH_THRESHOLD);

  if (isTouched && !wasActive) {
    if (powerUpReady && !powerUpActive) {
      activatePowerUp();
    }
  }
  wasActive = isTouched;
}

// ---------------- SETUP ----------------
void setup() {
  Wire.begin();
  Serial.begin(9600);

  scoreDisplay.setBrightness(7);
  showScore();

  // Register both panels with FastLED on separate data pins
  FastLED.addLeds<WS2812B, DATA_PIN_TOP,    GRB>(ledsTop,    NUM_LEDS);
  FastLED.addLeds<WS2812B, DATA_PIN_BOTTOM, GRB>(ledsBottom, NUM_LEDS);
  FastLED.setBrightness(10);

  randomSeed(analogRead(0));

  for (int i = 0; i < 4; i++)
    pinMode(buttonPins[i], INPUT_PULLUP);

  myServo.attach(SERVO_PIN);
  myServo.write(servoAngle);

  pinMode(startButtonPin, INPUT_PULLUP);

  powerUpSensor.set_CS_AutocaL_Millis(0xFFFFFFFF);
}

// ---------------- LOOP ----------------
void loop() {
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

  if (millis() - lastMoveTime > moveDelay) {
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
  gameStarted    = true;
  score          = 0;
  streakCount    = 0;
  totalBlocks    = 0;
  powerUpReady   = false;
  powerUpActive  = false;
  powerUpStartTime = 0;
  beatIndex      = 0;
  lastMoveTime   = millis();
  servoAngle     = 90;
  myServo.write(servoAngle);
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

void registerCorrectHit() {
  if (powerUpReady || powerUpActive) return;
  streakCount++;
  if (streakCount >= POWER_UP_STREAK_TARGET) {
    streakCount  = 0;
    powerUpReady = true;
  }
}

void resetStreak() {
  streakCount = 0;
}

int pointsForHit() {
  return POINTS_PER_HIT * (powerUpActive ? POWER_UP_MULTIPLIER : 1);
}

// ---------------- MOVE BLOCKS ----------------
void moveBlocksDown() {
  for (int c = 0; c < BLOCK_COLS; c++) {
    if (blockGrid[BLOCK_ROWS - 1][c] == 1)
      totalBlocks++;
  }

  for (int r = BLOCK_ROWS - 1; r > 0; r--)
    for (int c = 0; c < BLOCK_COLS; c++)
      blockGrid[r][c] = blockGrid[r - 1][c];

  for (int c = 0; c < BLOCK_COLS; c++)
    blockGrid[0][c] = 0;
}

// ---------------- SPAWN BLOCKS ----------------
void spawnBlocks() {
  static int playCount = 0;

  if (beatIndex >= songLength) {
    playCount++;
    beatIndex = 0;
    if (playCount >= 2) {
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
  // Clear both panels
  fill_solid(ledsTop,    NUM_LEDS, CRGB::Black);
  fill_solid(ledsBottom, NUM_LEDS, CRGB::Black);

  // Amber pulse across both panels when power-up is ready
  if (powerUpReady) {
    uint8_t pulse = (millis() / 4) % 255;
    CRGB bg = CRGB(pulse / 2, pulse / 4, 0);
    fill_solid(ledsTop,    NUM_LEDS, bg);
    fill_solid(ledsBottom, NUM_LEDS, bg);
  }

  for (int r = 0; r < BLOCK_ROWS; r++) {
    for (int c = 0; c < BLOCK_COLS; c++) {
      if (blockGrid[r][c] == 1) {
        CRGB color = powerUpActive ? POWER_UP_READY_COLOR : getColumnColor(c);
        drawSquare(r, c, color);
      }
    }
  }
}

// ---------------- BUTTON LOGIC ----------------
void checkButtons() {
  static bool lastState[4] = {HIGH, HIGH, HIGH, HIGH};
  int bottomRow = BLOCK_ROWS - 1;  // row 7 — bottom of lower panel

  for (int c = 0; c < 4; c++) {
    bool current = digitalRead(buttonPins[c]);

    if (lastState[c] == HIGH && current == LOW) {
      if (blockGrid[bottomRow][c] == 1) {
        score += pointsForHit();

        servoAngle += 20;
        servoAngle = constrain(servoAngle, 0, 180);
        myServo.write(servoAngle);
        delay(150);

        servoAngle -= 40;
        servoAngle = constrain(servoAngle, 0, 180);
        myServo.write(servoAngle);
        delay(150);

        servoAngle += 20;
        servoAngle = constrain(servoAngle, 0, 180);
        myServo.write(servoAngle);

        blockGrid[bottomRow][c] = 0;
        registerCorrectHit();

      } else {
        score -= POINTS_PER_HIT;
        if (score < 0) score = 0;
        resetStreak();
      }

      showScore();
    }

    lastState[c] = current;
  }
}
