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
#define PANEL_BLOCK_ROWS       4    // block rows per physical panel
#define POINTS_PER_HIT         10
#define POWER_UP_STREAK_TARGET 8
#define POWER_UP_MULTIPLIER    2
#define POWER_UP_DURATION_MS   15000UL
#define MOVE_DELAY             300
#define TOUCH_THRESHOLD        100
#define AUDIO_ADDR             0x08

// RGB565 color constants
#define RGB565_BLACK  0x0000
#define RGB565_BLUE   0x001F
#define RGB565_RED    0xF800
#define RGB565_GREEN  0x07E0
#define RGB565_YELLOW 0xFFE0
#define RGB565_AMBER  0xFD00

// ---------------- LED BUFFER ----------------
uint16_t ledbuf[TOTAL_LEDS];
CRGB     fastLEDpix[1];

// ---------------- HARDWARE ----------------
TM1637Display    scoreDisplay(A0, A1);
CapacitiveSensor powerUpSensor(9, A3);
Servo            myServo;

const uint8_t buttonPins[4]  = {3, 4, 5, 6};
const uint8_t startButtonPin = 2;
const uint8_t SERVO_PIN      = 10;
int servoAngle = 90;

// ---------------- GAME STATE ----------------
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
void showWinScreen();

// ---------------- RGB565 HELPERS ----------------
inline uint16_t toRGB565(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint16_t)(r & 0xF8) << 8) |
         ((uint16_t)(g & 0xFC) << 3) |
         (b >> 3);
}

inline CRGB fromRGB565(uint16_t c) {
  return CRGB(
    (c >> 8) & 0xF8,
    (c >> 3) & 0xFC,
    (c << 3) & 0xF8
  );
}

void pushLEDs() {
  static CRGB scratch[NUM_LEDS_PER_PANEL];

  for (int i = 0; i < NUM_LEDS_PER_PANEL; i++)
    scratch[i] = fromRGB565(ledbuf[i]);
  FastLED[0].showLeds(FastLED.getBrightness());

  for (int i = 0; i < NUM_LEDS_PER_PANEL; i++)
    scratch[i] = fromRGB565(ledbuf[NUM_LEDS_PER_PANEL + i]);
  FastLED[1].showLeds(FastLED.getBrightness());
}

// ---------------- SCORE ----------------
void showScore() {
  score = constrain(score, 0, 9999);
  scoreDisplay.showNumberDec(score, true);
}

// ---------------- PIXEL ROUTING (FIXED) ----------------
//
// Each panel is a 16×16 serpentine matrix. Within a panel:
//   - There are PANEL_BLOCK_ROWS (4) block-rows, each BLOCK_SIZE (4) pixels tall
//   - Block-row 0 should appear at the TOP of the panel (lowest pixel index after flip)
//   - Block-row 3 should appear at the BOTTOM of the panel (highest pixel index after flip)
//
// The panel's LED index 0 is physically at the BOTTOM-LEFT corner on WS2812B
// serpentine panels. We flip so that game block-row 0 → physical top of panel.
//
// physRow:  which block-row within this panel (0 = top of panel, 3 = bottom)
// pixelRow: first pixel row of that block, counting from the physical bottom
//           pixelRow = (PANEL_BLOCK_ROWS - 1 - physRow) * BLOCK_SIZE
//           e.g. physRow 0 (top) → pixelRow 12, physRow 3 (bottom) → pixelRow 0
//
// Within a pixel row, serpentine: even rows left→right, odd rows right→left.

void setPixel(int gameRow, int x, int y, uint16_t color565) {
  // Which panel and which block-row within that panel
  bool     isBottom = (gameRow >= PANEL_BLOCK_ROWS);
  int      physRow  = isBottom ? (gameRow - PANEL_BLOCK_ROWS) : gameRow;

  // Convert block-row + intra-block pixel offset to a pixel row index
  // counting from the physical bottom of the panel (where LED 0 lives).
  // physRow 0 = top of panel  → pixel rows 12–15 (furthest from LED 0)
  // physRow 3 = bottom of panel → pixel rows  0–3  (closest to LED 0)
  int pixelRow = (PANEL_BLOCK_ROWS - 1 - physRow) * BLOCK_SIZE + (BLOCK_SIZE - 1 - y);

  // Serpentine column mapping
  int pixelCol = (pixelRow % 2 == 0) ? x : (WIDTH - 1 - x);

  int idx = pixelRow * WIDTH + pixelCol;
  if (idx < 0 || idx >= NUM_LEDS_PER_PANEL) return;

  ledbuf[(isBottom ? NUM_LEDS_PER_PANEL : 0) + idx] = color565;
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

// ---------------- SONG CHART ----------------
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
  uint8_t b  = (millis() / 8) % 255;
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

  // Sweep servo back to centre
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

  for (int c = 0; c < BLOCK_COLS; c++)
    blockGrid[0][c] = pgm_read_byte(&songMap[beatIndex][c]);

  beatIndex++;
}

// ---------------- DRAW BLOCKS ----------------
void drawBlocks() {
  memset(ledbuf, 0, sizeof(ledbuf));

  if (powerUpReady) {
    uint8_t  pulse = (millis() / 4) % 255;
    uint16_t bg    = toRGB565(pulse / 2, pulse / 4, 0);
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
    uint8_t  pulse = (millis() / 4) % 255;
    uint16_t color = (score > 1500)
                       ? toRGB565(0, pulse, 0)
                       : toRGB565(pulse, 0, 0);

    if (score > 1500) {
      // Celebratory bounce
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

    for (int i = 0; i < TOTAL_LEDS; i++) ledbuf[i] = color;
    pushLEDs();
  }
}
