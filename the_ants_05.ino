#include <FastLED.h>
#include <TM1637Display.h>
#include <CapacitiveSensor.h>
#include <Servo.h>

// ---------------- LED MATRIX ----------------
#define WIDTH 16
#define HEIGHT 16
#define NUM_LEDS 256
#define DATA_PIN 7

#define BLOCK_SIZE 4
#define BLOCK_ROWS 4
#define BLOCK_COLS 4

CRGB leds[NUM_LEDS];

// ---------------- DISPLAY ----------------
const uint8_t TM_CLK = A0;
const uint8_t TM_DIO = A1;
TM1637Display scoreDisplay(TM_CLK, TM_DIO);

// ---------------- BUTTONS ----------------
//const int buttonPins[4] = {5,4,3,6};
CapacitiveSensor touchSensors[4] = {
  CapacitiveSensor(10, 5),
  CapacitiveSensor(10, 4),
  CapacitiveSensor(10, 3),
  CapacitiveSensor(10, 6)
};
const long TOUCH_THRESHOLD = 100; //sensitivity
const int startButtonPin = 2;
const int powerUpButtonPin = A3;

// ---------------- GAME ----------------
int blockGrid[BLOCK_ROWS][BLOCK_COLS];
int score = 0;
int streakCount = 0;

unsigned long lastMoveTime = 0;
int moveDelay = 350;

bool gameStarted = false;

const int POINTS_PER_HIT = 10;
const int POWER_UP_STREAK_TARGET = 8;
const int POWER_UP_MULTIPLIER = 2;
const unsigned long POWER_UP_DURATION_MS = 15000UL;

bool powerUpReady = false;
bool powerUpActive = false;
unsigned long powerUpStartTime = 0;
const CRGB POWER_UP_READY_COLOR = CRGB(255, 190, 0);

// ------------ GRASSHOPPER SERVO --------------
Servo myServo;
const int SERVO_PIN = 9;

int servoAngle = 90;   // starting position


// -------- SPEAKER DEFINITIONS --------
#define SPEAKER_PIN 8
#define SPEAKER_PIN2 12

// --- Note Frequency Definitions (Hz) ---
#define NOTE_A2 110
#define NOTE_BF2 116.541
#define NOTE_B2 123.471
#define NOTE_C3 130.813
#define NOTE_DF3 138.591
#define NOTE_D3 146.832
#define NOTE_EF3 155.563
#define NOTE_E3 164.814
#define NOTE_F3 174.614
#define NOTE_FS3 185
#define NOTE_G3 195.998
#define NOTE_GS3 208
#define NOTE_A3  220
#define NOTE_BF3 233
#define NOTE_B3  247
#define NOTE_C4 261.6
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_EF4 311.127
#define NOTE_E4  330
#define NOTE_F4 349.23
#define NOTE_FS4 370
#define NOTE_G4 392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_BF4 466
#define NOTE_B4  494
#define NOTE_C5 523.251
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_EF5 622.254
#define NOTE_E5  659
#define NOTE_F5 698.5
#define NOTE_FS5 740
#define NOTE_G5 784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_BF5 932
#define NOTE_B5  987
#define NOTE_C6 1046.5
#define NOTE_D6 1174.66
#define NOTE_EF6 1244.51
#define NOTE_F6 1396.91
#define NOTE_G6 1567.98
#define NOTE_A6 1760.00
#define NOTE_BF6 1864.66
#define NOTE_B6 1975.53
#define R     0

// ============================================================
//  Tempo
// ============================================================
const int BPM = 120;
const int WHOLE_NOTE_MS = (60000 / BPM) * 4;

// ============================================================
//  🎵 Harder, Better, Faster, Stronger 
//
//
//  Duration guide:
//    1  = whole, 2 = half, 4 = quarter, 8 = eighth, 16 = sixteenth
//   -4  = dotted quarter (1.5x), -8 = dotted eighth
// ============================================================

