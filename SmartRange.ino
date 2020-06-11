#include <Servo.h>                    // 서보모터 라이브러리
#include <Wire.h>                     // i2C 통신을 위한 라이브러리
#include <LiquidCrystal_I2C.h>        // LCD 2004 I2C용 라이브러리

LiquidCrystal_I2C lcd(0x27, 16, 2); //  0x3F or 0x27를 선택하여 주세요. 작동이 되지 않는 경우 0x27로 바꾸어주세요. 확인결과 0x3f가 작동하지 않을 수 있습니다.

Servo servo;
int pinServoMotor = 8;
int servoTurnOffAngle = 20;

int pinJoystickX = 0;
int pinJoystickY = 1;
int pinJoystickPush = 6;

byte highlight[8] = {
  B01000,
  B01100,
  B01110,
  B01111,
  B01111,
  B01110,
  B01100,
  B01000
};

const int MODE_TIME_FINISH = 11;
const int MODE_TARGET_TEMP = 12;
const int MODE_FINISH_SET_TEMP = 13;
const int MODE_FINISH_SET_MIN = 14;
const int MODE_FINISH_SET_SEC = 15;
const int MODE_TARGET_SET_TEMP = 16;
const int MODE_RUNNING_TIME_FINISH = 51;
const int MODE_RUNNING_TARGET_TEMP = 52;

const String NAME_TIME_FINISH = "Smart Finish";
const String NAME_TARGET_TEMP = "Set Target Temp";

enum JoystickState
{
  JOY_NONE, JOY_LEFT, JOY_RIGHT, JOY_UP, JOY_DOWN, JOY_CLICK
};
char *joyNames[] = 
{
  "None", "Left", "Right", "Up", "Down", "Click"
};

int currentMode = MODE_TIME_FINISH;
int secondsTimeout = 300;

int incrementTemp = 10;     //온도 증감 변화량
int incrementSeconds = 10;  //시간 초 증감 변화량

//For Smart Finish
bool timefinishRunning;
bool timecountStarted;

int countStartTemp = 70;    //온도 설정 기본값
int secondsToEnd = 180;     //시간 설정 기본값

unsigned long runningStartMillis;
unsigned long timeCountStartMillis;
unsigned long timeCountEndMillis;
unsigned long previousLCDRefreshMillis;
long runningInterval = 1000;
int elapsedTime;

int targetTemp = 170;

bool handled;

//testmode
int currentTemp = 30;
void setup() {
  Serial.begin(9600);
  pinMode(pinJoystickPush, INPUT);
  digitalWrite(pinJoystickPush, HIGH);
  
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, highlight);
  refreshLCD();  
}

void loop() {
  delay(100);
  JoystickState joy = getJoystickState();
  
  if (joy != JOY_NONE)
  {
    handled = handleJoystick(joy);    
    if (handled)
    {
      refreshLCD();
      delay(400);
    }
  }

  if (currentMode == MODE_RUNNING_TIME_FINISH)
  {
    handleRunningTimeFinish();    
  }
  else if (currentMode == MODE_RUNNING_TARGET_TEMP)
  {
    handleRunningTargetTemp();
  } 
}

JoystickState getJoystickState()
{   
  JoystickState joy = JOY_NONE;
  int xPosition = analogRead(pinJoystickX);
  int yPosition = analogRead(pinJoystickY);
  int pushed = digitalRead(pinJoystickPush);

  Serial.print("pushed : ");
  Serial.print(pushed);
  Serial.print("  X: ");
  Serial.print(xPosition);
  Serial.print("  Y: ");
  Serial.print(yPosition);
  Serial.println("");
  
  if (xPosition < 300)
  {
    joy = JOY_LEFT;
  }
  else if (xPosition > 700)
  {
    joy = JOY_RIGHT;
  }
  else if (yPosition < 300)
  {
    joy = JOY_UP;
  }
  else if (yPosition > 700)
  {
    joy = JOY_DOWN;
  }
  else if (pushed == 0)
  {
    joy = JOY_CLICK;
  }   
  return joy;
}

