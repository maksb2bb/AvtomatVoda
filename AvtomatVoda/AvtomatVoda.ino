#define _LCD_TYPE 1
#include <Wire.h>
#include <EEPROM.h>
#include <I2C_LiquidCrystal_RUS.h>
#include <DHT.h>

I2C_LiquidCrystal_RUS lcd(0x27, 16, 2);

// Buttons
#define ButtonOK 12
#define ButtonBACK 14
#define ButtonNEXT 26
#define ButtonMENU 27
#define ButtonUSER 15

// Device pins
#define RELE 18
#define TEMP_RELE 5
#define DHTPIN 25
#define COIN_PIN 33
#define BILL_PIN 32
#define SENSOR_PIN 2

// EEPROM
#define EEPROM_SIZE 256
#define EEPROM_TOTAL_MONEY_ADDR 0
#define EEPROM_TOTAL_LITERS_ADDR 8
#define EEPROM_PRICE_ADDR 16
#define EEPROM_TEMP_ON_ADDR 20
#define EEPROM_TEMP_OFF_ADDR 24
#define EEPROM_ML_PER_PULSE_ADDR 28

// Settings
#define MIN_TEMP_ON 0
#define MAX_TEMP_ON 20
#define MIN_TEMP_OFF 15
#define MAX_TEMP_OFF 30
#define PAUSE_DURATION 1000
#define CALIBRATION_FACTOR 7.5
#define PAUSE_TIMEOUT 300000 
#define CALIBRATION_VOLUME 5000
#define MONEY_DISPLAY_TIME 30000 

DHT dht(DHTPIN, DHT11);

enum status {
  inactive,
  Menu,
  EditPrice,
  EditTemperatures,
  ShowTotalMoney,
  ShowTotalLiters,
  ShowStatistics,
  ResetStatistics,
  Calibration
};

// Global variables
volatile bool criticalOperation = false;
volatile bool eepromUpdateNeeded = false;
status currentStatus = inactive;
unsigned long menutimebutton = 0;
const int long_press_menuButton = 2000;
int currentMenuIndex = 0;
const int NUM_MENU_ITEMS = 8;
String menuItems[NUM_MENU_ITEMS] = {
  "Statistics",
  "Set price",
  "Temperature",
  "Calibration",
  "Total money",
  "Total liters",
  "Reset stats",
  "Exit"
};

// Settings
float price = 10.00;
int tempOn = 18;
int tempOff = 22;
float mlPerPulse = 0.1;

// Editing variables
bool editingRubles = true;
int editRubles = 0;
int editKopecks = 0;
bool editingTempOn = false;

// System variables
volatile unsigned long pulseCount = 0;
volatile float availableLiters = 0.0;
volatile float waterDispensedLiters = 0.0;
volatile unsigned int sessionMoney = 0;
unsigned long totalMoney = 0;
unsigned long totalLiters = 0;
volatile bool dispensing = false;
volatile bool pauseMode = false;
unsigned long pauseStartTime = 0;
bool isRelayOn = false;
unsigned long lastPulseTime = 0;
unsigned long lastTempCheck = 0;
float currentTemp = 0;
bool needRedraw = true;
unsigned long lastUpdateTime = 0;
const uint16_t SCREEN_UPDATE_INTERVAL = 100;
bool showingMoneyInfo = false;
unsigned long lastMoneyInsertTime = 0;
volatile bool showNoMoneyMessage = false;
volatile unsigned long targetPulseCount = 0; 
volatile bool calibrationRequest = false;
volatile bool calibrationStopRequest = false;
volatile unsigned long calibrationPulseCount = 0;
bool isCalibrating = false;
unsigned long calibrationStartTime = 0;
volatile unsigned long calibrationPulses = 0;
volatile bool calibrationRunning = false;
unsigned long lastPulseReceivedTime = 0;

