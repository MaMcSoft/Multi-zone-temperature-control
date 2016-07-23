#include <Arduino.h>

#include <Key.h>
#include <Keypad.h>
#include <LiquidCrystal.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_MAX31855.h>
#include <EEPROM.h>

////////////////////////////
///    MAX31855 Setup   ///
///////////////////////////
#define MAXDO   14
#define MAXCS   15
#define MAXCLK  16

Adafruit_MAX31855 TC1(MAXCLK, MAXCS, MAXDO);
Adafruit_MAX31855 TC2(MAXCLK, MAXCS, MAXDO);
Adafruit_MAX31855 TC3(MAXCLK, MAXCS, MAXDO);
Adafruit_MAX31855 TC4(MAXCLK, MAXCS, MAXDO);
Adafruit_MAX31855 TC5(MAXCLK, MAXCS, MAXDO);
Adafruit_MAX31855 TC6(MAXCLK, MAXCS, MAXDO);

////////////////////////////
///      LCD Setup      ///
///////////////////////////
LiquidCrystal lcd(12, 13, 11, 10, 9, 8);

byte arrow[8] = {
  B00000,
  B00100,
  B00010,
  B11111,
  B00010,
  B00100,
  B00000,
};

byte grades[8] = {
  B00100,
  B01010,
  B00100,
  B00000,
  B00000,
  B00000,
  B00000,
};

////////////////////////////
///    Keypad Setup     ///
///////////////////////////
const byte ROWS = 4; // Four rows
const byte COLS = 4; // Three columns
// Define the Keymap
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
// Connect keypad ROW0, ROW1, ROW2 and ROW3 to these Arduino pins.
byte rowPins[ROWS] = { 4, 5, 6, 7 };
// Connect keypad COL0, COL1 and COL2 to these Arduino pins.
byte colPins[COLS] = { 0, 1, 2, 3 };

// Create the Keypad
Keypad kpd = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

////////////////////////////
///  GLOBAL VARIABLES   ///
///////////////////////////
#define BOTTOM_MESSAGE "  www.rihesa.com.mx" //Bottom display text
#define DISP_BOTTOM_MSG true //True to show the message false to hide it
#define TIMEOUT 5000 //Time of inactivity to get back to idle in milliseconds

#define AMOUNT_ZONES 6 //Defines the amount of zones
int ZONES[AMOUNT_ZONES][2] = {
  {0,0},{0,1},{0,2}/*,{0,3}*/,{10,0},{10,1},{10,2}/*,{10,3}*/ //Screen position of the zone (x,y)
};
int ACT_ZONE = -1;
#define AMOUNT_MENUS 4 //Defines the amount of menus per zone
char MENUS[AMOUNT_MENUS] = {'Z','R','S','E'}; //'Z' zone, 'R' range, 'S' set, 'E' enable
int RANGE_MENU_POS[4] = {5,6,8,9};
char STATES[4] = {'I','N','W','S'}; //'I' idle, 'N' navigating, 'W' writing, 'S' saving
char CURRENT_STATE = 'I';
int ACT_MENU[AMOUNT_ZONES] = {};
int ACT_TEMP[AMOUNT_ZONES] = {};
int PREV_TEMP[AMOUNT_ZONES] = {};

int rangePos = -1;
int setPos = 3;

////////////////////////////
///    Relays Setup     ///
///////////////////////////
int RELAY[AMOUNT_ZONES] = {
  18, //Zone1
  17, //Zone2
  22, //Zone3
  23, //Zone4
  24, //Zone5
  25  //Zone6
};

////////////////////////////
///   EEPROM Variables  ///
///////////////////////////
#define PARAMS 4
int SETTINGS[AMOUNT_ZONES][PARAMS] = {
//{R-,R+,S,E}
  {05,05,100,1},
  {05,05,100,1},
  {05,05,100,1},
  {05,05,100,1},
  {05,05,100,1},
  {05,05,100,1}
};

////////////////////////////
///    Main Functions   ///
///////////////////////////
int prevTime = 0;
int actTime;
int lastTime = 0;
int interval = 2000;
int lastPulse = 0;
int pulse = 1000;
char input;

void setup() {
  //Serial.begin(9600);
  //clearEEPROM();
  //setEEPROM();
  // set up the LCD's number of columns and rows:
  lcd.begin(20,4);
  lcd.createChar(0, arrow);
  lcd.createChar(1, grades);
  pinMode(RELAY[0], OUTPUT);
  pinMode(RELAY[1], OUTPUT);
  PREV_TEMP[0] = TC1.readCelsius();
  PREV_TEMP[1] = TC2.readCelsius();
  PREV_TEMP[2] = TC3.readCelsius();
  PREV_TEMP[3] = TC4.readCelsius();
  PREV_TEMP[4] = TC5.readCelsius();
  PREV_TEMP[5] = TC6.readCelsius();
  lcdInit();
}