const int melody[][2] = {

  // Melody, speaker1

  { R,  8 },
  { R,  8 },
  { NOTE_BF4,   8 },
  { NOTE_G4,  8 },
  { NOTE_D5,   8 },
  { NOTE_C5, 8 },
  { NOTE_BF4, 8},
  { NOTE_G4, 8 },

  { R,  8 },
  { R,  8 },
  { NOTE_BF4,   8 },
  { NOTE_F4,  8 },
  { NOTE_C5,   8 },
  { NOTE_BF4, 8 },
  { NOTE_A4, 8},
  { NOTE_BF4, 8 },

  { R,  8 },
  { R,  8 },
  { NOTE_G5,   8 },
  { NOTE_E5,  8 },
  { NOTE_C6,   8 },
  { NOTE_BF5, 8 },
  { NOTE_G5, 8},
  { NOTE_E5, 8 },
  
  { NOTE_EF5,   8 },
  { NOTE_EF5,  8},
  { NOTE_EF4, 8 },
  { NOTE_EF4, 8},
  { NOTE_EF3, 8},
  { NOTE_EF3, 8},
  { NOTE_D3, 8},
  { NOTE_D3, 8},


  { R,         8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },
  { NOTE_D5,   8 },
  { NOTE_C5,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },

  { R,         8 },
  { NOTE_F4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_F4,   8 },
  { NOTE_C5,   8 },
  { NOTE_BF4,  8 },
  { NOTE_A4,   8 },
  { NOTE_BF4,  8 },

  { R,         8 },
  { NOTE_E3,   8 },
  { NOTE_G3,   8 },
  { NOTE_E3,   8 },
  { NOTE_C4,   8 },
  { NOTE_BF3,  8 },
  { NOTE_G3,   8 },
  { NOTE_E3,   8 },

  { NOTE_E3,   8 },
  { NOTE_G3,   8 },
  { NOTE_BF3,  8 },
  { NOTE_G3,   8 },
  { NOTE_BF3,  8 },
  { NOTE_G3,   8 },
  { NOTE_BF3,  8 },
  { NOTE_G3,   8 },

  { R,         8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },
  { NOTE_D5,   8 },
  { NOTE_C5,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },

  { R,         8 },
  { NOTE_F4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_F4,   8 },
  { NOTE_C5,   8 },
  { NOTE_BF4,  8 },
  { NOTE_A4,   8 },
  { NOTE_BF4,  8 },

  { R,         8 },
  { NOTE_E5,   8 },
  { NOTE_G5,   8 },
  { NOTE_E5,   8 },
  { NOTE_C6,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },

  { NOTE_E5,   8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },

  { NOTE_F4,   8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },
  { NOTE_D5,   8 },
  { NOTE_C5,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },

  { NOTE_F4,   8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },
  { NOTE_C5,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },

  { NOTE_E4,   8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },
  { NOTE_C5,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },

  { NOTE_E4,   8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },

  { NOTE_F5,   8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_D6,   8 },
  { NOTE_C6,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },

  { NOTE_F5,   8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_C6,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },

  { NOTE_E5,   8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_C6,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },

  { NOTE_E5,   8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },

  { NOTE_D6,   8 },
  { NOTE_C6,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_F5,   8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },

  { NOTE_C5,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_EF4,  8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },

  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },
  { NOTE_BF5,  8 },
  { NOTE_D6,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  
  { NOTE_F4,   8 },
  { NOTE_F5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_F5,   8 },
  { NOTE_C6,   8 },
  { NOTE_BF5,  8 },
  { NOTE_A5,   8 },
  { NOTE_BF5,  8 },

  { NOTE_A6,   8 },
  { NOTE_A6,   8 },
  { NOTE_F6,   8 },
  { NOTE_F6,   8 },
  { NOTE_BF5,  8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_G5,   8 },

  { NOTE_A6,   8 },
  { NOTE_A6,   8 },
  { NOTE_F6,   8 },
  { NOTE_F6,   8 },
  { NOTE_A6,   8 },
  { NOTE_A6,   8 },
  { NOTE_F6,   8 },
  { NOTE_F6,   8 },

  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_D5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },

  { NOTE_F4,   8 },
  { NOTE_F5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_F5,   8 },
  { NOTE_C6,   8 },
  { NOTE_BF5,  8 },
  { NOTE_A5,   8 },
  { NOTE_BF5,  8 },

  { NOTE_E4,   8 },
  { NOTE_E5,   8 },
  { NOTE_G5,   8 },
  { NOTE_E5,   8 },
  { NOTE_C6,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_E5,   8 },

  { NOTE_E5,   8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },

  { NOTE_D6,   8 },
  { NOTE_C6,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_F5,   8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },

  { NOTE_C5,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_E4,   8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },

  { NOTE_F4,   8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },
  { NOTE_D5,   8 },
  { NOTE_C5,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },

  { NOTE_F4,   8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },
  { NOTE_C5,   8 },
  { NOTE_BF4,  8 },
  { NOTE_A4,   8 },
  { NOTE_BF4,  8 },

  { NOTE_E4,   8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },
  { NOTE_C5,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },

  { NOTE_E4,   8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },

  { NOTE_F5,   8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_D6,   8 },
  { NOTE_C6,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },

  { NOTE_F5,   8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_C6,   8 },
  { NOTE_BF5,  8 },
  { NOTE_A5,   8 },
  { NOTE_BF5,  8 },

  { NOTE_E5,   8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_C6,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },

  { NOTE_E5,   8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },

  { NOTE_D6,   8 },
  { NOTE_C6,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { R,         8 },
  { R,         8 },
  { R,         8 },
  { R,         8 },

  { NOTE_F5,   8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { R,         8 },
  { R,         8 },
  { R,         8 },
  { R,         8 },

  { NOTE_D5,   8 },
  { NOTE_C5,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },
  { R,         8 },
  { R,         8 },
  { R,         8 },
  { R,         8 },

  { NOTE_BF4,  8 },
  { NOTE_BF4,  8 },
  { NOTE_C5,   8 },
  { NOTE_C5,   8 },
  { NOTE_G4,   8 },
  { NOTE_G4,   8 },
  { NOTE_G4,   8 },
  { NOTE_G4,   8 },

  { NOTE_D6,   8 },
  { NOTE_C6,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_D5,   8 },
  { NOTE_C5,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },

  { NOTE_F5,   8 },
  { NOTE_G5,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },
  { NOTE_F4,   8 },
  { NOTE_G4,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },

  { NOTE_D5,   8 },
  { NOTE_C5,   8 },
  { NOTE_BF4,  8 },
  { NOTE_G4,   8 },
  { NOTE_D6,   8 },
  { NOTE_C6,   8 },
  { NOTE_BF5,  8 },
  { NOTE_G5,   8 },

  { NOTE_G5,   8 },
  { NOTE_G5,   8 },
  { NOTE_G5,   8 },
  { NOTE_G5,   8 },
  { R,         8 },
  { R,         8 },
  { R,         8 },
  { R,         8 },




  
};
const int NOTE_COUNT = sizeof(melody) / sizeof(melody[0]);

// ============================================================
//  Playback engine
// ============================================================
void playSong() {
  for (int i = 0; i < NOTE_COUNT; i++) {
    int note    = melody[i][0];
    int divider = melody[i][1];

    int noteDuration;
    if (divider > 0) {
      noteDuration = WHOLE_NOTE_MS / divider;
    } else {
      noteDuration = (WHOLE_NOTE_MS / abs(divider)) * 1.5;
    }

    if (note == R) {
      noTone(SPEAKER_PIN);
    } else {
      tone(SPEAKER_PIN, note, noteDuration * 0.9);
    }

    delay(noteDuration);
    noTone(SPEAKER_PIN);
  }
}

// ---------------- SETUP ----------------
void setup(){
  pinMode(SPEAKER_PIN, OUTPUT);
  playSong();

  myServo.attach(SERVO_PIN);
  myServo.write(servoAngle);

  FastLED.addLeds<WS2812B,DATA_PIN,GRB>(leds,NUM_LEDS);
  FastLED.setBrightness(10);

  scoreDisplay.setBrightness(7);
  showScore();

  pinMode(startButtonPin, INPUT_PULLUP);
  pinMode(powerUpButtonPin, INPUT_PULLUP);

  randomSeed(analogRead(0)); // important for randomness
}

// ---------------- LOOP ----------------
void loop(){

  checkStartButton();
  checkPowerUpButton();
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

// ---------------- START BUTTON ----------------
void checkStartButton(){

  static bool lastState = HIGH;
  bool current = digitalRead(startButtonPin);

  if(lastState == HIGH && current == LOW){
    startGame();
  }

  lastState = current;
}

void checkPowerUpButton(){
  static bool lastState = HIGH;
  bool current = digitalRead(powerUpButtonPin);

  if(lastState == HIGH && current == LOW && powerUpReady && !powerUpActive){
    activatePowerUp();
  }

  lastState = current;
}

// ---------------- SONG CHART ----------------
// Lanes: 0=Blue, 1=Red, 2=Green, 3=Yellow

const int songLength = 64;

const byte songMap[songLength][4] = {

  // Intro groove
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
  {0,0,0,1}
};

int beatIndex = 0;


// ---------------- START GAME ----------------
void startGame(){

  gameStarted = true;
  score = 0;
  streakCount = 0;
  powerUpReady = false;
  powerUpActive = false;
  powerUpStartTime = 0;
  beatIndex = 0;
  lastMoveTime = millis();
  pinMode(SPEAKER_PIN, OUTPUT);
  playSong();

  for(int r=0;r<BLOCK_ROWS;r++){
    for(int c=0;c<BLOCK_COLS;c++){
      blockGrid[r][c] = 0;
    }
  }

  showScore();
}
//---------------- POWER UP ----------------

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

  int bottomRow = BLOCK_ROWS - 1;

  for(int c = 0; c < BLOCK_COLS; c++){
    if(blockGrid[bottomRow][c] == 1){
      resetStreak();
      break;
    }
  }

  for(int r = BLOCK_ROWS - 1; r > 0; r--){
    for(int c = 0; c < BLOCK_COLS; c++){
      blockGrid[r][c] = blockGrid[r-1][c];
    }
  }

  for(int c = 0; c < BLOCK_COLS; c++)
    blockGrid[0][c] = 0;
}

// ---------------- SPAWN (ALWAYS 2 BLOCKS) ----------------
void spawnBlocks(){
  int count = 0;

  if(beatIndex >= songLength){
    count++;
    if(count == 2){
      return;
    }
    else{
      beatIndex = 0;
    }
  }
    

  for(int c = 0; c < BLOCK_COLS; c++){
    blockGrid[0][c] = songMap[beatIndex][c];
  }

  beatIndex++;
}

// ---------------- DRAW ----------------
void drawBlocks(){

  FastLED.clear();

  for(int r = 0; r < BLOCK_ROWS; r++){
    for(int c = 0; c < BLOCK_COLS; c++){

      if(blockGrid[r][c] == 1){
        drawSquare(c * BLOCK_SIZE, r * BLOCK_SIZE, getColor(c));
      }
    }
  }
}

// ---------------- DRAW SQUARE ----------------
void drawSquare(int startX,int startY,CRGB color){

  for(int y=0;y<BLOCK_SIZE;y++){
    for(int x=0;x<BLOCK_SIZE;x++){
      leds[XY(startX+x,startY+y)] = color;
    }
  }
}

// ---------------- XY MAPPING ----------------
int XY(int x,int y){

  y = HEIGHT - 1 - y;

  if(y % 2 == 0)
    return y * WIDTH + x;
  else
    return y * WIDTH + (WIDTH - 1 - x);
}

// ---------------- COLORS ----------------
CRGB getColor(int col){

  if(powerUpReady && !powerUpActive) return POWER_UP_READY_COLOR;

  if(col == 0) return CRGB::Blue;
  if(col == 1) return CRGB::Red;
  if(col == 2) return CRGB::Green;
  if(col == 3) return CRGB::Yellow;

  return CRGB::Black;
}

// ---------------- Sensors ----------------
void checkButtons(){
  int bottomRow = BLOCK_ROWS - 1;

  for(int c = 0; c < 4; c++){
    long reading = touchSensors[c].capacitiveSensor(30);
    static bool wasTouched[4] = {false, false, false, false};
    bool isTouched = reading > TOUCH_THRESHOLD;

    if(isTouched && !wasTouched[c]){
      if(blockGrid[bottomRow][c] == 1){
        score += pointsForHit();
        blockGrid[bottomRow][c] = 0;
        registerCorrectHit();
        servoAngle += 20;
        servoAngle = constrain(servoAngle, 0, 180);
        myServo.write(servoAngle);
        servoAngle -= 40;
        servoAngle = constrain(servoAngle, 0, 180);
        myServo.write(servoAngle);
        servoAngle += 20;
        servoAngle = constrain(servoAngle, 0, 180);
        myServo.write(servoAngle);
      } else {
        score -= POINTS_PER_HIT;
        if(score < 0) score = 0;
        resetStreak();
      }
      showScore();
    }

    wasTouched[c] = isTouched;
  }
}

// ---------------- SCORE DISPLAY ----------------
void showScore(){

  if(score > 9999) score = 9999;
  scoreDisplay.showNumberDec(score, true);
}

// ---------------- START SCREEN ----------------
void drawStartScreen(){

  FastLED.clear();

  CRGB color = CRGB::Yellow;

  // ---- FACE OUTLINE (circle-ish) ----
  for(int x=4;x<=11;x++){
    leds[XY(x,2)] = color;
    leds[XY(x,13)] = color;
  }

  for(int y=4;y<=11;y++){
    leds[XY(2,y)] = color;
    leds[XY(13,y)] = color;
  }

  // rounded corners
  leds[XY(3,3)] = color;
  leds[XY(12,3)] = color;
  leds[XY(3,12)] = color;
  leds[XY(12,12)] = color;

  // ---- EYES ----
  leds[XY(6,6)] = CRGB::Black;
  leds[XY(6,7)] = CRGB::Black;

  leds[XY(9,6)] = CRGB::Black;
  leds[XY(9,7)] = CRGB::Black;

  // ---- SMILE ----
  leds[XY(5,10)] = CRGB::Black;
  leds[XY(6,11)] = CRGB::Black;
  leds[XY(7,11)] = CRGB::Black;
  leds[XY(8,11)] = CRGB::Black;
  leds[XY(9,11)] = CRGB::Black;
  leds[XY(10,10)] = CRGB::Black;

  FastLED.show();
}
