#define _LCD_TYPE 1
#include <Wire.h>
#include <EEPROM.h>
#include <I2C_LiquidCrystal_RUS.h>
#include <DHT.h>

I2C_LiquidCrystal_RUS lcd(0x27, 16, 2);

// Buttons
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
#define MONEY_INSERTION_TIMEOUT 500 // Время для определения активных импульсов (в мс)

DHT dht(DHTPIN, DHT11);

// Global variables
volatile bool criticalOperation = false;
volatile bool eepromUpdateNeeded = false;
float price = 10.00;
int tempOn = 18;
int tempOff = 22;
float mlPerPulse = 0.1;

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
unsigned long lastPulseReceivedTime = 0;

// Debounce
volatile unsigned long lastCoinInterruptTime = 0;
volatile unsigned long lastBillInterruptTime = 0;
volatile unsigned long lastUserButtonPress = 0;
const unsigned long debounceDelay = 300;

// Flag for user button press
volatile bool userButtonFlag = false;

void IRAM_ATTR pulseCounter() {
  static unsigned long lastPulseTime = 0;
  unsigned long now = micros();
  
  if (now - lastPulseTime > 5000) { 
    if (dispensing) {
      pulseCount++;
      lastPulseReceivedTime = millis();
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
  }
  lastBillInterruptTime = interruptTime;
  interrupts();
}

void IRAM_ATTR userButtonPressed() {
  static unsigned long lastPress = 0;
  unsigned long now = millis();
  
  if (now - lastPress > debounceDelay) {
    userButtonFlag = true;
    lastPress = now;
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
}

void checkTemperature() {
  float t = dht.readTemperature();
  if (!isnan(t)) {
    currentTemp = t;
    needRedraw = true;

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
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Подача воды");
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
      showInactive();
      if (availableLiters > 0) {
        lcd.setCursor(10, 1);
        lcd.print(String(availableLiters, 1) + " л");
      }
    }
  }
  
  lastDispensingState = dispensing;
  lastAvailableLiters = availableLiters;
  lastWaterDispensed = waterDispensedLiters;
  needRedraw = false;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.flush();
  Serial.println("System started");
  
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

  // Handle user button press
  if (userButtonFlag) {
    userButtonFlag = false;
    // Проверяем, поступают ли импульсы от купюроприёмника (в течение 500 мс)
    if ((currentMillis - lastCoinInterruptTime < MONEY_INSERTION_TIMEOUT || 
         currentMillis - lastBillInterruptTime < MONEY_INSERTION_TIMEOUT)) {
      Serial.println("USER button ignored: money insertion in progress");
    } else if (availableLiters > 0 && !dispensing) {
      dispensing = true;
      digitalWrite(RELE, LOW);
      isRelayOn = true;
      waterDispensedLiters = 0;
      pulseCount = 0;
      pauseMode = false;
      needRedraw = true;
      Serial.println("Dispensing started");
    } else if (availableLiters <= 0 && !dispensing) {
      showNoMoneyMessage = true;
      needRedraw = true;
      Serial.println("No funds");
    }
  }

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
    safeEEPROMUpdate();
  }
  
  delay(10);
}
