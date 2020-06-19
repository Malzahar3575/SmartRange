#include <Wire.h>                     // i2C 통신을 위한 라이브러리
#include <LiquidCrystal_I2C.h>        // LCD 2004 I2C용 라이브러리
#include <Adafruit_MLX90614.h>        // 비접촉 온도센서 라이브러리

LiquidCrystal_I2C lcd(0x27, 16, 2);   //  0x3F or 0x27를 선택하여 주세요. 작동이 되지 않는 경우 0x27로 바꾸어주세요. 확인결과 0x3f가 작동하지 않을 수 있습니다.
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

//PIN Settings
int pinJoystickX = 0;
int pinJoystickY = 1;
int pinJoystickPush = 6;
int pinBuzzer = 3;

enum MainMenu
{
  MENU_MANUAL, MENU_EGG, MENU_CUPRAMEN, MENU_THREECOOK, MENU_MEAT, MENU_COUNT
};
char *menuNames[] = 
{
  "Manual", "Egg", "Cup Ramen", "Three Cook", "Meat"
};

enum SettingMenu
{
  SET_TEMP, SET_MIN, SET_SEC, SETTING_MENU_COUNT
};

enum JoystickState
{
  JOY_NONE, JOY_LEFT, JOY_RIGHT, JOY_UP, JOY_DOWN, JOY_CLICK
};
char *joyNames[] = 
{
  "None", "Left", "Right", "Up", "Down", "Click"
};

const int MODE_MENU = 1;
const int MODE_SETTING = 2;
const int MODE_RUNNING = 3;

int currentMode = MODE_MENU;    //현재 모드
MainMenu currentMenu = MENU_MANUAL;  //현재 메뉴
SettingMenu currentSettingMenu = SET_TEMP;   //현재 설정 메뉴

int incrementTemp = 5;     //온도 증감 변화량
int incrementSeconds = 10;  //시간 초 증감 변화량
int secondsTimeout = 300;   //모든 모드의 제한 시간(초)

bool timecountStarted;
int countStartTemp = 70;    //카운트 시작 온도
unsigned int secondsToEnd = 180;     //서보모터 동작까지 시간

unsigned long runningStartMillis;
unsigned long timeCountEndMillis;
unsigned long preHandleRunningMillis;
unsigned long preSettingRefreshMillis;
long intervalRunningHandle = 900;
long intervalSettingHandle = 1000;
int remainingSeconds;

bool handled;

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


void setup() {
  Serial.begin(9600);
  pinMode(pinJoystickPush, INPUT);
  digitalWrite(pinJoystickPush, HIGH);
  mlx.begin();
  pinMode(pinBuzzer, OUTPUT);
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
      delay(300);
    }
  }

  if (currentMode == MODE_SETTING)
  {
    handleSetting();
  }
  else if (currentMode == MODE_RUNNING)
  {
    handleRunning();
  }  
}

JoystickState getJoystickState()
{   
  JoystickState joy = JOY_NONE;
  int xPosition = analogRead(pinJoystickX);
  int yPosition = analogRead(pinJoystickY);
  int pushed = digitalRead(pinJoystickPush);
  
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
  if (currentMode == MODE_MENU)
  {    
    switch (joy)
    {      
      case JOY_UP:
        return tryMoveToPreMenu();
      case JOY_DOWN:
        return tryMoveToNextMenu();
      case JOY_RIGHT:
      case JOY_CLICK:
        enterMenu(currentMenu);
        return true;
      default:
        return false;
    }
  }
  else if (currentMode == MODE_SETTING)
  {
    bool result;
    switch (joy)
    {
      case JOY_LEFT:
        result = tryMoveToPreSetting();
        if (result == false)
        {
          currentMode = MODE_MENU;
          result = true;
        }
        return result;
      case JOY_RIGHT:
        result = tryMoveToNextSetting();
        if (result == false)
        {
          startRunning();
          result = true;
        }
        return result;
      case JOY_UP:
      case JOY_DOWN:
        changeSetting(currentSettingMenu, joy);
        return true;
      case JOY_CLICK:
        startRunning();
        return true;
      default:
        return false;
    }
  }
  else if (currentMode == MODE_RUNNING)
  {
    switch (joy)
    {      
      case JOY_LEFT:
        currentMode = MODE_SETTING;
        return true;
      case JOY_CLICK:
        stopRunning(false);
        return true;
      default:
        return false;
    }
  }
}