bool handleJoystick(JoystickState joy)
{ 
  Serial.print("Handle Joystick: ");
  Serial.println(joyNames[joy]);
  if (currentMode == MODE_TIME_FINISH)
  {
    switch (joy)
    {
      case JOY_DOWN:
        currentMode = MODE_TARGET_TEMP;
        return true;
      case JOY_RIGHT:
      case JOY_CLICK:
        currentMode = MODE_FINISH_SET_TEMP;
        return true;
      default:
        return false;
    }    
  }
  else if (currentMode == MODE_FINISH_SET_TEMP)
  {
    switch (joy)
    {
      case JOY_LEFT:
        currentMode = MODE_TIME_FINISH;
        return true;
      case JOY_RIGHT:
        currentMode = MODE_FINISH_SET_MIN;
        return true;
      case JOY_UP:
        countStartTemp += incrementTemp;
        return true;
      case JOY_DOWN:
        countStartTemp -= incrementTemp;
        return true;
      default:
        return false;
    }
  }
  else if (currentMode == MODE_FINISH_SET_MIN)
  {
    switch (joy)
    {
      case JOY_LEFT:
        currentMode = MODE_FINISH_SET_TEMP;
        return true;
      case JOY_UP:
        secondsToEnd += 60;
        return true;
      case JOY_DOWN:
      if (secondsToEnd >= 60)
        {
          secondsToEnd -= 60;
        }
        return true;
      case JOY_RIGHT:
      case JOY_CLICK:
        currentMode = MODE_FINISH_SET_SEC;
        return true;
      default:
        return false;
    }
  }
  else if (currentMode == MODE_FINISH_SET_SEC)
  {
    switch (joy)
    {
      case JOY_LEFT:
        currentMode = MODE_FINISH_SET_MIN;
        return true;
      case JOY_UP:
        secondsToEnd += incrementSeconds;
        return true;
      case JOY_DOWN:
        if (secondsToEnd >= incrementSeconds)
        {
          secondsToEnd -= incrementSeconds;
          return true;  
        }
      case JOY_RIGHT:
      case JOY_CLICK:
        currentMode = MODE_RUNNING_TIME_FINISH;
        return true;
      default:
        return false;
    }
  }
  else if (currentMode == MODE_RUNNING_TIME_FINISH)
  {
    switch (joy)
    {
      case JOY_LEFT:
        currentMode = MODE_FINISH_SET_SEC;
        return true;
      case JOY_UP:
        //testmode        
        currentTemp += 10;
        return true;
      case JOY_DOWN:
        //testmode
        currentTemp -= 10;
        return true;      
      case JOY_CLICK:
        currentMode = MODE_TIME_FINISH;
        return true;
      default:
        return false;
    }
  }
  else if (currentMode == MODE_TARGET_TEMP)
  {
    switch (joy)
    {
      case JOY_UP:
        currentMode = MODE_TIME_FINISH;
        return true;
      case JOY_RIGHT:
      case JOY_CLICK:
        currentMode = MODE_TARGET_SET_TEMP;
        return true;
      default:
        return false;
    }
  }  
}

