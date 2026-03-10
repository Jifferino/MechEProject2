#include <FastLED.h>
#include <LiquidCrystal.h>

// ---------------- LED MATRIX ----------------
#define WIDTH 16
#define HEIGHT 16
#define NUM_LEDS 256
#define DATA_PIN 6

#define BLOCK_SIZE 4
#define BLOCK_ROWS 4
#define BLOCK_COLS 4

CRGB leds[NUM_LEDS];

// ---------------- BUTTONS ----------------
// Blue, Red, Green, Yellow
const int buttonPins[4] = {5,4,3,2};

// ---------------- LCD ----------------
LiquidCrystal lcd(7,8,9,10,11,12);

// ---------------- GAME ----------------
int blockGrid[BLOCK_ROWS][BLOCK_COLS];
int score = 0;
int totalBlocks = 0;
int correctHits = 0;

unsigned long lastMoveTime = 0;
int moveDelay = 0;

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

// ---------------- SETUP ----------------
void setup(){

  FastLED.addLeds<WS2812B,DATA_PIN,GRB>(leds,NUM_LEDS);
  FastLED.setBrightness(10);

  randomSeed(analogRead(0));

  moveDelay = random(100,200);

  for(int i=0;i<4;i++)
    pinMode(buttonPins[i],INPUT_PULLUP);

  lcd.begin(16,2);
  lcd.print("Score: 0");
}

// ---------------- LOOP ----------------
void loop(){

  checkButtons();

  if(millis() - lastMoveTime > moveDelay){

    lastMoveTime = millis();

    moveBlocksDown();
    spawnBlocks();

    moveDelay = random(400,600);

  }

  drawBlocks();
  FastLED.show();

  updateLCD();
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

  int c1 = random(BLOCK_COLS);
  int c2 = random(BLOCK_COLS);

  while(c2 == c1)
    c2 = random(BLOCK_COLS);

  blockGrid[0][c1] = 1;
  blockGrid[0][c2] = 1;
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
    }

    lastState[c] = current;
  }
}

// ---------------- LCD ----------------
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