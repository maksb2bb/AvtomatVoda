#include <Wire.h>
#include <RobotClass_LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <DHT11.h>

#define _btnBack 10
#define _btnOk 11
#define _btnNext 12
#define btnMenu A1
#define THRESHOLD 100
#define DEBOUNCE_DELAY 200
#define HOLD_TIME 3000

float _PRICEPERLITER;
int _PULSEPERLITER = 0;  //? 1 ЛИТР || 5 ЛИТРОВ
int _SUMRUB = 0;
int _WATERLITER = 0;
int _TEMPON = 0;
int _TEMPOFF = 0;
String message = "Нет воды";

bool inMenu = false;
bool set1 = false;
bool set2 = false;
bool set3 = false;
bool set4 = false;
bool set5 = false;
bool set6 = false;
int cursorPos = 5;
bool btnNextPrev = LOW;
bool btnBackPrev = LOW;
bool btnOkPrev = LOW;
bool buttonHold = false;
unsigned long buttonPressTime = 0;
int temperature = 0;

RobotClass_LiquidCrystal_I2C lcd(0x27, 16, 2, CP_UTF8);
DHT11 dht11(9);

void firstScreen() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Готов");
  lcd.setCursor(0, 1);
  lcd.print("1л = " + String(_PRICEPERLITER) + " руб");
}

void settings_1() {
  inMenu = true;
  set1 = true;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Настройки цены");
  lcd.setCursor(0, 1);
  lcd.print("1л = " + String(_PRICEPERLITER) + " руб");
  lcd.setCursor(cursorPos, 1);
  lcd.cursor();
  lcd.blink();
}

void settings_2() {
  inMenu = true;
  set1 = false;
  set2 = true;

  lcd.noCursor();
  lcd.noBlink();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Калиб имп");
  lcd.setCursor(13, 0);
  lcd.print("5л");
  lcd.setCursor(0, 1);
  lcd.print(String(_PULSEPERLITER));
}

void settings_3() {
  inMenu = true;
  set2 = false;
  set3 = true;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Сумма руб");
  lcd.setCursor(0, 1);
  lcd.print(String(_SUMRUB));
}

void settings_4() {
  inMenu = true;
  set3 = false;
  set4 = true;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Литры воды");
  lcd.setCursor(0, 1);
  lcd.print(String(_WATERLITER));
}

void settings_5() {
  inMenu = true;
  set4 = false;
  set5 = true;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("t Вкл");
  lcd.setCursor(0, 1);
  lcd.print(String(_TEMPON));
  lcd.setCursor(10, 0);
  lcd.print("t Выкл");
  lcd.setCursor(10, 1);
  lcd.print(String(_TEMPOFF));

  cursorPos = 0;  // Начальная позиция курсора на "t Вкл"
  lcd.setCursor(cursorPos, 1);
  lcd.cursor();
  lcd.blink();
}

void settings_6() {
  inMenu = true;
  set5 = false;
  set6 = true;

  lcd.noCursor();
  lcd.noBlink();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ошибка");
  lcd.setCursor(0, 1);
  lcd.print(message);
  lcd.setCursor(1, 1);
}

void updatePrice() {
  lcd.setCursor(5, 1);
  lcd.print("            ");
  lcd.setCursor(5, 1);
  lcd.print(String(_PRICEPERLITER) + " руб");
  cursorPos = 5;
  lcd.setCursor(cursorPos, 1);
}

