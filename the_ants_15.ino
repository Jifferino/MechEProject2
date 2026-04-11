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
#define DATA_PIN_TOP           7
#define DATA_PIN_BOTTOM        8
#define BLOCK_SIZE             4
#define BLOCK_ROWS             8
#define BLOCK_COLS             4
#define PANEL_BLOCK_ROWS       4
#define POINTS_PER_HIT         10
#define POWER_UP_STREAK_TARGET 8
#define POWER_UP_MULTIPLIER    2
#define POWER_UP_DURATION_MS   15000UL
#define MOVE_DELAY             488
#define TOUCH_THRESHOLD        100
#define AUDIO_ADDR             0x08

// ---------------- SINGLE LED SCRATCH BUFFER ----------------
// Only ONE CRGB[256] buffer exists. Both panels render into it
// sequentially and are transmitted one at a time.
// 256 * 3 = 768 bytes — the only large RAM allocation.
CRGB scratch[NUM_LEDS_PER_PANEL];

// ---------------- HARDWARE ----------------
TM1637Display    scoreDisplay(A0, A1);
CapacitiveSensor powerUpSensor(9, A3);
Servo            myServo;

const uint8_t buttonPins[4]  = {3, 4, 5, 6};
const uint8_t startButtonPin = 2;
const uint8_t SERVO_PIN      = 10;
int           servoAngle     = 90;

// ---------------- GAME STATE ----------------
// blockGrid stores column color index (1-4) rather than just 0/1.
// 0 = empty, 1 = blue, 2 = red, 3 = green, 4 = yellow.
// uint8_t = 1 byte per cell, 8*4 = 32 bytes total.
uint8_t blockGrid[BLOCK_ROWS][BLOCK_COLS];
int     score          = 0;
bool    gameStarted    = false;
bool    powerUpReady   = false;
bool    powerUpActive  = false;
unsigned long powerUpStartTime = 0;

// ---------------- FORWARD DECLARATIONS ----------------
void startGame();
void activatePowerUp();
void renderPanel(int panelIndex);
void drawBlocks();
void moveBlocksDown();
void spawnBlocks();
void checkButtons();
void updatePowerUpState();
void showWinScreen();

// ---------------- SCORE ----------------
void showScore() {
  score = constrain(score, 0, 9999);
  scoreDisplay.showNumberDec(score, true);
}

// ---------------- COLOR LOOKUP ----------------
// Returns the CRGB for a given blockGrid cell value.
// Called per-pixel during renderPanel() — no buffer needed.
CRGB cellColor(uint8_t cell) {
  if (powerUpActive) return CRGB(255, 190, 0);   // amber override
  switch (cell) {
    case 1: return CRGB::Blue;
    case 2: return CRGB::Red;
    case 3: return CRGB::Green;
    case 4: return CRGB::Yellow;
    default: return CRGB::Black;
  }
}

// ---------------- PANEL RENDERER ----------------
// Renders one panel's worth of game rows into scratch[], then transmits.
// panelIndex 0 = top panel  (game rows 0-3)
// panelIndex 1 = bottom panel (game rows 4-7)
void renderPanel(int panelIndex) {
  int rowOffset = panelIndex * PANEL_BLOCK_ROWS;

  // Compute pulse background if power-up is ready
  CRGB bg = CRGB::Black;
  if (powerUpReady) {
    uint8_t pulse = (millis() / 4) % 255;
    bg = CRGB(pulse / 2, pulse / 4, 0);
  }

  // Fill scratch with background first
  fill_solid(scratch, NUM_LEDS_PER_PANEL, bg);

  // Draw each block row belonging to this panel
  for (int br = 0; br < PANEL_BLOCK_ROWS; br++) {
    int gameRow = rowOffset + br;

    // physRow: 0 = top of panel, PANEL_BLOCK_ROWS-1 = bottom
    // pixelRow counts from physical bottom (where LED 0 is)
    // so physRow 0 (top) → highest pixelRow values
    for (int bc = 0; bc < BLOCK_COLS; bc++) {
      uint8_t cell = blockGrid[gameRow][bc];
      if (!cell) continue;

      CRGB color = cellColor(cell);
      int startX = bc * BLOCK_SIZE;

      for (int dy = 0; dy < BLOCK_SIZE; dy++) {
        // Flip: physRow 0 → pixelRow 12..15, physRow 3 → pixelRow 0..3
        int pixelRow = (PANEL_BLOCK_ROWS - 1 - br) * BLOCK_SIZE + (BLOCK_SIZE - 1 - dy);
        for (int dx = 0; dx < BLOCK_SIZE; dx++) {
          int x        = startX + dx;
          int pixelCol = (pixelRow % 2 == 0) ? x : (WIDTH - 1 - x);
          int idx      = pixelRow * WIDTH + pixelCol;
          if (idx >= 0 && idx < NUM_LEDS_PER_PANEL)
            scratch[idx] = color;
        }
      }
    }
  }

  // Transmit this panel
  FastLED[panelIndex].showLeds(FastLED.getBrightness());
}

