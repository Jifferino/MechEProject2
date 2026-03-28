#include <FastLED.h>
#include <TM1637Display.h>

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
const int buttonPins[4] = {5,4,3,6};
const int startButtonPin = 2;

// ---------------- GAME ----------------
int blockGrid[BLOCK_ROWS][BLOCK_COLS];
int score = 0;

unsigned long lastMoveTime = 0;
int moveDelay = 350;

bool gameStarted = false;


// ---------------- SETUP ----------------
void setup(){

  FastLED.addLeds<WS2812B,DATA_PIN,GRB>(leds,NUM_LEDS);
  FastLED.setBrightness(10);

  scoreDisplay.setBrightness(7);
  showScore();

  for(int i=0;i<4;i++)
    pinMode(buttonPins[i],INPUT_PULLUP);

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

  // clear row
  for(int c=0;c<BLOCK_COLS;c++){
    blockGrid[0][c] = 0;
  }

  int c1 = random(4);
  int c2 = random(4);

  while(c2 == c1)
    c2 = random(4);

  blockGrid[0][c1] = 1;
  blockGrid[0][c2] = 1;
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

// ---------------- BUTTONS ----------------
void checkButtons(){

  static bool lastState[4] = {HIGH,HIGH,HIGH,HIGH};
  int bottomRow = BLOCK_ROWS - 1;

  for(int c=0;c<4;c++){

    bool current = digitalRead(buttonPins[c]);

    if(lastState[c] == HIGH && current == LOW){

      if(blockGrid[bottomRow][c] == 1){
        score += 10;
        blockGrid[bottomRow][c] = 0;
      }
      else{
        score -= 10;
        if(score < 0) score = 0;
      }

      showScore();
    }

    lastState[c] = current;
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