enum calibrationState { CALIB_IDLE, CALIB_STARTED, CALIB_COMPLETED };
calibrationState calibState = CALIB_IDLE;
unsigned long calibrationStartPulses = 0;
const float CALIBRATION_LITERS = 5.0;

// Debounce
volatile unsigned long lastCoinInterruptTime = 0;
volatile unsigned long lastBillInterruptTime = 0;
volatile unsigned long lastUserButtonPress = 0;
const unsigned long debounceDelay = 300;

void IRAM_ATTR pulseCounter() {
  static unsigned long lastPulseTime = 0;
  unsigned long now = micros();
  
  if (now - lastPulseTime > 5000) { 
    if (isCalibrating) {
      calibrationPulses++;
    } else if (dispensing) {
      pulseCount++;
      lastPulseReceivedTime = millis();
      Serial.print("Pulse: ");
      Serial.println(pulseCount);
    }
    lastPulseTime = now;
  }
}

void IRAM_ATTR coinInserted() {
  noInterrupts();
  unsigned long interruptTime = millis();
  if (interruptTime - lastCoinInterruptTime > debounceDelay) {
    totalMoney++;
    sessionMoney++;
    availableLiters = sessionMoney / price;
    targetPulseCount = (availableLiters * 1000) / mlPerPulse;
    lastMoneyInsertTime = millis();
    showingMoneyInfo = true;
    needRedraw = true;
    eepromUpdateNeeded = true;
    Serial.print("Coin inserted, money: ");
    Serial.print(sessionMoney);
    Serial.print(", liters: ");
    Serial.println(availableLiters, 2);
  }
  lastCoinInterruptTime = interruptTime;
  interrupts();
}

void IRAM_ATTR billInserted() {
  noInterrupts();
  unsigned long interruptTime = millis();
  if (interruptTime - lastBillInterruptTime > debounceDelay) {
    totalMoney += 10;
    sessionMoney += 10;
    availableLiters = sessionMoney / price;
    targetPulseCount = (availableLiters * 1000) / mlPerPulse;
    lastMoneyInsertTime = millis();
    showingMoneyInfo = true;
    needRedraw = true;
    eepromUpdateNeeded = true;
    Serial.print("Bill inserted, money: ");
    Serial.print(sessionMoney);
    Serial.print(", liters: ");
    Serial.println(availableLiters, 2);
  }
  lastBillInterruptTime = interruptTime;
  interrupts();
}

void IRAM_ATTR userButtonPressed() {
  static unsigned long lastPress = 0;
  unsigned long now = millis();
  
  if (now - lastPress > debounceDelay) {
    noInterrupts();
    if (digitalRead(ButtonUSER) == HIGH) {
      Serial.println("USER button: False trigger detected");
      lastPress = now;
      interrupts();
      return;
    }
    delay(10);
    if (digitalRead(ButtonUSER) != LOW) {
      Serial.println("USER button: Bounce detected");
      lastPress = now;
      interrupts();
      return;
    }
    
    Serial.print("USER button pressed, state: ");
    Serial.print(currentStatus);
    Serial.print(", moneyInfo: ");
    Serial.print(showingMoneyInfo);
    Serial.print(", dispensing: ");
    Serial.print(dispensing);
    Serial.print(", availableLiters: ");
    Serial.print(availableLiters, 2);
    Serial.print(", lastMoneyInsert: ");
    Serial.println(lastMoneyInsertTime);
    
    if (showingMoneyInfo && currentStatus == inactive) {
      Serial.println("USER button ignored: money insertion in progress");
      lastPress = now;
      interrupts();
      return;
    }
    
    if (!isCalibrating && currentStatus == Calibration) {
      calibrationRequest = true;
      Serial.println("Calibration requested");
    } else if (isCalibrating) {
      calibrationStopRequest = true;
      Serial.println("Calibration stop requested");
    } else if (currentStatus == inactive && availableLiters > 0 && !dispensing) {
      dispensing = true;
      digitalWrite(RELE, LOW);
      isRelayOn = true;
      waterDispensedLiters = 0;
      pulseCount = 0;
      pauseMode = false;
      needRedraw = true;
      Serial.println("Dispensing started");
    } else if (currentStatus == inactive && availableLiters <= 0 && !dispensing) {
      showNoMoneyMessage = true;
      needRedraw = true;
      Serial.println("No funds");
    }
    lastPress = now;
    interrupts();
  }
}

