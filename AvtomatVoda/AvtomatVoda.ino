#define _LCD_TYPE 1

#include <Wire.h>
#include <I2C_LiquidCrystal_RUS.h>
#include <DHT.h>
#include <EEPROM.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// === Пины ===
#define DHT_PIN 4
#define RELAY_PIN 5

#define BUTTON_1 32
#define BUTTON_2 33
#define BUTTON_3 25
#define BUTTON_4 26

#define FLOW_SENSOR_PIN 23    // Расходомер воды
#define COIN_ACCEPTOR_PIN 18  // Монетоприемник
#define BILL_ACCEPTOR_PIN 19 // купюроприемник

// === Настройки ===
#define DHT_TYPE DHT11
#define EEPROM_SIZE 64
#define BUTTON_HOLD_TIME 700  // Время удержания кнопки 4 (в мс)
#define COIN_DEBOUNCE_TIME 50

// === Объекты ===
I2C_LiquidCrystal_RUS lcd(0x27, 16, 2);
DHT dht(DHT_PIN, DHT_TYPE);


// === Переменные для настроек ===
float _PRICEPERLITER = 23.8;  // Цена за литр
float _TEMPON = 25.0;         // Температура включения термокабеля
float _TEMPOFF = 35.0;        // Температура выключения термокабеля
float _WATERCOUNT = 0.0;      // Общая сумма вылитой воды
int _WATERIMPULSE = 0;        // Импульсов на 5л воды

// === Переменные и настройки нельзя менять ===
volatile int waterPulses = 0, coinPulses = 0;
volatile unsigned long lastCoinTime = 0;
int menuIndex = 0;
bool menuActive = false;
volatile unsigned long lastButtonPressTime = 0;
volatile unsigned long button4PressTime = 0;
int cursorPos = 1;
int totalMoney = 0;

#define DEBOUNCE_DELAY 50

// === Очередь для кнопок ===
QueueHandle_t buttonQueue;

// === Прерывания ===
void IRAM_ATTR flowISR() {
  waterPulses++;
}

void IRAM_ATTR coinISR() {
  unsigned long now = millis();
  if (now - lastCoinTime > 70) { // Минимальная защита от дребезга
    coinPulses++;
    lastCoinTime = now;
  }
}

void IRAM_ATTR buttonISR1() {
  int btn = BUTTON_1;
  xQueueSendFromISR(buttonQueue, &btn, NULL);
}
void IRAM_ATTR buttonISR2() {
  int btn = BUTTON_2;
  xQueueSendFromISR(buttonQueue, &btn, NULL);
}
void IRAM_ATTR buttonISR3() {
  int btn = BUTTON_3;
  xQueueSendFromISR(buttonQueue, &btn, NULL);
}

// === Прерывание для кнопки 4 ===
void IRAM_ATTR buttonISR4() {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPressTime > DEBOUNCE_DELAY) {  // Проверка дребезга
    if (digitalRead(BUTTON_4) == LOW) {
      button4PressTime = currentTime;  // Запоминаем время нажатия
    } else {
      unsigned long pressDuration = currentTime - button4PressTime;
      if (pressDuration > BUTTON_HOLD_TIME) {
        menuActive = !menuActive;  // Долгое удержание – открыть/закрыть меню
        menuIndex = 0;
        if (!menuActive) {
          EEPROM.put(5, _TEMPON);
          EEPROM.commit();
          EEPROM.put(10, _TEMPOFF);
          EEPROM.commit();
          EEPROM.put(0, _PRICEPERLITER);
          EEPROM.commit();
        }
      } else {
        int btn = BUTTON_4;
        xQueueSendFromISR(buttonQueue, &btn, NULL);  // Короткое нажатие – переключение подменю
      }
    }
    lastButtonPressTime = currentTime;  // Обновляем время последнего нажатия
  }
}