bool on = false;

void loop() {
  input = kpd.getKey();
  if(input) {
    keyInput(input);
  }
  actTime = millis();
  if(CURRENT_STATE != 'I'){
    if(actTime - prevTime > TIMEOUT) {
      stateReset();
    }
  }
  if(CURRENT_STATE == 'I') {
    if(actTime - lastTime > interval) {
      lastTime = actTime;
      ACT_TEMP[0] = TC1.readCelsius();
      ACT_TEMP[1] = TC2.readCelsius();
      ACT_TEMP[2] = TC3.readCelsius();
      ACT_TEMP[3] = TC4.readCelsius();
      ACT_TEMP[4] = TC5.readCelsius();
      ACT_TEMP[5] = TC6.readCelsius();
      for(int i = 0; i < AMOUNT_ZONES; i++) {
        if(ACT_TEMP[i] < SETTINGS[i][2] - SETTINGS[i][0]) {
          digitalWrite(RELAY[i], HIGH);
        } else if (ACT_TEMP[i] > SETTINGS[i][2] + SETTINGS[i][1]) {
          digitalWrite(RELAY[i], LOW);
        } else {
          for(int j = 0; j < 2; j++) {
            if(actTime - lastPulse > pulse) {
              lastPulse = actTime;
              digitalWrite(RELAY[i], !on);
              on = !on;
            }
          }
        }
        if(MENUS[ACT_MENU[i]] == 'Z') {
          if(ACT_TEMP[i] - PREV_TEMP[i] >= 1 || PREV_TEMP[i] - ACT_TEMP[i] >= 1) {
            PREV_TEMP[i] = ACT_TEMP[i];
            lcdUpdate(i, ACT_TEMP[i]);
          }
        } else {
          return;
        }
      }
    }
  }
}

////////////////////////////
///     State Manager   ///
///////////////////////////
void stateReset() {
  CURRENT_STATE = 'I';
  lcdInit();
  resetNav(-1);
}

////////////////////////////
///   EEPROM Functions  ///
///////////////////////////
void clearEEPROM() {
  for (int i = 0 ; i < EEPROM.length()-1 ; i++) {
    EEPROM.write(i, 0);
  }
}

void setEEPROM() {
  //Serial.println("Writing...");
  int add = 0;
  for(int z = 0; z < AMOUNT_ZONES; z++) {
    for(int p = 0; p < PARAMS; p++){
      EEPROM.put(add, SETTINGS[z][p]);
      //Serial.println("Az" + String(z) + "p" + String(p) + String(SETTINGS[z][p]));
      add += 2;
    }
  }
}

void readEPPROM() {
  //Serial.println("Reading...");
  int add = 0;
  for(int z = 0; z < AMOUNT_ZONES; z++) {
    for(int p = 0; p < PARAMS; p++){
      //Serial.println("Az" + String(z) + "p" + String(p) + String(EEPROM.read(add)));
      EEPROM.get(add, SETTINGS[z][p]);
      add += 2;
    }
  }
}

////////////////////////////
///  LCD Initialization ///
///////////////////////////
void lcdInit() {
  lcd.clear();
  readEPPROM();
  resetNav(-1);
  for (int i = 0; i < AMOUNT_ZONES; i++) {
    String prefix = " Z";
    int actZone = i+1;
    int temp = PREV_TEMP[i];
    String dummy = " ";
    char units = 'C';
    String text = String(prefix + actZone + dummy + temp);
    lcd.noBlink();
    lcd.setCursor(ZONES[i][0], ZONES[i][1]);
    lcd.print(text);
    lcd.setCursor(ZONES[i][0]+8, ZONES[i][1]);
    lcd.write(byte(1));
    lcd.print(units);
    if(DISP_BOTTOM_MSG){
      lcd.setCursor(0, 3);
      lcd.print(BOTTOM_MESSAGE);
    }
  }
}

////////////////////////////
///      LCD Write      ///
///////////////////////////
void lcdUpdate(int zone, int temp) {
  switch (MENUS[ACT_MENU[zone]]) {
    case 'Z':
      lcd.setCursor(ZONES[zone][0]+4, ZONES[zone][1]);
      lcd.print(temp);
    break;
  }
}