void handleWaterDispensing() {
  static unsigned long lastPulseCheck = 0;
  unsigned long now = millis();
  
  if (now - lastPulseCheck > 100) {
    noInterrupts();
    criticalOperation = true;
    
    unsigned long currentPulses = pulseCount;
    pulseCount = 0;
    
    criticalOperation = false;
    interrupts();
    
    if (dispensing && currentPulses > 0) {
      waterDispensedLiters += (currentPulses * mlPerPulse) / 1000.0;
      Serial.print("Dispensed: ");
      Serial.print(waterDispensedLiters, 2);
      Serial.print("L/");
      Serial.println(availableLiters, 2);
      needRedraw = true;
      
      if (waterDispensedLiters >= availableLiters - 0.01) {
        dispensing = false;
        digitalWrite(RELE, HIGH);
        isRelayOn = false;
        totalLiters += waterDispensedLiters;
        eepromUpdateNeeded = true;
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Налив завершен");
        lcd.setCursor(0, 1);
        lcd.print(String(waterDispensedLiters, 1) + " л налито");
        Serial.print("Dispensing done: ");
        Serial.print(waterDispensedLiters, 2);
        Serial.println("L");
        delay(2000);
        
        availableLiters = 0;
        sessionMoney = 0;
        waterDispensedLiters = 0;
        needRedraw = true;
      }
    }
    
    lastPulseCheck = now;
  }
}

void checkWaterSensor() {
  static int lastState = HIGH;
  static unsigned long lastDebounceTime = 0;
  
  int reading = digitalRead(SENSOR_PIN);
  
  if (reading != lastState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > 50) {
    if (reading == LOW && lastState == HIGH) {
      pulseCount++;
      lastPulseTime = micros();
    }
    lastState = reading;
  }
}

void safeEEPROMUpdate() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 10000 && eepromUpdateNeeded) {
    noInterrupts();
    criticalOperation = true;
    EEPROM.put(EEPROM_TOTAL_LITERS_ADDR, totalLiters);
    EEPROM.put(EEPROM_TOTAL_MONEY_ADDR, totalMoney);
    EEPROM.put(EEPROM_PRICE_ADDR, price);
    EEPROM.put(EEPROM_TEMP_ON_ADDR, tempOn);
    EEPROM.put(EEPROM_TEMP_OFF_ADDR, tempOff);
    EEPROM.put(EEPROM_ML_PER_PULSE_ADDR, mlPerPulse);
    EEPROM.commit();
    criticalOperation = false;
    eepromUpdateNeeded = false;
    interrupts();
    lastUpdate = millis();
    Serial.println("EEPROM updated");
  }
}

void loadSettings() {
  EEPROM.get(EEPROM_TOTAL_MONEY_ADDR, totalMoney);
  EEPROM.get(EEPROM_TOTAL_LITERS_ADDR, totalLiters);
  EEPROM.get(EEPROM_PRICE_ADDR, price);
  EEPROM.get(EEPROM_TEMP_ON_ADDR, tempOn);
  EEPROM.get(EEPROM_TEMP_OFF_ADDR, tempOff);
  EEPROM.get(EEPROM_ML_PER_PULSE_ADDR, mlPerPulse);

  if (isnan(mlPerPulse) || mlPerPulse <= 0) mlPerPulse = 0.1;
  if (isnan(price) || price <= 0) price = 10.00;
  if (totalMoney < 0) totalMoney = 0;
  if (totalLiters < 0) totalLiters = 0;
  
  tempOn = constrain(tempOn, MIN_TEMP_ON, MAX_TEMP_ON);
  tempOff = constrain(tempOff, MIN_TEMP_OFF, MAX_TEMP_OFF);
  if (tempOn >= tempOff) {
    tempOn = tempOff - 2;
    if (tempOn < MIN_TEMP_ON) tempOn = MIN_TEMP_ON;
  }
  Serial.println("Settings loaded");
}

