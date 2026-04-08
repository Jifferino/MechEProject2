#include <FastLED.h>
#include <LiquidCrystal.h>
#include <TM1637Display.h>
#include <Wire.h>
#include <Servo.h>
#include <CapacitiveSensor.h>

//SONG CHOSEN: DAFT PUNK HARDER BETTER FASTER STRONGER
// ---------------- LED MATRIX ----------------
#define WIDTH 16
#define HEIGHT 16
#define NUM_LEDS 256
#define DATA_PIN 7

#define BLOCK_SIZE 4
#define BLOCK_ROWS 4
#define BLOCK_COLS 4

CRGB leds[NUM_LEDS];

// --------I2C---------
const byte AUDIO_ADDR = 0x08;

// -------------------- TM1637 (4-pin) score display --------------------
const uint8_t TM_CLK = A0;
const uint8_t TM_DIO = A1;
TM1637Display scoreDisplay(TM_CLK, TM_DIO);

// ---------------- BUTTONS ----------------
// Blue, Red, Green, Yellow

//Power up Button will be a CTS
CapacitiveSensor powerUpSensor(9, A3);

const long TOUCH_THRESHOLD = 100; //sensitivity
const int buttonPins[4] = {3,4,5,6};
const int startButtonPin = 2;
//const int powerUpButtonPin = A3;

// ---------------- GAME ----------------
int blockGrid[BLOCK_ROWS][BLOCK_COLS];
int score = 0;
int streakCount = 0;

// FIX 1: Declared missing global variable used in moveBlocksDown()
int totalBlocks = 0;

unsigned long lastMoveTime = 0;
int moveDelay = 300;//488;

bool gameStarted = false;

const int POINTS_PER_HIT = 10;
const int POWER_UP_STREAK_TARGET = 8;
const int POWER_UP_MULTIPLIER = 2;
const unsigned long POWER_UP_DURATION_MS = 15000UL;

bool powerUpReady = false;
bool powerUpActive = false;
unsigned long powerUpStartTime = 0;
const CRGB POWER_UP_READY_COLOR = CRGB(255, 190, 0);

void showScore() {
  if (score < 0) score = 0;
  if (score > 9999) score = 9999;
  scoreDisplay.showNumberDec(score, true); // leading zeros
}

// ------------ GRASSHOPPER SERVO --------------
Servo myServo;
const int SERVO_PIN = 10;

int servoAngle = 90;   // starting position

// ---------------- XY MAPPING ----------------
int XY(int x, int y){
  y = HEIGHT - 1 - y;
  if(y % 2 == 0)
    return y * WIDTH + x;
  else
    return y * WIDTH + (WIDTH - 1 - x);
}

// ---------------- COLORS ----------------
CRGB getColumnColor(int col){
  if(col == 0) return CRGB::Blue;
  if(col == 1) return CRGB::Red;
  if(col == 2) return CRGB::Green;
  if(col == 3) return CRGB::Yellow;
  return CRGB::Black;
}

// ---------------- DRAW BLOCK ----------------
void drawSquare(int startX, int startY, CRGB color){
  for(int y = 0; y < BLOCK_SIZE; y++){
    for(int x = 0; x < BLOCK_SIZE; x++){
      leds[XY(startX + x, startY + y)] = color;
    }
  }
}

// ---------------- SONG CHART ----------------
// Lanes: 0=Blue, 1=Red, 2=Green, 3=Yellow

const int songLength = 64;