////////////////////////////
///      Navigation     ///
///////////////////////////
int pos = -1;

void resetNav(int newPos) {
  lcd.noBlink();
  pos = newPos;
  setPos = 3;
  rangePos = -1;
  ACT_ZONE = -1;
  for(int i = 0; i < AMOUNT_ZONES; i++) {
    ACT_MENU[i] = 0;
  }
}

void selectUp() {
  for (pos; pos >= -1;) {
    if(pos <= -1) {
      pos = AMOUNT_ZONES-1;
      ACT_ZONE = pos;
      lcd.setCursor(ZONES[pos][0], ZONES[pos][1]);
      lcd.write(byte(0));
      return;
    }
    if(pos == 0) {
      lcd.setCursor(ZONES[pos][0], ZONES[pos][1]);
      lcd.print(" ");
      pos = AMOUNT_ZONES;
    } else {
      if(pos > -1 && pos < 8) {
        lcd.setCursor(ZONES[pos][0], ZONES[pos][1]);
        lcd.print(" ");
      }
      pos--;
      ACT_ZONE = pos;
      lcd.setCursor(ZONES[pos][0], ZONES[pos][1]);
      lcd.write(byte(0));
      return;
    }
  }
}

void selectDown() {
  for (pos; pos < AMOUNT_ZONES;) {
    if(pos == AMOUNT_ZONES-1) {
      lcd.setCursor(ZONES[pos][0], ZONES[pos][1]);
      lcd.print(" ");
      pos = -1;
    } else {
      if(pos > -1) {
        lcd.setCursor(ZONES[pos][0], ZONES[pos][1]);
        lcd.print(" ");
      }
      pos++;
      ACT_ZONE = pos;
      lcd.setCursor(ZONES[pos][0], ZONES[pos][1]);
      lcd.write(byte(0));
      return;
    }
  }
}

void enterMenu(int zone, char menu) {
  switch (menu) {
    case 'R':
      for(rangePos; rangePos < 4;) {
        rangePos++;
        if(rangePos == 4) {
          rangePos = 0;
          lcd.setCursor(RANGE_MENU_POS[rangePos], ZONES[zone][1]);
        } else {
          lcd.setCursor(RANGE_MENU_POS[rangePos], ZONES[zone][1]);
        }
        lcd.blink();
        return;
      }
    break;
    case 'S':
      for(setPos; setPos < 7;) {
        setPos++;
        if(setPos == 7) {
          setPos = 4;
          lcd.setCursor(ZONES[zone][0] + setPos, ZONES[zone][1]);
        } else {
          lcd.setCursor(ZONES[zone][0] + setPos, ZONES[zone][1]);
        }
        lcd.blink();
        return;
      }
    break;
    case 'E':
    break;
  }
}