void checkTemperature() {
  float t = dht.readTemperature();
  if (!isnan(t)) {
    currentTemp = t;
    if (currentStatus == inactive) needRedraw = true;

    if (t < tempOn && !isRelayOn) {
      digitalWrite(TEMP_RELE, HIGH); 
      isRelayOn = true;
    } else if (t > tempOff && isRelayOn) {
      digitalWrite(TEMP_RELE, LOW); 
      isRelayOn = false;
    }
  }
}

void showInactive() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Готово");
  lcd.setCursor(0, 1);
  lcd.print("Цена: " + String(price, 2) + " р/л");
}

void updateDisplay() {
  static bool lastDispensingState = false;
  static float lastAvailableLiters = 0;
  static float lastWaterDispensed = 0;
  
  bool needUpdate = needRedraw || dispensing || (dispensing != lastDispensingState) || 
                   (abs(availableLiters - lastAvailableLiters) > 0.01) ||
                   (abs(waterDispensedLiters - lastWaterDispensed) > 0.01);
  
  if (!needUpdate) return;

  if (dispensing) {
    lcd.setCursor(0, 1);
    String dispStr = String(waterDispensedLiters, 1) + "/" + String(availableLiters, 1) + "л";
    if (millis() - lastPulseReceivedTime < 1000) {
      dispStr += " *";
    } else {
      dispStr += "  ";
    }
    lcd.print(dispStr);
  } else {
    lcd.clear();
    if (pauseMode) {
      unsigned long remaining = (PAUSE_TIMEOUT - (millis() - pauseStartTime)) / 1000;
      lcd.setCursor(0, 0);
      lcd.print("Пауза (осталось)");
      lcd.setCursor(0, 1);
      lcd.print(String(remaining) + " сек до сброса");
    } else if (showingMoneyInfo) {
      lcd.setCursor(0, 0);
      lcd.print("Внесено: " + String(sessionMoney) + "р");
      lcd.setCursor(0, 1);
      lcd.print("Доступно: " + String(availableLiters, 1) + "л");
    } else if (showNoMoneyMessage) {
      lcd.setCursor(0, 0);
      lcd.print("Внесите средства");
      showNoMoneyMessage = false;
    } else {
      switch(currentStatus) {
        case inactive:
          showInactive();
          if (availableLiters > 0) {
            lcd.setCursor(10, 1);
            lcd.print(String(availableLiters, 1) + " л");
          }
          break;
          
        case Menu:
          lcd.setCursor(0, 0);
          lcd.print("> " + menuItems[currentMenuIndex]);
          break;
          
        case EditPrice:
          lcd.setCursor(0, 0);
          lcd.print("Цена за литр:");
          lcd.setCursor(0, 1);
          if (editingRubles) {
            lcd.print(">");
            lcd.print(String(editRubles));
            lcd.print("<");
            lcd.print(".");
            if (editKopecks < 10) lcd.print("0");
            lcd.print(String(editKopecks));
          } else {
            lcd.print(String(editRubles));
            lcd.print(".");
            lcd.print(">");
            if (editKopecks < 10) lcd.print("0");
            lcd.print(String(editKopecks));
            lcd.print("<");
          }
          break;
          
        case EditTemperatures:
          lcd.setCursor(0, 0);
          lcd.print("Вкл t: " + String(tempOn));
          lcd.setCursor(0, 1);
          lcd.print("Выкл t: " + String(tempOff));
          if (editingTempOn) {
            lcd.setCursor(6, 0);
            lcd.print(">");
            lcd.setCursor(8 + String(tempOn).length(), 0);
            lcd.print("<");
          } else {
            lcd.setCursor(6, 1);
            lcd.print(">");
            lcd.setCursor(8 + String(tempOff).length(), 1);
            lcd.print("<");
          }
          break;
          
        case Calibration:
          handleCalibration();
          break;
      }
    }
  }
  
  lastDispensingState = dispensing;
  lastAvailableLiters = availableLiters;
  lastWaterDispensed = waterDispensedLiters;
  needRedraw = false;
}