void refreshLCD()
{
  Serial.println("Refresh LCD / Current Mode: " + currentMode);
    
  if (currentMode == MODE_MENU)
  {
    String firstRowName = "";
    String secondRowName = "";
    
    int menuIndex = currentMenu;
    int preMenuIndex = menuIndex - 1;
    int nextMenuIndex = menuIndex + 1;
    int menuRowIndex = menuIndex % 2;
    
    if (menuRowIndex == 1)
    {
      firstRowName = menuNames[menuIndex - 1];
      secondRowName = menuNames[menuIndex];
    }
    else
    {
      firstRowName = menuNames[menuIndex];
      if (nextMenuIndex < MENU_COUNT)
      {
        secondRowName = menuNames[nextMenuIndex];
      }
    }
    
    lcd.clear();
    lcd.setCursor(0,menuRowIndex);
    lcd.write(byte(0));
    lcd.setCursor(1,0);
    lcd.print(firstRowName);
    lcd.setCursor(1,1);
    lcd.print(secondRowName);
  }
  else if (currentMode == MODE_SETTING)
  {
    lcd.clear();
    lcd.setCursor(0,0);
    int temp = getObjectTemperature();
    lcd.print("Current:" + intToFixedLengthString(temp, 3, " ") + (char)223 + "C");    

    int startIndex = 0;
    lcd.setCursor(1,1);
    lcd.print(intToFixedLengthString(countStartTemp, 3, " ") + (char)223 + "C");
    lcd.print("  ");
    lcd.print(intToFixedLengthString((int)(secondsToEnd / 60.0), 2, "0"));
    lcd.print("M ");
    lcd.print(intToFixedLengthString((int)(secondsToEnd % 60), 2, "0"));
    lcd.print("S");

    if (currentSettingMenu == SET_TEMP)
    {
      lcd.setCursor(startIndex,1);
    }
    else if (currentSettingMenu == SET_MIN)
    {
      lcd.setCursor(startIndex + 7,1);
    }        
    else if (currentSettingMenu == SET_SEC)
    {
      lcd.setCursor(startIndex + 11,1);
    }        
    lcd.write(byte(0));    

    preSettingRefreshMillis = millis();
  }
  else if (currentMode == MODE_RUNNING)
  {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Temp:");    
    int temp = getObjectTemperature();
    lcd.print(intToFixedLengthString(temp, 3, " ") + (char)223 + "C");
    lcd.print("/");
    lcd.print(intToFixedLengthString(countStartTemp, 3, " ") + (char)223 + "C");    
    lcd.setCursor(0,1);
    lcd.print("Time: ");
    lcd.print(intToFixedLengthString((int)(remainingSeconds / 60.0), 2, "0"));
    lcd.print("M");
    lcd.print(intToFixedLengthString((int)(remainingSeconds % 60), 2, "0"));
    lcd.print("S");
  }
}
void handleSetting()
{
  unsigned long currentMillis = millis();
  if (currentMillis - preSettingRefreshMillis < intervalSettingHandle)
  {
    return;
  }
  Serial.println("Handle Setting");
  refreshLCD();
}
void startRunning()
{
  currentMode = MODE_RUNNING;
  runningStartMillis = millis();
  preHandleRunningMillis = runningStartMillis - intervalRunningHandle;
  timecountStarted = false;
  remainingSeconds = secondsToEnd;
}
void stopRunning(bool complete)
{
  if (complete)
  {    
    playCompleteSound();
  }  
  currentMode = MODE_MENU;
}
void handleRunning()
{
  unsigned long currentMillis = millis();
  
  if (currentMillis - preHandleRunningMillis < intervalRunningHandle)
  {
    return;
  }  
  
  Serial.println("Handle Running");
  preHandleRunningMillis = currentMillis;
    
  if (timecountStarted == false)
  {
      int temp = getObjectTemperature();
      if (temp >= countStartTemp)
      {
        timecountStarted = true;
        timeCountEndMillis = currentMillis + (unsigned long)secondsToEnd * 1000;
        Serial.println(currentMillis);
        Serial.println(timeCountEndMillis);
        Serial.println(secondsToEnd);
      }      
  }  
  if (timecountStarted)
  {
    if (timeCountEndMillis < currentMillis)
    {
      stopRunning(true);
    }
    else
    {
      remainingSeconds = (timeCountEndMillis - currentMillis) / 1000;
      Serial.println("remaing time");
      Serial.println(timeCountEndMillis);
      Serial.println(currentMillis);
      Serial.println(remainingSeconds);
    }
  }

  refreshLCD();
  
}

