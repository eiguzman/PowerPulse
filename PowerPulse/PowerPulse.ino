#include <Wire.h>
#include <LiquidCrystal.h>
#include <Adafruit_INA260.h>

//
// -------------------------------------------------
// HARDWARE OBJECTS
// -------------------------------------------------
//
LiquidCrystal lcd(12,11,5,4,3,2);
Adafruit_INA260 ina260;

//
// -------------------------------------------------
// PIN DEFINITIONS
// -------------------------------------------------
//
const int redPin = 7;
const int greenPin = 8;
const int bluePin = 9;

const int buttonPin = 6;
const int warningLED = 13;

//
// -------------------------------------------------
// CONFIGURATION
// -------------------------------------------------
//
const float dangerThreshold = 125;

const unsigned long debounceDelay = 50;

const int filterSize = 10;

//
// -------------------------------------------------
// DISPLAY MODES
// -------------------------------------------------
//
enum Mode
{
  MODE_ALL,
  MODE_WATT,
  MODE_VOLT,
  MODE_CURRENT
};

//
// -------------------------------------------------
// STATE VARIABLES
// -------------------------------------------------
//
Mode currentMode = MODE_ALL;

bool recording = false;
bool inaDetected = false;

unsigned long startTime = 0;

bool lastButtonState = HIGH;
bool currentButtonState = HIGH;
unsigned long lastDebounceTime = 0;

//
// Moving average filter
//
float powerSamples[filterSize];
int sampleIndex = 0;
float filteredPower = 0;

//
// Warning LED blink state
//
unsigned long lastBlink = 0;
bool ledState = false;

//
// -------------------------------------------------
// RGB LED CONTROL
// -------------------------------------------------
//
void setRGB(int r,int g,int b)
{
  analogWrite(redPin,r);
  analogWrite(greenPin,g);
  analogWrite(bluePin,b);
}

void updateRGB()
{
  if(currentMode == MODE_ALL)
    setRGB(255,255,255);

  else if(currentMode == MODE_WATT)
    setRGB(255,150,0);

  else if(currentMode == MODE_VOLT)
    setRGB(0,0,255);

  else if(currentMode == MODE_CURRENT)
    setRGB(255,105,180);
}

//
// -------------------------------------------------
// BUTTON HANDLING
// -------------------------------------------------
//
void handleButton()
{
  bool reading = digitalRead(buttonPin);

  if(reading != lastButtonState)
    lastDebounceTime = millis();

  if((millis() - lastDebounceTime) > debounceDelay)
  {
    if(reading != currentButtonState)
    {
      currentButtonState = reading;

      if(currentButtonState == LOW)
      {
        currentMode = (Mode)((currentMode + 1) % 4);
        updateRGB();
        lcd.clear();
      }
    }
  }

  lastButtonState = reading;
}

//
// -------------------------------------------------
// MOVING AVERAGE FILTER
// -------------------------------------------------
//
float filterPower(float power)
{
  powerSamples[sampleIndex] = power;
  sampleIndex = (sampleIndex + 1) % filterSize;

  float sum = 0;

  for(int i=0;i<filterSize;i++)
    sum += powerSamples[i];

  return sum / filterSize;
}

//
// -------------------------------------------------
// WARNING LED
// -------------------------------------------------
//
void updateWarningLED(float filteredPower)
{
  if(filteredPower >= dangerThreshold / 3)
  {
    if(millis() - lastBlink >= 200)
    {
      ledState = !ledState;
      digitalWrite(warningLED, ledState);
      lastBlink = millis();
    }
  }
  else
  {
    digitalWrite(warningLED, LOW);
  }
}

//
// -------------------------------------------------
// LCD DISPLAY
// -------------------------------------------------
//
void updateDisplay(float voltage,float current,float totalPower,float elapsedTime)
{
  if(currentMode == MODE_ALL)
  {
    lcd.setCursor(0,0);
    lcd.print("V:");
    lcd.print(voltage,2);
    lcd.print(" I:");
    lcd.print(current,2);

    lcd.setCursor(0,1);
    lcd.print("P:");
    lcd.print(totalPower,2);
    lcd.print(" t=");
    lcd.print(elapsedTime,2);
    lcd.print("   ");
  }

  else if(currentMode == MODE_WATT)
  {
    lcd.setCursor(0,0);
    lcd.print("P:");
    lcd.print(totalPower,2);
    lcd.print("W");

    lcd.setCursor(0,1);
    lcd.print("t=");
    lcd.print(elapsedTime,2);
    lcd.print("        ");
  }

  else if(currentMode == MODE_VOLT)
  {
    lcd.setCursor(0,0);
    lcd.print("V:");
    lcd.print(voltage,2);
    lcd.print("V");

    lcd.setCursor(0,1);
    lcd.print("t=");
    lcd.print(elapsedTime,2);
    lcd.print("        ");
  }

  else if(currentMode == MODE_CURRENT)
  {
    lcd.setCursor(0,0);
    lcd.print("I:");
    lcd.print(current,2);
    lcd.print("A");

    lcd.setCursor(0,1);
    lcd.print("t=");
    lcd.print(elapsedTime,2);
    lcd.print("        ");
  }
}

//
// -------------------------------------------------
// SERIAL COMMANDS
// -------------------------------------------------
//
void handleSerial()
{
  if(!Serial.available())
    return;

  char cmd = Serial.read();

  if(cmd == 'r')
  {
    recording = true;
    startTime = millis();

    Serial.println("time,voltage,current,total_power");
  }

  if(cmd == 's')
  {
    recording = false;
  }
}

//
// -------------------------------------------------
// SETUP
// -------------------------------------------------
//
void setup()
{
  Serial.begin(115200);

  lcd.begin(16,2);

  pinMode(redPin,OUTPUT);
  pinMode(greenPin,OUTPUT);
  pinMode(bluePin,OUTPUT);

  pinMode(buttonPin,INPUT_PULLUP);
  pinMode(warningLED,OUTPUT);

  inaDetected = ina260.begin();

  if(!inaDetected)
  {
    lcd.setCursor(0,0);
    lcd.print("INA260 FAIL");
    setRGB(255,0,0);
  }
  else
  {
    setRGB(255,255,255);
  }

  delay(1000);

  for(int i=0;i<filterSize;i++)
    powerSamples[i] = 0;

  lcd.clear();
}

//
// -------------------------------------------------
// LOOP
// -------------------------------------------------
//
void loop()
{
  handleSerial();
  handleButton();

  if(!inaDetected)
  {
    setRGB(255,0,0);
    return;
  }

  float voltage = ina260.readBusVoltage()/1000;
  float current = ina260.readCurrent()/1000;
  float power   = ina260.readPower()/1000;

  filteredPower = filterPower(power);

  float totalPower = filteredPower * 3.0;

  updateWarningLED(filteredPower);

  float elapsedTime = 0;

  if(recording)
    elapsedTime = (millis() - startTime) / 1000.0;

  updateDisplay(voltage,current,totalPower,elapsedTime);

  if(recording)
  {
    Serial.print(elapsedTime,2);
    Serial.print(",");
    Serial.print(voltage);
    Serial.print(",");
    Serial.print(current);
    Serial.print(",");
    Serial.println(totalPower);
  }

  delay(100);
}