void handleSystemState() {
  switch (currentStatus) {
    case inactive: handleIdle(); break;
    case Menu: handleMenu(); break;
    case EditPrice: handleEditPrice(); break;
    case EditTemperatures: handleEditTemperatures(); break;
    case ShowTotalMoney: showTotalMoney(); break;
    case ShowTotalLiters: showTotalLiters(); break;
    case ShowStatistics: showStatistics(); break;
    case ResetStatistics: confirmReset(); break;
    case Calibration: handleCalibration(); break;
  }
}

void handleIdle() {
  if (digitalRead(ButtonMENU) == LOW) {
    delay(50);
    if (digitalRead(ButtonMENU) == LOW) {
      currentStatus = Menu;
      needRedraw = true;
    }
  }
}

void handleMenu() {
  static unsigned long lastButtonPress = 0;
  if (millis() - lastButtonPress < 200) return;

  if (digitalRead(ButtonNEXT) == LOW) {
    currentMenuIndex = (currentMenuIndex + 1) % NUM_MENU_ITEMS;
    needRedraw = true;
    lastButtonPress = millis();
  }

  if (digitalRead(ButtonBACK) == LOW) {
    currentMenuIndex = (currentMenuIndex - 1 + NUM_MENU_ITEMS) % NUM_MENU_ITEMS;
    needRedraw = true;
    lastButtonPress = millis();
  }

  if (digitalRead(ButtonOK) == LOW) {
    switch (currentMenuIndex) {
      case 0: currentStatus = ShowStatistics; break;
      case 1: 
        currentStatus = EditPrice; 
        editRubles = (int)price;
        editKopecks = (price - editRubles) * 100;
        editingRubles = true;
        break;
      case 2: currentStatus = EditTemperatures; break;
      case 3: currentStatus = Calibration; break;
      case 4: currentStatus = ShowTotalMoney; break;
      case 5: currentStatus = ShowTotalLiters; break;
      case 6: currentStatus = ResetStatistics; break;
      case 7: currentStatus = inactive; break;
    }
    needRedraw = true;
    lastButtonPress = millis();
  }
}

void handleEditPrice() {
  static unsigned long lastButtonPress = 0;
  if (millis() - lastButtonPress < 200) return;

  if (digitalRead(ButtonMENU) == LOW) {
    editingRubles = !editingRubles;
    needRedraw = true;
    lastButtonPress = millis();
  }

  if (digitalRead(ButtonNEXT) == LOW) {
    if (editingRubles) editRubles = min(editRubles + 1, 999);
    else editKopecks = min(editKopecks + 1, 99);
    needRedraw = true;
    lastButtonPress = millis();
  }

  if (digitalRead(ButtonBACK) == LOW) {
    if (editingRubles) editRubles = max(editRubles - 1, 0);
    else editKopecks = max(editKopecks - 1, 0);
    needRedraw = true;
    lastButtonPress = millis();
  }

  if (digitalRead(ButtonOK) == LOW) {
    price = editRubles + (editKopecks / 100.0);
    EEPROM.put(EEPROM_PRICE_ADDR, price);
    EEPROM.commit();
    currentStatus = Menu;
    needRedraw = true;
    lastButtonPress = millis();
  }
}