void termoCable(){
  Serial.println(temperature);
  if (temperature <= 27){
    Serial.println("Термо вкл");
    Serial.println(temperature);
    digitalWrite(5, HIGH);
  }else if (temperature >= 28){
    Serial.println("Термо выкл");
    Serial.println(temperature);
    digitalWrite(5, LOW);
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(_btnBack, INPUT);
  pinMode(_btnOk, INPUT);
  pinMode(_btnNext, INPUT);
  pinMode(5, OUTPUT);

  EEPROM.get(0, _PRICEPERLITER);
  EEPROM.get(10, _TEMPON);
  EEPROM.get(20, _TEMPOFF);

  firstScreen();
}

void loop() {
  int value = analogRead(btnMenu);
  bool btnNext = digitalRead(_btnNext);
  bool btnBack = digitalRead(_btnBack);
  bool btnOk = digitalRead(_btnOk);
  temperature = dht11.readTemperature();

  termoCable();

  if (value < THRESHOLD) {  // Кнопка нажата
    if (buttonPressTime == 0) {
      buttonPressTime = millis();  // Фиксируем момент нажатия
    }

    if (millis() - buttonPressTime >= HOLD_TIME && !buttonHold) {
      // Кнопка удерживается 3 секунды — переключаем inMenu
      buttonHold = true;
      inMenu = !inMenu;
      if (inMenu) {
        settings_1();  //цена
      } else {
        EEPROM.put(0, _PRICEPERLITER);
        EEPROM.put(10, _TEMPON);  // 2 байта
        EEPROM.put(20, _TEMPOFF);
        firstScreen();  //основной экран
      }
      buttonPressTime = 0;
      buttonHold = false;
    }
  } else {
    if (buttonPressTime > 0 && !buttonHold) {
      if (set1) {
        settings_2();  // пульсов на 5 литров
      } else if (set2) {
        settings_3();  // сумма руб
      } else if (set3) {
        settings_4();  // литры воды
      } else if (set4) {
        settings_5();  // включение/выключение
      } else if (set5) {
        Serial.println(_TEMPON);
        Serial.println(_TEMPOFF);
        if (_TEMPON > 100) {  // Задайте разумный диапазон
          _TEMPON = 25;       // Значение по умолчанию
        }
        if (_TEMPOFF > 100) {  // Задайте разумный диапазон
          _TEMPOFF = 25;       // Значение по умолчанию
        }
        settings_6();  // Телефон
      }
      //  else if (set6) {
      //   settings_1();  //цена
      // }
      buttonPressTime = 0;
    }
    buttonHold = false;
  }

  // Увеличение цены при нажатии кнопки Next (с ожиданием отпускания)
  if (inMenu && set1 && cursorPos == 5 && btnNext == HIGH && btnNextPrev == LOW) {
    _PRICEPERLITER += 10;
    Serial.println(_PRICEPERLITER);
    updatePrice();
    delay(DEBOUNCE_DELAY);  // Защита от дребезга
    while (digitalRead(_btnNext) == HIGH)
      ;  // Ждём отпускание кнопки
  }
  //Уменьшение цены при нажатии кнопки Back (с ожиданием отпускания)
  if (inMenu && set1 && cursorPos == 5 && btnBack == HIGH && btnBackPrev == LOW) {
    _PRICEPERLITER -= 10;
    Serial.println(_PRICEPERLITER);
    updatePrice();
    delay(DEBOUNCE_DELAY);
    while (digitalRead(_btnBack) == HIGH)
      ;  // Ждём отпускание кнопки
  }
  if (inMenu && set5 && cursorPos == 0 && btnNext == HIGH && btnNextPrev == LOW) {
    lcd.setCursor(0, 1);
    lcd.print("       ");
    lcd.setCursor(0, 1);
    _TEMPON++;
    lcd.print(String(_TEMPON));
    lcd.setCursor(0, 1);
    delay(DEBOUNCE_DELAY);
    while (digitalRead(_btnNext) == HIGH)
      ;
  }
  if (inMenu && set5 && cursorPos == 0 && btnBack == HIGH && btnBackPrev == LOW) {
    lcd.setCursor(0, 1);
    lcd.print("       ");
    lcd.setCursor(0, 1);
    _TEMPON--;
    lcd.print(String(_TEMPON));
    lcd.setCursor(0, 1);
    delay(DEBOUNCE_DELAY);
    while (digitalRead(_btnBack) == HIGH)
      ;
  }
  if (inMenu && set5 && btnOk == HIGH && btnOkPrev == LOW) {
    lcd.noCursor();
    lcd.noBlink();

    if (cursorPos == 0) {
      cursorPos = 10;  // Переход на "t Выкл"
    } else {
      cursorPos = 0;  // Возврат на "t Вкл"
    }

    lcd.setCursor(cursorPos, 1);
    lcd.cursor();
    lcd.blink();

    delay(DEBOUNCE_DELAY);  // защита от дребезга
    while (digitalRead(_btnOk) == HIGH)
      ;  // ожидание отпускания
  }
  if (inMenu && set5 && cursorPos == 10 && btnNext == HIGH && btnNextPrev == LOW) {
    lcd.setCursor(cursorPos, 1);
    lcd.print("       ");
    lcd.setCursor(cursorPos, 1);
    _TEMPOFF++;
    lcd.print(String(_TEMPOFF));
    lcd.setCursor(cursorPos, 1);
    delay(DEBOUNCE_DELAY);
    while (digitalRead(_btnNext) == HIGH)
      ;
  }
  if (inMenu && set5 && cursorPos == 10 && btnBack == HIGH && btnBackPrev == LOW) {
    lcd.setCursor(cursorPos, 1);
    lcd.print("       ");
    lcd.setCursor(cursorPos, 1);
    _TEMPOFF--;
    lcd.print(String(_TEMPOFF));
    lcd.setCursor(cursorPos, 1);
    delay(DEBOUNCE_DELAY);
    while (digitalRead(_btnBack) == HIGH)
      ;
  }


  btnNextPrev = btnNext;  // Запоминаем состояние кнопки
  btnBackPrev = btnBack;  // Запоминаем состояние кнопки
  btnOkPrev = btnOk;
}