// === Функция обработки кнопок ===
void buttonTask(void *pvParameters) {
  int button;
  while (1) {
    if (xQueueReceive(buttonQueue, &button, portMAX_DELAY)) {
      switch (button) {
        case BUTTON_1:
          if (menuActive && menuIndex == 1 && cursorPos == 1) {  // Если открыто меню 1
            _TEMPON -= 10.0;
            EEPROM.put(5, _TEMPON);  // Сохраняем в EEPROM
            EEPROM.commit();
          }
          if (menuActive && menuIndex == 1 && cursorPos == 2) {
            _TEMPON -= 1.0;
            EEPROM.put(5, _TEMPON);  // Сохраняем в EEPROM
            EEPROM.commit();
          }
          if (menuActive && menuIndex == 1 && cursorPos == 10) {
            _TEMPOFF -= 10.0;
            EEPROM.put(10, _TEMPOFF);  // Сохраняем в EEPROM
            EEPROM.commit();
          }
          if (menuActive && menuIndex == 1 && cursorPos == 11) {
            _TEMPOFF -= 1.0;
            EEPROM.put(10, _TEMPOFF);  // Сохраняем в EEPROM
            EEPROM.commit();
          }
          break;
        case BUTTON_2:
          if (menuActive && menuIndex == 1) {
            cursorPos++;
          }
          if (menuActive && menuIndex == 1 && cursorPos == 3) {
            cursorPos = 10;
          }
          if (menuActive && menuIndex == 1 && cursorPos == 12) {
            cursorPos = 1;
          }
          break;
        case BUTTON_3:
          if (menuActive && menuIndex == 1 && cursorPos == 1) {  // Если открыто меню 1
            _TEMPON += 10.0;                                     // Увеличиваем значение на 10
            EEPROM.put(5, _TEMPON);                              // Сохраняем в EEPROM
            EEPROM.commit();
          }
          if (menuActive && menuIndex == 1 && cursorPos == 2) {
            _TEMPON += 1.0;
            EEPROM.put(5, _TEMPON);  // Сохраняем в EEPROM
            EEPROM.commit();
          }
          if (menuActive && menuIndex == 1 && cursorPos == 10) {
            _TEMPOFF += 10.0;
            EEPROM.put(10, _TEMPOFF);  // Сохраняем в EEPROM
            EEPROM.commit();
          }
          if (menuActive && menuIndex == 1 && cursorPos == 11) {
            _TEMPOFF += 1.0;
            EEPROM.put(10, _TEMPOFF);  // Сохраняем в EEPROM
            EEPROM.commit();
          }
          break;
        case BUTTON_4:
          if (menuActive) {
            menuIndex++;
            if (menuIndex >= 7) menuIndex = 0;
          }
          break;
      }
    }
  }
}