const byte songMap[songLength][4] = {
  //52 total measures
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
//Repeats earlier patterns
  {1,0,0,0},
  {0,1,0,0},
  {0,0,1,0},
  {0,0,0,1},

  {1,0,1,0},
  {0,1,0,1},
  {1,0,0,0},
  {0,0,1,0},


  {0,1,0,0},
  {0,0,1,0},
  {0,0,0,1},
  {1,0,0,0},

  {0,1,1,0},
  {0,0,1,1},
  {1,0,1,0},
  {0,1,0,1},


  {1,0,0,0},
  {0,1,0,0},
  {0,0,1,0},
  {0,0,0,1},

//26
  {1,1,0,0},
  {0,1,1,0},
  {0,0,1,1},
  {1,0,0,1},

  {1,0,1,0},
  {0,1,0,1},
  {1,1,0,0},
  {0,0,1,1},

  {1,0,0,0},
  {0,1,0,0},
  {0,0,1,0},
  {0,0,0,1},


  {1,1,0,0},
  {0,1,1,0},
  {0,0,1,1},
  {1,0,0,1},

  {1,0,1,0},
  {0,1,0,1},
  {1,1,1,0},
  {0,1,1,1},

//31

  {1,0,0,0},
  {0,1,0,0},
  {0,0,1,0},
  {0,0,0,1},

  {1,0,1,0},
  {0,1,0,1},
  {1,1,0,0},
  {0,0,1,1},

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
//36
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
//41
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
  {1,0,0,1}

  {1,0,0,0},
  {0,1,0,0},
  {0,0,1,0},
  {0,0,0,1},

  {1,0,0,0},
  {1,1,0,0},
  {1,1,1,0},
  {1,1,1,1},
//46
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
//51
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
// FIX 2: Added missing drawStartScreen() implementation
void drawStartScreen(){
  FastLED.clear();
  // Pulse all LEDs in a soft white/blue to signal "waiting to start"
  uint8_t brightness = (millis() / 8) % 255;
  for(int i = 0; i < NUM_LEDS; i++){
    leds[i] = CRGB(0, 0, brightness);
  }
  FastLED.show();
}

// ---------------- CLEAR BOARD ----------------
void clearBoard(){
  for(int r = 0; r < BLOCK_ROWS; r++)
    for(int c = 0; c < BLOCK_COLS; c++)
      blockGrid[r][c] = 0;
}

// POWERUP AND START BUTTONS

bool checkStartButton(){
  static bool lastState = HIGH;
  bool current = digitalRead(startButtonPin);
  bool wasPressed = (lastState == HIGH && current == LOW);

  if(wasPressed){
    startGame();
  }

  lastState = current;
  return wasPressed;
}

void checkPowerUpTouch(){
  static bool lastState = false;
  long sensorValue = powerUpSensor.capacitiveSensor(30);

  bool isTouched = (sensorValue > TOUCH_THRESHOLD);

  if(isTouched && !lastState){
    if(powerUpReady && !powerUpActive){
      activatePowerUp();
    }
  }

  lastState = isTouched;
}

// ---------------- SETUP ----------------
void setup(){
  Wire.begin();
  Serial.begin(9600);
  // TM1637
  scoreDisplay.setBrightness(7);
  showScore();

  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(10);

  randomSeed(analogRead(0));

  for(int i = 0; i < 4; i++)
    pinMode(buttonPins[i], INPUT_PULLUP);

  myServo.attach(SERVO_PIN);
  myServo.write(servoAngle);

  pinMode(startButtonPin, INPUT_PULLUP);
  pinMode(powerUpButtonPin, INPUT_PULLUP);
}

// ---------------- LOOP ----------------
void loop(){

  if(checkStartButton()){
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

  if(!gameStarted){
    drawStartScreen();
    return;
  }

  checkButtons();

  if(millis() - lastMoveTime > moveDelay){
    lastMoveTime = millis();
    moveBlocksDown();
    spawnBlocks();
    showScore();
  }

  drawBlocks();
  FastLED.show();
}

// ---------------- START GAME ----------------
void startGame(){
  gameStarted = true;
  score = 0;
  streakCount = 0;
  totalBlocks = 0;   // FIX 1b: reset totalBlocks on new game
  powerUpReady = false;
  powerUpActive = false;
  powerUpStartTime = 0;
  beatIndex = 0;
  lastMoveTime = millis();
  servoAngle = 90;
  myServo.write(servoAngle);

  clearBoard();
  showScore();
}

// ---------------- POWER UP ----------------

void updatePowerUpState(){
  if(powerUpActive && millis() - powerUpStartTime >= POWER_UP_DURATION_MS){
    powerUpActive = false;
  }
}

void activatePowerUp(){
  powerUpReady = false;
  powerUpActive = true;
  powerUpStartTime = millis();
}

void registerCorrectHit(){
  if(powerUpReady || powerUpActive){
    return;
  }
  streakCount++;
  if(streakCount >= POWER_UP_STREAK_TARGET){
    streakCount = 0;
    powerUpReady = true;
  }
}

void resetStreak(){
  streakCount = 0;
}

int pointsForHit(){
  return POINTS_PER_HIT * (powerUpActive ? POWER_UP_MULTIPLIER : 1);
}

// ---------------- MOVE BLOCKS ----------------
void moveBlocksDown(){
  // Count blocks that reached the bottom without being hit
  for(int c = 0; c < BLOCK_COLS; c++){
    if(blockGrid[BLOCK_ROWS-1][c] == 1)
      totalBlocks++;
  }

  for(int r = BLOCK_ROWS - 1; r > 0; r--){
    for(int c = 0; c < BLOCK_COLS; c++){
      blockGrid[r][c] = blockGrid[r-1][c];
    }
  }

  for(int c = 0; c < BLOCK_COLS; c++)
    blockGrid[0][c] = 0;
}

// ---------------- SPAWN BLOCKS ----------------
// FIX 3: Use a static counter so the song-end detection works across calls
void spawnBlocks(){
  static int playCount = 0;

  if(beatIndex >= songLength){
    playCount++;
    beatIndex = 0;
    if(playCount >= 2){
      // Played through twice — stop spawning new blocks (song ended)
      playCount = 0;
      gameStarted = false;   // end the game; player sees start screen again
      return;
    }
  }

  for(int c = 0; c < BLOCK_COLS; c++){
    blockGrid[0][c] = songMap[beatIndex][c];
  }

  beatIndex++;
}

// ---------------- DRAW BLOCKS ----------------
void drawBlocks(){
  FastLED.clear();

  // If power-up is ready, flash a background hint color
  if(powerUpReady){
    uint8_t pulse = (millis() / 4) % 255;
    for(int i = 0; i < NUM_LEDS; i++){
      leds[i] = CRGB(pulse / 2, pulse / 4, 0); // faint amber glow
    }
  }

  for(int r = 0; r < BLOCK_ROWS; r++){
    for(int c = 0; c < BLOCK_COLS; c++){
      if(blockGrid[r][c] == 1){
        CRGB color = powerUpActive ? POWER_UP_READY_COLOR : getColumnColor(c);
        drawSquare(c * BLOCK_SIZE, r * BLOCK_SIZE, color);
      }
    }
  }
}

// ---------------- BUTTON LOGIC ----------------
// FIX 4: Corrected if/else bracket structure so penalty only fires on wrong press,
//         not on every loop iteration for every button.
void checkButtons(){
  static bool lastState[4] = {HIGH, HIGH, HIGH, HIGH};

  int bottomRow = BLOCK_ROWS - 1;

  for(int c = 0; c < 4; c++){
    bool current = digitalRead(buttonPins[c]);

    if(lastState[c] == HIGH && current == LOW){
      // Button was just pressed
      if(blockGrid[bottomRow][c] == 1){
        // Correct hit — block present in this lane
        score += pointsForHit();
        // FIX 5: Added delays between servo writes so motor has time to move
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
        // Wrong press — no block in this lane
        score -= POINTS_PER_HIT;
        if(score < 0) score = 0;
        resetStreak();
      }

      showScore();
    }

    lastState[c] = current;
  }
}