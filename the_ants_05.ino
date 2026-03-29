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
  CapacitiveSensor(10, 3),
  CapacitiveSensor(10, 4),
  CapacitiveSensor(10, 5),
  CapacitiveSensor(10, 6)
};
const long TOUCH_THRESHOLD = 100; //sensitivity
const int startButtonPin = 2;

// ---------------- GAME ----------------
int blockGrid[BLOCK_ROWS][BLOCK_COLS];
int score = 0;

unsigned long lastMoveTime = 0;
int moveDelay = 350;

bool gameStarted = false;

// ------------ GRASSHOPPER SERVO -------------- 
Servo myServo;
const int SERVO_PIN = 9;

int servoAngle = 90;   // starting position



// ---------------- SETUP ----------------
void setup(){

  myServo.attach(SERVO_PIN);
  myServo.write(servoAngle);

  FastLED.addLeds<WS2812B,DATA_PIN,GRB>(leds,NUM_LEDS);
  FastLED.setBrightness(10);

  scoreDisplay.setBrightness(7);
  showScore();

  pinMode(startButtonPin, INPUT_PULLUP);

  randomSeed(analogRead(0)); // important for randomness
}

// ---------------- LOOP ----------------
void loop(){

  checkStartButton();

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

  for(int r=0;r<BLOCK_ROWS;r++){
    for(int c=0;c<BLOCK_COLS;c++){
      blockGrid[r][c] = 0;
    }
  }

  showScore();
}

// ---------------- MOVE BLOCKS ----------------
void moveBlocksDown(){

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
        score += 10;
        blockGrid[bottomRow][c] = 0;
        servoAngle += 20;
        servoAngle = constrain(servoAngle, 0, 180);
        delay (150);
        myServo.write(servoAngle);
        servoAngle -= 40;
        servoAngle = constrain(servoAngle, 0, 180);
        myServo.write(servoAngle);
        delay (150);
        servoAngle += 20;
        servoAngle = constrain(servoAngle, 0, 180);
        myServo.write(servoAngle);
      } else {
        score -= 10;
        if(score < 0) score = 0;
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
