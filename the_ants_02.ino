#include <FastLED.h>
#include <LiquidCrystal.h>
#include <TM1637Display.h>

//SONG CHOSEN: DAFT PUNK HARDER BETTER FASTER STRONGER
// ---------------- LED MATRIX ----------------
#define WIDTH 16
#define HEIGHT 16
#define NUM_LEDS 256
#define DATA_PIN 6

#define BLOCK_SIZE 4
#define BLOCK_ROWS 4
#define BLOCK_COLS 4

CRGB leds[NUM_LEDS];

// -------------------- TM1637 (4-pin) score display --------------------
const uint8_t TM_CLK = 1;
const uint8_t TM_DIO = 6;
TM1637Display scoreDisplay(TM_CLK, TM_DIO);

// ---------------- BUTTONS ----------------
// Blue, Red, Green, Yellow
const int buttonPins[4] = {A0,A1,A2,A3};

// ---------------- LCD ----------------
//LiquidCrystal lcd(7,8,9,10,11,12);

// ---------------- GAME ----------------
int blockGrid[BLOCK_ROWS][BLOCK_COLS];
int score = 0;
int totalBlocks = 0;
int correctHits = 0;

unsigned long lastMoveTime = 0;
int moveDelay = 488; //Defined by finding beatInterval (60000 / bpm of song (123)) --> 488ms.
                     //SONG CHOSEN: DAFT PUNK HARDER BETTER FASTER STRONGER
// Update TM1637 score display
void showScore() {
  if (score < 0) score = 0;
  if (score > 9999) score = 9999;
  scoreDisplay.showNumberDec(score, true); // leading zeros
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
CRGB getColumnColor(int col){

  if(col == 0) return CRGB::Blue;
  if(col == 1) return CRGB::Red;
  if(col == 2) return CRGB::Green;
  if(col == 3) return CRGB::Yellow;

  return CRGB::Black;
}

// ---------------- DRAW BLOCK ----------------
void drawSquare(int startX,int startY,CRGB color){

  for(int y=0;y<BLOCK_SIZE;y++){
    for(int x=0;x<BLOCK_SIZE;x++){

      leds[XY(startX + x, startY + y)] = color;

    }
  }
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

// ---------------- SETUP ----------------
void setup(){
  // TM1637
  scoreDisplay.setBrightness(7);
  showScore();

  FastLED.addLeds<WS2812B,DATA_PIN,GRB>(leds,NUM_LEDS);
  FastLED.setBrightness(10);

  randomSeed(analogRead(0));

  for(int i=0;i<4;i++)
    pinMode(buttonPins[i],INPUT_PULLUP);

  //lcd.begin(16,2);
  //lcd.print("Score: 0");
}

// ---------------- LOOP ----------------
void loop(){

  checkButtons();

  if(millis() - lastMoveTime > moveDelay){

    lastMoveTime = millis();

    moveBlocksDown();
    spawnBlocks();
    showScore();
  }

  drawBlocks();
  FastLED.show();

  //updateLCD(); --> dont need LCD anymore since using 7 segment 4 digit display for score.
}

// ---------------- MOVE BLOCKS ----------------
void moveBlocksDown(){
  // Count missed blocks in the bottom row before overwriting
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
void spawnBlocks(){

  if(beatIndex >= songLength)
    return;

  for(int c = 0; c < BLOCK_COLS; c++){
    blockGrid[0][c] = songMap[beatIndex][c];
  }

  beatIndex++;
}

// ---------------- DRAW BLOCKS ----------------
void drawBlocks(){

  FastLED.clear();

  for(int r = 0; r < BLOCK_ROWS; r++){
    for(int c = 0; c < BLOCK_COLS; c++){

      if(blockGrid[r][c] == 1){

        drawSquare(c * BLOCK_SIZE, r * BLOCK_SIZE, getColumnColor(c));

      }
    }
  }
}

// ---------------- BUTTON LOGIC ----------------
void checkButtons(){

  static bool lastState[4] = {HIGH,HIGH,HIGH,HIGH};

  int bottomRow = BLOCK_ROWS - 1;

  for(int c=0;c<4;c++){

    bool current = digitalRead(buttonPins[c]);

    if(lastState[c] == HIGH && current == LOW){

      if(blockGrid[bottomRow][c] == 1){
        score += 10;
        correctHits++;
        totalBlocks++;           // counts as a block that was handled
        blockGrid[bottomRow][c] = 0;
      }
      else{

        score -= 10;

      }
      showScore();
    }

    lastState[c] = current;
  }
}

// ---------------- LCD ---------------- OBSOLETE FOR NOW
void updateLCD(){
  // Row 0: Score
  lcd.setCursor(0, 0);
  lcd.print("Score: ");
  lcd.print(score);
  lcd.print("     ");   // clear trailing chars

  // Row 1: Accuracy
  lcd.setCursor(0, 1);
  lcd.print("Accuracy: ");
  if(totalBlocks > 0){
    int accuracy = (correctHits * 100) / totalBlocks;
    lcd.print(accuracy);
    lcd.print("%   ");
  } else {
    lcd.print("N/A  ");
  }
}