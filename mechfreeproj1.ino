#include <SevSeg.h>
SevSeg sevseg;
const int greenLEDPin1 = 2; //This is a digital input/output/PWM pin
const int greenLEDPin2 = 4; //This is a digital input/output/PWM pin
const int greenLEDPin3 = 7; //This is a digital input/output/PWM pin
const int greenLEDPin4 = 8; //This is a digital input/output/PWM pin
const int buttonPin = 12; //This is a digital input/output/PWM pin
float timeSeconds;
int score = 0;

int pinA = A0;int pinB = A2;int pinC = 9;int pinD = 11;
int pinE = 13;int pinF = A1; int pinG = 6;
int pinDP = 10;int D1 = A5;int D2 = A4;int D3 = 5; int D4 = 3;

const int delayTime = 500;

boolean buttonState=LOW;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(buttonPin, INPUT); 
  pinMode(greenLEDPin1, OUTPUT); 
  pinMode(greenLEDPin2, OUTPUT); 
  pinMode(greenLEDPin3, OUTPUT); 
  pinMode(greenLEDPin4, OUTPUT); 

  pinMode(pinA, OUTPUT);
  pinMode(pinB, OUTPUT);
  pinMode(pinC, OUTPUT);
  pinMode(pinD, OUTPUT);
  pinMode(pinE, OUTPUT);
  pinMode(pinF, OUTPUT);
  pinMode(pinG, OUTPUT);
  pinMode(pinDP, OUTPUT);
  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D3, OUTPUT);
  pinMode(D4, OUTPUT);

  // CHANGE THESE PINS TO MATCH YOUR WIRING:
  byte numDigits = 4;
  byte digitPins[]   = {D1, D2, D3, D4};            // D1..D4
  byte segmentPins[] = {pinA, pinB, pinC, pinD, pinE, pinF, pinG, pinDP};   // a,b,c,d,e,f,g,dp

  bool resistorsOnSegments = true;
  sevseg.begin(COMMON_CATHODE, numDigits, digitPins, segmentPins, resistorsOnSegments);

  sevseg.setBrightness(90);
}

void allOff() {
  digitalWrite(greenLEDPin1, LOW);
  digitalWrite(greenLEDPin2, LOW);
  digitalWrite(greenLEDPin3, LOW);
  digitalWrite(greenLEDPin4, LOW);
}

// helper: a delay that can be interrupted immediately by button press
// returns true if it was interrupted (button pressed), false otherwise
bool delayWithButtonAbort(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    sevseg.refreshDisplay();          // <-- KEEP DISPLAY ALIVE
    if (digitalRead(buttonPin) == HIGH) {   // button pressed
      return true;
    }
    delay(1); // tiny sleep so we don't spin at 100% CPU
  }
  return false;
}

void loop() {

  // If button is held, keep everything off and wait until released
  if (digitalRead(buttonPin) == HIGH) {
    allOff(); 
    while (digitalRead(buttonPin) == HIGH) {
      // stay here until user lets go
      sevseg.refreshDisplay();          // <-- KEEP DISPLAY ALIVE
      delay(1);
    }
    // released -> restart sequence from LED1
  }
  sevseg.refreshDisplay();

  // Run one full sequence, but abort immediately if button gets pressed
  // LED 1
  digitalWrite(greenLEDPin1, HIGH);
  if (delayWithButtonAbort(delayTime)){ 
    allOff(); 
    return; 
  }
  digitalWrite(greenLEDPin1, LOW);

  // LED 2
  digitalWrite(greenLEDPin2, HIGH);
  if (delayWithButtonAbort(delayTime)){ 
    allOff(); 
    return; 
  }
  digitalWrite(greenLEDPin2, LOW);

  // LED 3
  digitalWrite(greenLEDPin3, HIGH);
  if (delayWithButtonAbort(delayTime)){ 
    allOff(); 
    return; 
  }
  digitalWrite(greenLEDPin3, LOW);

  // LED 4
  timeSeconds = millis()/1000.0;
  digitalWrite(greenLEDPin4, HIGH);
  if (delayWithButtonAbort(delayTime)){ 
    allOff();
    timeSeconds = millis()/1000.0 - timeSeconds;
    if(timeSeconds < 0.25){ //Should standardize to be half of the delay time or something
      score += 10;
    } 
    else if(timeSeconds > 1.25){
      score += 0;
    }
    else{
      score += int(10 * (1 - (timeSeconds - 0.25)));
    }
    Serial.println(score);
    sevseg.setNumber(score, 0);
    return; 
  }
  digitalWrite(greenLEDPin4, LOW);
  
}