// === Функция обновления дисплея ===
void lcdTask(void *pvParameters) {
  while (1) {
    lcd.clear();
    if (!menuActive) {
      lcd.noCursor();
      lcd.noBlink();
      lcd.setCursor(0, 0);
      lcd.print("Готов");
      lcd.setCursor(0, 1);
      lcd.print("1л. = " + String(_PRICEPERLITER) + " руб");
    } else {
      switch (menuIndex) {
        case 0:
          lcd.noCursor();
          lcd.noBlink();
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Температура");
          lcd.setCursor(0, 1);
          lcd.print(String(dht.readTemperature()) + " C");
          break;
        case 1:
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Цена за литр");
          lcd.setCursor(0, 1);
          lcd.print(String(_PRICEPERLITER));
          lcd.setCursor(cursorPos, 1);
          lcd.cursor();
          lcd.blink();
          break;
        case 2:
          lcd.clear();
          lcd.setCursor(1, 0);
          lcd.print("Вкл");
          lcd.setCursor(10, 0);
          lcd.print("Выкл");
          lcd.setCursor(1, 1);
          lcd.print(String(_TEMPON));
          lcd.setCursor(10, 1);
          lcd.print(String(_TEMPOFF));
          lcd.setCursor(cursorPos, 1);
          lcd.cursor();
          lcd.blink();
          break;
        case 3:
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Всего воды");
          lcd.setCursor(0, 1);
          lcd.print(String(_WATERCOUNT));
          break;
        case 4:
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Импульсов на 5л");
          lcd.setCursor(0, 1);
          lcd.print(String(_WATERIMPULSE));
          break;
        case 5:
          lcd.clear();
          lcd.noCursor();
          lcd.noBlink();
          lcd.setCursor(0, 0);
          lcd.print("Внесено:");
          lcd.setCursor(0, 1);
          lcd.print("4 руб = 25л");
          break;
        case 6:
          lcd.clear();
          lcd.noCursor();
          lcd.noBlink();
          lcd.setCursor(0, 0);
          lcd.print("Ошибка");
          lcd.setCursor(0, 1);
          lcd.print("Нет воды");
          break;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// === Функция измерения температуры ===
void temperatureTask(void *pvParameters) {
  while (1) {
    float temp = dht.readTemperature();
    Serial.println(temp);
    if (!isnan(temp)) {
      if (temp >= _TEMPON) {
        digitalWrite(RELAY_PIN, HIGH);
      } else if (temp <= _TEMPOFF) {
        digitalWrite(RELAY_PIN, LOW);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// === Функция обработки расходомера воды ===
void flowTask(void *pvParameters) {
  while (1) {
    Serial.printf("Расход воды: %d\n", waterPulses);
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// // === Функция обработки монетоприемника ===
// void coinTask(void *pvParameters) {
//   while (1) {
//     if (coinPulses > 0) {
//       Serial.printf("Получено %d импульсов от монетоприемника\n", coinPulses);
      
//       vTaskDelay(pdMS_TO_TICKS(COIN_DEBOUNCE_TIME)); // Ждем 50 мс
//       vTaskDelay(pdMS_TO_TICKS(10)); // Доп. проверка

//       int addedMoney = 0;
//       switch (coinPulses) {
//         case 1: addedMoney = 1; break;
//         case 2: addedMoney = 2; break;
//         case 5: addedMoney = 5; break;
//         case 10: addedMoney = 10; break;
//         default: 
//           Serial.println("Ошибка: Неверное число импульсов!");
//           addedMoney = 0; break;
//       }

//       if (addedMoney > 0) {
//         totalMoney += addedMoney;
//         Serial.printf("Монета: %d руб. Всего: %d руб.\n", addedMoney, totalMoney);
//       }

//       coinPulses = 0; // Сбрасываем счетчик
//     }
    
//     vTaskDelay(pdMS_TO_TICKS(100)); // Разгружаем процессор
//   }
// }

// === Настройка ===
void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  Wire.begin();
  lcd.init();
  lcd.backlight();
  dht.begin();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  pinMode(COIN_ACCEPTOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(COIN_ACCEPTOR_PIN), coinISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowISR, FALLING);

  buttonQueue = xQueueCreate(10, sizeof(int));

  pinMode(BUTTON_1, INPUT_PULLUP);
  pinMode(BUTTON_2, INPUT_PULLUP);
  pinMode(BUTTON_3, INPUT_PULLUP);
  pinMode(BUTTON_4, INPUT_PULLUP);

  attachInterrupt(BUTTON_1, buttonISR1, FALLING);
  attachInterrupt(BUTTON_2, buttonISR2, FALLING);
  attachInterrupt(BUTTON_3, buttonISR3, FALLING);
  attachInterrupt(BUTTON_4, buttonISR4, CHANGE);

  xTaskCreate(buttonTask, "Button Task", 2048, NULL, 1, NULL);
  xTaskCreate(lcdTask, "LCD Task", 2048, NULL, 1, NULL);
  xTaskCreate(temperatureTask, "Temperature Task", 2048, NULL, 1, NULL);
  xTaskCreate(flowTask, "Flow Task", 2048, NULL, 1, NULL);
  //xTaskCreate(coinTask, "Coin Task", 2048, NULL, 1, NULL);

  EEPROM.get(0, _PRICEPERLITER);
  EEPROM.get(5, _TEMPON);
  EEPROM.get(10, _TEMPOFF);
}

// === Основной цикл ===
void loop() {
  vTaskDelay(portMAX_DELAY);
}