// ---------------- SONG CHART (PROGMEM = flash only) ----------------
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
  fill_solid(scratch, NUM_LEDS_PER_PANEL, CRGB(0, 0, b));
  FastLED[0].showLeds(FastLED.getBrightness());
  FastLED[1].showLeds(FastLED.getBrightness());
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

  FastLED.addLeds<WS2812B, DATA_PIN_TOP,    GRB>(scratch, NUM_LEDS_PER_PANEL);
  FastLED.addLeds<WS2812B, DATA_PIN_BOTTOM, GRB>(scratch, NUM_LEDS_PER_PANEL);
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

  while (servoAngle != 90) {
    servoAngle += (servoAngle < 90) ? 1 : -1;
    myServo.write(servoAngle);
    delay(15);
  }

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
// songMap stores 0/1 per lane. We map lane index → color code (1-4).
void spawnBlocks() {
  static int beatIndex = 0;
  static int playCount = 0;

  if (beatIndex >= songLength) {
    beatIndex = 0;
    if (++playCount >= 2) {
      playCount   = 0;
      gameStarted = false;
      showWinScreen();
      return;
    }
  }

  for (int c = 0; c < BLOCK_COLS; c++) {
    // Store lane+1 as color code so 0 stays "empty"
    blockGrid[0][c] = pgm_read_byte(&songMap[beatIndex][c]) ? (c + 1) : 0;
  }

  beatIndex++;
}

// ---------------- DRAW BLOCKS ----------------
void drawBlocks() {
  renderPanel(0);   // top panel: game rows 0-3
  renderPanel(1);   // bottom panel: game rows 4-7
}

// ---------------- BUTTON LOGIC ----------------
void checkButtons() {
  static bool lastState[4] = {HIGH, HIGH, HIGH, HIGH};
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

// ---------------- WIN / LOSE SCREEN ----------------
void showWinScreen() {
  Wire.beginTransmission(AUDIO_ADDR);
  Wire.write(score > 1500 ? 'W' : 'L');
  Wire.endTransmission();

  unsigned long start = millis();
  while (millis() - start < 5000) {
    uint8_t pulse = (millis() / 4) % 255;
    CRGB color    = (score > 1500)
                      ? CRGB(0, pulse, 0)
                      : CRGB(pulse, 0, 0);

    fill_solid(scratch, NUM_LEDS_PER_PANEL, color);
    FastLED[0].showLeds(FastLED.getBrightness());
    FastLED[1].showLeds(FastLED.getBrightness());

    if (score > 1500) {
      servoAngle = constrain(servoAngle + 20, 0, 180);
      myServo.write(servoAngle);
      delay(300);
      servoAngle = constrain(servoAngle - 40, 0, 180);
      myServo.write(servoAngle);
      delay(300);
      servoAngle = constrain(servoAngle + 20, 0, 180);
      myServo.write(servoAngle);
    } else {
      servoAngle = constrain(servoAngle + 30, 0, 180);
      myServo.write(servoAngle);
      delay(600);
    }
  }
}