void handleEditTemperatures() {
  static unsigned long lastButtonPress = 0;
  if (millis() - lastButtonPress < 200) return;

  if (digitalRead(ButtonMENU) == LOW) {
    editingTempOn = !editingTempOn;
    needRedraw = true;
    lastButtonPress = millis();
  }

  if (digitalRead(ButtonNEXT) == LOW) {
    if (editingTempOn) tempOn = min(tempOn + 1, tempOff - 1);
    else tempOff = min(tempOff + 1, MAX_TEMP_OFF);
    needRedraw = true;
    lastButtonPress = millis();
  }

  if (digitalRead(ButtonBACK) == LOW) {
    if (editingTempOn) tempOn = max(tempOn - 1, MIN_TEMP_ON);
    else tempOff = max(tempOff - 1, tempOn + 1);
    needRedraw = true;
    lastButtonPress = millis();
  }

  if (digitalRead(ButtonOK) == LOW) {
    EEPROM.put(EEPROM_TEMP_ON_ADDR, tempOn);
    EEPROM.put(EEPROM_TEMP_OFF_ADDR, tempOff);
    EEPROM.commit();
    currentStatus = Menu;
    needRedraw = true;
    lastButtonPress = millis();
  }
}

void handleCalibration() {
  static enum { CALIB_IDLE, CALIB_RUNNING, CALIB_DONE } state = CALIB_IDLE;
  static unsigned long startPulses = 0;

  switch (state) {
    case CALIB_IDLE:
      Serial.println("Calibration: Idle");
      if (calibrationRequest) {
        calibrationRequest = false;
        startPulses = pulseCount;
        calibrationPulseCount = 0;
        digitalWrite(RELE, LOW);
        isCalibrating = true;
        state = CALIB_RUNNING;
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Calibration...");
        lcd.setCursor(0, 1);
        lcd.print("Наливайте 5л");
        
        Serial.println("Calibration started");
        Serial.print("Pulses: ");
        Serial.println(startPulses);
      }
      break;

    case CALIB_RUNNING:
      static unsigned long lastUpdate = 0;
      if (millis() > lastUpdate + 1000) {
        lcd.setCursor(0, 1);
        lcd.print("Имп: ");
        lcd.print(String(calibrationPulseCount));
        lcd.print("   ");
        lastUpdate = millis();
        
        Serial.print("Calibration pulses: ");
        Serial.println(calibrationPulseCount);
      }
      
      if (calibrationStopRequest) {
        calibrationStopRequest = false;
        digitalWrite(RELE, HIGH);
        isCalibrating = false;
        state = CALIB_DONE;

        if (calibrationPulseCount > 0) {
          mlPerPulse = 5000.0 / calibrationPulseCount;
          eepromUpdateNeeded = true;
          
          Serial.print("Calibration done, pulses: ");
          Serial.print(calibrationPulseCount);
          Serial.print(", ml/pulse: ");
          Serial.println(mlPerPulse, 4);
          
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Готово!");
          lcd.setCursor(0, 1);
          lcd.print("100мл=");
          lcd.print(100.0/mlPerPulse, 1);
          lcd.print("имп");
        } else {
          Serial.println("Calibration error: 0 pulses");
          lcd.clear();
          lcd.print("Ошибка: 0 имп");
        }
      }
      break;

    case CALIB_DONE:
      Serial.println("Calibration: Done");
      if (digitalRead(ButtonOK) == LOW || digitalRead(ButtonBACK) == LOW) {
        Serial.println("Returning to menu");
        state = CALIB_IDLE;
        currentStatus = Menu;
        needRedraw = true;
      }
      break;
  }
}

void showStatistics() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Общая статистика:");
  lcd.setCursor(0, 1);
  lcd.print(String(totalMoney) + "р " + String(totalLiters, 1) + "л");
  delay(3000);
  currentStatus = Menu;
  needRedraw = true;
}