void swapMenu(int zone) {
  if(zone > -1) {
    for (ACT_MENU[zone]; ACT_MENU[zone] < AMOUNT_MENUS;) {
      ACT_MENU[zone] += 1;
      String dummy;
      String plus = "+";
      String minus = " -";
      String text;
      String setTemp;
      String minTemp;
      String maxTemp;
      String oneCero = "0";
      String twoCero = "00";
      String threeCero = "000";
      char units = 'C';
      int temp = ACT_TEMP[zone];
      if(ACT_MENU[zone] >= AMOUNT_MENUS) {
        ACT_MENU[zone] = 0;
        String prefix = String(MENUS[ACT_MENU[zone]]);
        int zoneNum = zone+1;
        switch (MENUS[ACT_MENU[zone]]) {
          case 'Z':
            dummy = " ";
            text = String(prefix + zoneNum + dummy + temp + dummy);
            lcd.setCursor(ZONES[zone][0]+1, ZONES[zone][1]);
            lcd.print(text);
            lcd.setCursor(ZONES[zone][0]+8, ZONES[zone][1]);
            lcd.write(byte(1));
            lcd.print(units);
          break;
          case 'R':
            if(SETTINGS[zone][0] >= 10) {
              minTemp = SETTINGS[zone][0];
            } else if(SETTINGS[zone][0] > 0) {
              minTemp = String(oneCero + SETTINGS[zone][0]);
            } else {
              minTemp = twoCero;
            }
            if(SETTINGS[zone][1] >= 10) {
              maxTemp = SETTINGS[zone][1];
            } else if(SETTINGS[zone][1] > 0) {
              maxTemp = String(oneCero + SETTINGS[zone][1]);
            } else {
              maxTemp = twoCero;
            }
            text = String(prefix + zoneNum + minus + minTemp + plus + maxTemp);
            lcd.setCursor(ZONES[zone][0]+1, ZONES[zone][1]);
            lcd.print(text);
          break;
          case 'S':
            if(SETTINGS[zone][2] >= 100) {
              setTemp = SETTINGS[zone][2];
            } else if (SETTINGS[zone][2] >= 10){
              setTemp = String(oneCero+ SETTINGS[zone][2]);
            } else if(SETTINGS[zone][2] > 0) {
              setTemp = String(twoCero + SETTINGS[zone][2]);
            } else {
              setTemp = threeCero;
            }
            dummy = " ";
            text = String(prefix + zoneNum + dummy + setTemp + dummy);
            lcd.setCursor(ZONES[zone][0]+1, ZONES[zone][1]);
            lcd.print(text);
            lcd.setCursor(ZONES[zone][0]+8, ZONES[zone][1]);
            lcd.write(byte(1));
            lcd.print(units);
          break;
          case 'E':
            dummy = " OFF ";
            text = String(prefix + zoneNum + dummy);
            lcd.setCursor(ZONES[zone][0]+1, ZONES[zone][1]);
            lcd.print(text);
          break;
        }
        lcd.setCursor(ZONES[zone][0]+1, ZONES[zone][1]);
        lcd.print(text);
        return;
      } else {
        String prefix = String(MENUS[ACT_MENU[zone]]);
        int zoneNum = zone+1;
        switch (MENUS[ACT_MENU[zone]]) {
          case 'Z':
            dummy = " ";
            text = String(prefix + zoneNum + dummy + temp + dummy);
            lcd.setCursor(ZONES[zone][0]+1, ZONES[zone][1]);
            lcd.print(text);
            lcd.setCursor(ZONES[zone][0]+8, ZONES[zone][1]);
            lcd.write(byte(1));
            lcd.print(units);
          break;
          case 'R':
            if(SETTINGS[zone][0] >= 10) {
              minTemp = SETTINGS[zone][0];
            } else if(SETTINGS[zone][0] > 0) {
              minTemp = String(oneCero + SETTINGS[zone][0]);
            } else {
              minTemp = twoCero;
            }
            if(SETTINGS[zone][1] >= 10) {
              maxTemp = SETTINGS[zone][1];
            } else if(SETTINGS[zone][1] > 0) {
              maxTemp = String(oneCero + SETTINGS[zone][1]);
            } else {
              maxTemp = twoCero;
            }
            text = String(prefix + zoneNum + minus + minTemp + plus + maxTemp);
            lcd.setCursor(ZONES[zone][0]+1, ZONES[zone][1]);
            lcd.print(text);
          break;
          case 'S':
            if(SETTINGS[zone][2] >= 100) {
              setTemp = SETTINGS[zone][2];
            } else if(SETTINGS[zone][2] >= 10) {
              setTemp = String(oneCero + SETTINGS[zone][2]);
            } else if(SETTINGS[zone][2] > 0) {
              setTemp = String(twoCero + SETTINGS[zone][2]);
            } else {
              setTemp = threeCero;
            }
            dummy = " ";
            text = String(prefix + zoneNum + dummy + setTemp + dummy);
            lcd.setCursor(ZONES[zone][0]+1, ZONES[zone][1]);
            lcd.print(text);
            lcd.setCursor(ZONES[zone][0]+8, ZONES[zone][1]);
            lcd.write(byte(1));
            lcd.print(units);
          break;
          case 'E':
            dummy = " OFF ";
            text = String(prefix + zoneNum + dummy);
            lcd.setCursor(ZONES[zone][0]+1, ZONES[zone][1]);
            lcd.print(text);
          break;
        }
        return;
      }
    }
  }
}

////////////////////////////
///       Writing       ///
///////////////////////////
void resetWriting() {
  lcd.noBlink();
  setPos = 3;
  rangePos = -1;
}