void refreshLCD()
{
  Serial.println("Refresh LCD / Current Mode: " + currentMode);
    
  if (currentMode == MODE_TIME_FINISH)
  {
    lcd.clear();
    lcd.setCursor(0,0);  
    lcd.write(byte(0));
    lcd.setCursor(1,0);  
    lcd.print(NAME_TIME_FINISH);  
    lcd.setCursor(1,1);
    lcd.print(NAME_TARGET_TEMP);
  }
  else if (currentMode == MODE_TARGET_TEMP)
  {
    lcd.clear();    
    lcd.setCursor(1,0);  
    lcd.print(NAME_TIME_FINISH);  
    lcd.setCursor(0,1);
    lcd.write(byte(0));
    lcd.setCursor(1,1);
    lcd.print(NAME_TARGET_TEMP);
  }
  else if (currentMode == MODE_FINISH_SET_TEMP
          || currentMode == MODE_FINISH_SET_MIN
          || currentMode == MODE_FINISH_SET_SEC)
  {
    lcd.clear();
    lcd.setCursor(0,0);
    int temp = getTemperature();
    lcd.print("Current:" + intToFixedLengthString(temp, 3, " ") + (char)223 + "C");    

    int startIndex = 0;
    lcd.setCursor(1,1);
    lcd.print(intToFixedLengthString(countStartTemp, 3, " ") + (char)223 + "C");
    lcd.print("  ");
    lcd.print(intToFixedLengthString((int)(secondsToEnd / 60.0), 2, "0"));
    lcd.print("M ");
    lcd.print(intToFixedLengthString((int)(secondsToEnd % 60), 2, "0"));
    lcd.print("S");

    if (currentMode == MODE_FINISH_SET_TEMP)
    {
      lcd.setCursor(startIndex,1);
    }
    else if (currentMode == MODE_FINISH_SET_MIN)
    {
      lcd.setCursor(startIndex + 7,1);
    }        
    else if (currentMode == MODE_FINISH_SET_SEC)
    {
      lcd.setCursor(startIndex + 11,1);
    }        
    lcd.write(byte(0));
  }
  else if (currentMode == MODE_RUNNING_TIME_FINISH)
  {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("MODE_RUNNING_TIME_FINISH");
    lcd.setCursor(0,1);
    int temp = getTemperature();
    lcd.print("Current: " + intToFixedLengthString(temp, 3, " ") + (char)223 + "C");   
  }
}
void handleSettingTimeFinish()
{
  unsigned long currentMillis = millis();
  if (currentMillis - previousLCDRefreshMillis < runningInterval)
  {
    return;
  }
}
void handleRunningTimeFinish()
{
  unsigned long currentMillis = millis();
  
  if (timefinishRunning == false)
  {
    runningStartMillis = currentMillis;  
    timefinishRunning = true;
    previousLCDRefreshMillis = runningStartMillis;
  }
  
  if (currentMillis - previousLCDRefreshMillis < runningInterval)
  {
    return;
  }
  
  if (timecountStarted == false)
  {
      int temp = getTemperature();
      if (temp >= countStartTemp)
      {
        timecountStarted = true;
        timeCountStartMillis = currentMillis;
        timeCountEndMillis = timeCountStartMillis + secondsToEnd * 1000;
      }                   
  }
  
  if (timecountStarted)
  {
    if (timeCountEndMillis < currentMillis)
    {
      lcdShowCompleteMessage();
      leverTurnOff();
      playCompleteSound();
      timefinishRunning = false;
      currentMode = MODE_TIME_FINISH;
    }
  }

  refreshLCD();  
}

int getTemperature()
{
  return currentTemp;
}

String intToFixedLengthString(int value, int fixedLength, String addEmpty)
{
  String resultText = String(value);
  String emptyText = "";
  for(int i = resultText.length(); i < fixedLength; i++)
  {
    emptyText += addEmpty;
  }
  return emptyText + resultText;
}
String secondsToTime(int seconds)
{
  String hourText = intToFixedLengthString((int)(seconds / 60.0), 2, "0");
  String secondText = intToFixedLengthString((int)(seconds % 60), 2, "0");
  return hourText + "M " + secondText;
}
void lcdShowCompleteMessage()
{
  
}


void leverTurnOff()
{
  servo.attach(pinServoMotor);
  servo.write(servoTurnOffAngle);
  delay(500);
  servo.detach();  
}
void playCompleteSound()
{
  
}

void handleRunningTargetTemp()
{
  
}
