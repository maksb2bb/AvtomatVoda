#include <Wire.h>

#include <DHT.h>
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

#define FLOW_SENSOR_PIN 23  // Расходомер воды

// === Настройки ===
#define DHT_TYPE DHT11
#define TEMP_THRESHOLD 27 // Порог температуры для реле

// === Объекты ===
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHT_PIN, DHT_TYPE);

volatile int waterPulses = 0;
int menuIndex = 0;

// === Очередь для кнопок ===
QueueHandle_t buttonQueue;

// === Прерывания ===
void IRAM_ATTR flowISR() { 
  waterPulses++; 
}

void IRAM_ATTR buttonISR1() { int btn = BUTTON_1; xQueueSendFromISR(buttonQueue, &btn, NULL); }
void IRAM_ATTR buttonISR2() { int btn = BUTTON_2; xQueueSendFromISR(buttonQueue, &btn, NULL); }
void IRAM_ATTR buttonISR3() { int btn = BUTTON_3; xQueueSendFromISR(buttonQueue, &btn, NULL); }
void IRAM_ATTR buttonISR4() { int btn = BUTTON_4; xQueueSendFromISR(buttonQueue, &btn, NULL); }

// === Функция обработки кнопок ===
void buttonTask(void *pvParameters) {
  int button;
  while (1) {
    if (xQueueReceive(buttonQueue, &button, portMAX_DELAY)) {
      switch (button) {
        case BUTTON_1: menuIndex = (menuIndex + 1) % 4; break; // Переключение меню
        case BUTTON_2: break;
        case BUTTON_3: break;
        case BUTTON_4: break;
      }
    }
  }
}

// === Функция обновления дисплея ===
void lcdTask(void *pvParameters) {
  while (1) {
    lcd.clear();
    switch (menuIndex) {
      case 0: lcd.setCursor(0, 0); lcd.print("1 литр - 4 рубля"); break;
      case 1: lcd.setCursor(0, 0); lcd.print("Температура:"); lcd.setCursor(0, 1); lcd.print(dht.readTemperature()); break;
      case 2: lcd.setCursor(0, 0); lcd.print("Расход воды:"); lcd.setCursor(0, 1); lcd.print(waterPulses); break;
      case 3: lcd.setCursor(0, 0); lcd.print("Реле: "); lcd.print(digitalRead(RELAY_PIN) ? "ВКЛ" : "ВЫКЛ"); break;
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
      if (temp < TEMP_THRESHOLD) digitalWrite(RELAY_PIN, LOW);
      else digitalWrite(RELAY_PIN, HIGH);
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

// === Настройка ===
void setup() {
  Serial.begin(115200);
  Wire.begin();
  lcd.init();
  lcd.backlight();
  dht.begin();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowISR, FALLING);

  buttonQueue = xQueueCreate(10, sizeof(int));

  pinMode(BUTTON_1, INPUT_PULLUP);
  pinMode(BUTTON_2, INPUT_PULLUP);
  pinMode(BUTTON_3, INPUT_PULLUP);
  pinMode(BUTTON_4, INPUT_PULLUP);

  attachInterrupt(BUTTON_1, buttonISR1, FALLING);
  attachInterrupt(BUTTON_2, buttonISR2, FALLING);
  attachInterrupt(BUTTON_3, buttonISR3, FALLING);
  attachInterrupt(BUTTON_4, buttonISR4, FALLING);

  xTaskCreate(buttonTask, "Button Task", 2048, NULL, 1, NULL);
  xTaskCreate(lcdTask, "LCD Task", 2048, NULL, 1, NULL);
  xTaskCreate(temperatureTask, "Temperature Task", 2048, NULL, 1, NULL);
  xTaskCreate(flowTask, "Flow Task", 2048, NULL, 1, NULL);
}

// === Основной цикл ===
void loop() {
  vTaskDelay(portMAX_DELAY);
}