void showTotalMoney() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Всего денег:");
  lcd.setCursor(0, 1);
  lcd.print(String(totalMoney) + " руб");
  delay(2000);
  currentStatus = Menu;
  needRedraw = true;
}

void showTotalLiters() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Всего литров:");
  lcd.setCursor(0, 1);
  lcd.print(String(totalLiters, 1) + " л");
  delay(2000);
  currentStatus = Menu;
  needRedraw = true;
}

void confirmReset() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Сбросить статистику?");
  lcd.setCursor(0, 1);
  lcd.print("OK-да BACK-нет");

  unsigned long startTime = millis();
  while(millis() - startTime < 5000) {
    if (digitalRead(ButtonOK) == LOW) {
      totalMoney = 0;
      totalLiters = 0;
      sessionMoney = 0;
      
      eepromUpdateNeeded = true;
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Статистика");
      lcd.setCursor(0, 1);
      lcd.print("сброшена!");
      delay(2000);
      break;
    }
    
    if (digitalRead(ButtonBACK) == LOW) {
      break;
    }
    
    delay(100);
  }
  
  currentStatus = Menu;
  needRedraw = true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.flush();
  Serial.println("System started");
  
  pinMode(ButtonOK, INPUT_PULLUP);
  pinMode(ButtonBACK, INPUT_PULLUP);
  pinMode(ButtonNEXT, INPUT_PULLUP);
  pinMode(ButtonMENU, INPUT_PULLUP);
  pinMode(ButtonUSER, INPUT_PULLUP);
  pinMode(RELE, OUTPUT);
  digitalWrite(RELE, HIGH);
  
  pinMode(COIN_PIN, INPUT_PULLUP);
  pinMode(BILL_PIN, INPUT_PULLUP);
  pinMode(SENSOR_PIN, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();

  EEPROM.begin(EEPROM_SIZE);
  loadSettings();

  dht.begin();

  attachInterrupt(digitalPinToInterrupt(COIN_PIN), coinInserted, FALLING);
  attachInterrupt(digitalPinToInterrupt(BILL_PIN), billInserted, FALLING);
  attachInterrupt(digitalPinToInterrupt(ButtonUSER), userButtonPressed, FALLING);
  attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), pulseCounter, FALLING);

  availableLiters = 0;
  waterDispensedLiters = 0;
  lastMoneyInsertTime = 0;
  dispensing = false;
  sessionMoney = 0;
  showingMoneyInfo = false;
  lastPulseReceivedTime = 0;

  showInactive();
}

void loop() {
  unsigned long currentMillis = millis();
  checkWaterSensor();
  handleWaterDispensing();
  
  if (pauseMode && (currentMillis - pauseStartTime > PAUSE_TIMEOUT)) {
    noInterrupts();
    pauseMode = false;
    sessionMoney = 0;
    availableLiters = 0;
    waterDispensedLiters = 0;
    digitalWrite(RELE, HIGH);
    isRelayOn = false;
    interrupts();
    needRedraw = true;
    Serial.println("Pause timeout");
  }
  
  if (showingMoneyInfo && (currentMillis - lastMoneyInsertTime > MONEY_DISPLAY_TIME)) {
    showingMoneyInfo = false;
    sessionMoney = 0;
    availableLiters = 0;
    needRedraw = true;
    Serial.println("Money display timeout, USER button enabled");
  }

  if (currentMillis - lastUpdateTime >= SCREEN_UPDATE_INTERVAL) {
    updateDisplay();
    lastUpdateTime = currentMillis;
  }
  
  if (currentMillis - lastTempCheck > 3000 && !criticalOperation) {
    checkTemperature();
    lastTempCheck = currentMillis;
  }

  if (!criticalOperation) {
    handleSystemState();
    safeEEPROMUpdate();
  }
  
  delay(10);
}