void writeKey(int key, char menu, int zone) {
  switch (menu) {
    case 'R':
      for(rangePos; rangePos < 4;) {
        lcd.print(key);
        switch(RANGE_MENU_POS[rangePos]) {
          case 5:
            SETTINGS[zone][0] = key*10;
          break;
          case 6:
            SETTINGS[zone][0] += key;
          break;
          case 8:
            SETTINGS[zone][1] = key*10;
          break;
          case 9:
            SETTINGS[zone][1] += key;
          break;
        }
        rangePos++;
        if(rangePos == 4) {
          rangePos = 0;
          lcd.setCursor(RANGE_MENU_POS[rangePos], ZONES[zone][1]);
        } else {
          lcd.setCursor(RANGE_MENU_POS[rangePos], ZONES[zone][1]);
        }
        return;
      }
    break;
    case 'S':
      for(setPos; setPos < 7;) {
        lcd.print(key);
        switch(setPos) {
          case 4:
            SETTINGS[zone][2] = key*100;
          break;
          case 5:
            SETTINGS[zone][2] += key*10;
          break;
          case 6:
            SETTINGS[zone][2] += key;
          break;
        }
        setPos++;
        if(setPos == 7) {
          setPos = 4;
          lcd.setCursor(ZONES[zone][0] + setPos, ZONES[zone][1]);
        } else {
          lcd.setCursor(ZONES[zone][0] + setPos, ZONES[zone][1]);
        }
        return;
      }
    break;
    case 'E':
    break;
  }
}

void saveSettings() {
  lcd.clear();
  lcd.setCursor(5, 1);
  lcd.print("Saving...");
  setEEPROM();
}

////////////////////////////
///     Keypad Input    ///
///////////////////////////
void keyInput(char key) {
  prevTime = millis();
  switch (key) {
    case '1':
      if (CURRENT_STATE == 'W') {
        writeKey(1, MENUS[ACT_MENU[ACT_ZONE]], ACT_ZONE);
        CURRENT_STATE = 'W';
      }
    break;
    case '2':
      if (CURRENT_STATE == 'W') {
        writeKey(2, MENUS[ACT_MENU[ACT_ZONE]], ACT_ZONE);
        CURRENT_STATE = 'W';
      }
    break;
    case '3':
      if (CURRENT_STATE == 'W') {
        writeKey(3, MENUS[ACT_MENU[ACT_ZONE]], ACT_ZONE);
        CURRENT_STATE = 'W';
      }
    break;
    case 'A':
      if(CURRENT_STATE == 'I' || CURRENT_STATE == 'N') {
        selectUp();
        CURRENT_STATE = 'N';
      }
    break;
    case '4':
      if (CURRENT_STATE == 'W') {
        writeKey(4, MENUS[ACT_MENU[ACT_ZONE]], ACT_ZONE);
        CURRENT_STATE = 'W';
      }
    break;
    case '5':
      if (CURRENT_STATE == 'W') {
        writeKey(5, MENUS[ACT_MENU[ACT_ZONE]], ACT_ZONE);
        CURRENT_STATE = 'W';
      }
    break;
    case '6':
      if (CURRENT_STATE == 'W') {
        writeKey(6, MENUS[ACT_MENU[ACT_ZONE]], ACT_ZONE);
      }
    break;
    case 'B':
      if(CURRENT_STATE == 'I' || CURRENT_STATE == 'N') {
        selectDown();
        CURRENT_STATE = 'N';
      }
    break;
    case '7':
      if (CURRENT_STATE == 'W') {
        writeKey(7, MENUS[ACT_MENU[ACT_ZONE]], ACT_ZONE);
        CURRENT_STATE = 'W';
      }
    break;
    case '8':
      if (CURRENT_STATE == 'W') {
        writeKey(8, MENUS[ACT_MENU[ACT_ZONE]], ACT_ZONE);
        CURRENT_STATE = 'W';
      }
    break;
    case '9':
      if (CURRENT_STATE == 'W') {
        writeKey(9, MENUS[ACT_MENU[ACT_ZONE]], ACT_ZONE);
        CURRENT_STATE = 'W';
      }
    break;
    case 'C':
      if(CURRENT_STATE == 'N' || CURRENT_STATE == 'W') {
        if(CURRENT_STATE == 'W') {
          resetWriting();
          swapMenu(ACT_ZONE);
          CURRENT_STATE = 'N';
        } else {
          swapMenu(ACT_ZONE);
          CURRENT_STATE = 'N';
        }
      }
    break;
    case '*':
    break;
    case '0':
      if (CURRENT_STATE == 'W') {
        writeKey(0, MENUS[ACT_MENU[ACT_ZONE]], ACT_ZONE);
        CURRENT_STATE = 'W';
      }
    break;
    case '#':
      if(CURRENT_STATE == 'W' || CURRENT_STATE == 'N') {
        saveSettings();
      }
    break;
    case 'D':
      if((CURRENT_STATE == 'N') && MENUS[ACT_MENU[ACT_ZONE]] != 'Z') {
        enterMenu(ACT_ZONE, MENUS[ACT_MENU[ACT_ZONE]]);
        CURRENT_STATE = 'W';
      }
    break;
  }
}