int getObjectTemperature()
{
  double temp = mlx.readObjectTempC();
  Serial.print("Object temp.= "); Serial.print(temp); Serial.println("*C");
  return (int)temp;
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

void enterMenu(int menu)
{
  if (menu == MENU_MANUAL)
  {
    startSetting(0, 0);
  }
  else if (menu == MENU_EGG)
  {
    startSetting(60, 7 * 60);
  }
  else if (menu == MENU_CUPRAMEN)
  {
    startSetting(50, 0);
  }
  else if (menu == MENU_THREECOOK)
  {
    startSetting(60, 3 * 60);
  }
  else if (menu == MENU_MEAT)
  {
    startSetting(170, 0);
  }
}

void startSetting(int temp, int seconds)
{
  countStartTemp = temp;
  secondsToEnd = seconds;
  currentMode = MODE_SETTING;
  currentSettingMenu = 0;  
}
void changeSetting(int currentSettingMenu, int joy)
{
  if (currentSettingMenu == SET_TEMP)
  {
    if (joy == JOY_UP)
    {
      countStartTemp += incrementTemp;
    }
    else if (joy == JOY_DOWN)
    {
      countStartTemp -= incrementTemp;
    }
    
    if (countStartTemp < 0)
    {
      countStartTemp = 0;
    }
    else if (countStartTemp > 250)
    {
      countStartTemp = 250;
    }
  }
  else if (currentSettingMenu == SET_MIN)
  {
    if (joy == JOY_UP)
    {
      secondsToEnd += 60;
    }
    else if (joy == JOY_DOWN)
    {
      if (secondsToEnd >= 60)
      {
        secondsToEnd -= 60;  
      }      
    }
    if (secondsToEnd < 0)
    {
      secondsToEnd = 0;
    }
    else if (secondsToEnd > 60 * 60)
    {
      secondsToEnd = 60 * 60;
    }
  }
  else if (currentSettingMenu == SET_SEC)
  {
    if (joy == JOY_UP)
    {
      secondsToEnd += incrementSeconds;
    }
    else if (joy == JOY_DOWN)
    {
      secondsToEnd -= incrementSeconds;
    }

    if (secondsToEnd < 0)
    {
      secondsToEnd = 0;
    }
    else if (secondsToEnd > 60 * 60)
    {
      secondsToEnd = 60 * 60;
    }
  }
}
bool tryMoveToPreMenu()
{
  int preMenu = currentMenu - 1;
  if (preMenu > -1)
  {
    currentMenu = preMenu;
    return true;
  }
  return false;
}
bool tryMoveToNextMenu()
{
  int nextMenu = currentMenu + 1;
  if (nextMenu < MENU_COUNT)
  {
    currentMenu = nextMenu;
    return true;
  }
  return false;
}

bool tryMoveToPreSetting()
{
  int preSetting = currentSettingMenu - 1;
  if (preSetting > -1)
  {
    currentSettingMenu = preSetting;
    return true;
  }
  return false;
}
bool tryMoveToNextSetting()
{
  int nextSetting = currentSettingMenu + 1;
  if (nextSetting < SETTING_MENU_COUNT)
  {
    currentSettingMenu = nextSetting;
    return true;
  }
  return false;
}

//for sound
#define NOTE_E6  1319
#define NOTE_G6  1568
#define NOTE_A6  1760
#define NOTE_AS6 1865
#define NOTE_B6  1976
#define NOTE_C7  2093
#define NOTE_D7  2349
#define NOTE_E7  2637
#define NOTE_F7  2794
#define NOTE_G7  3136
#define NOTE_A7  3520


int melody[] = {
  NOTE_E7, NOTE_E7, 0, NOTE_E7,
  0, NOTE_C7, NOTE_E7, 0,
  NOTE_G7, 0, 0,  0,
  NOTE_G6, 0, 0, 0,
 
  NOTE_C7, 0, 0, NOTE_G6,
  0, 0, NOTE_E6, 0,
  0, NOTE_A6, 0, NOTE_B6,
  0, NOTE_AS6, NOTE_A6, 0,
 
  NOTE_G6, NOTE_E7, NOTE_G7,
  NOTE_A7, 0, NOTE_F7, NOTE_G7,
  0, NOTE_E7, 0, NOTE_C7,
  NOTE_D7, NOTE_B6, 0, 0,
 
  NOTE_C7, 0, 0, NOTE_G6,
  0, 0, NOTE_E6, 0,
  0, NOTE_A6, 0, NOTE_B6,
  0, NOTE_AS6, NOTE_A6, 0,
 
  NOTE_G6, NOTE_E7, NOTE_G7,
  NOTE_A7, 0, NOTE_F7, NOTE_G7,
  0, NOTE_E7, 0, NOTE_C7,
  NOTE_D7, NOTE_B6, 0, 0
};
//Mario main them tempo
int tempo[] = {
  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12,
 
  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12,
 
  9, 9, 9,
  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12,
 
  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12,
 
  9, 9, 9,
  12, 12, 12, 12,
  12, 12, 12, 12,
  12, 12, 12, 12,
};

void buzz(int targetPin, long frequency, long length) {  
  long delayValue = 1000000 / frequency / 2; // calculate the delay value between transitions
  //// 1 second's worth of microseconds, divided by the frequency, then split in half since
  //// there are two phases to each cycle
  long numCycles = frequency * length / 1000; // calculate the number of cycles for proper timing
  //// multiply frequency, which is really cycles per second, by the number of seconds to
  //// get the total number of cycles to produce
  for (long i = 0; i < numCycles; i++) { // for the calculated length of time...
    digitalWrite(targetPin, HIGH); // write the buzzer pin high to push out the diaphram
    delayMicroseconds(delayValue); // wait for the calculated delay value
    digitalWrite(targetPin, LOW); // write the buzzer pin low to pull back the diaphram
    delayMicroseconds(delayValue); // wait again or the calculated delay value
  }   
}
void playCompleteSound()
{  
  //http://www.princetronics.com/?p=473
  int size = sizeof(melody) / sizeof(int);
  for (int thisNote = 0; thisNote < size; thisNote++) {

    // to calculate the note duration, take one second
    // divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int noteDuration = 1000 / tempo[thisNote];

    buzz(pinBuzzer, melody[thisNote], noteDuration);

    // to distinguish the notes, set a minimum time between them.
    // the note's duration + 30% seems to work well:
    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);

    // stop the tone playing:
    buzz(pinBuzzer, 0, noteDuration);
  }
}
