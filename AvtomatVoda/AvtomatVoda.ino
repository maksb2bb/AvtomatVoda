#include <Wire.h>
#include <RobotClass_LiquidCrystal_I2C.h>
#include <EEPROM.h>

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
bool buttonHeld = false;
unsigned long buttonPressTime = 0;

RobotClass_LiquidCrystal_I2C lcd(0x27, 16, 2, CP_UTF8);

void firstScreen() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Готов");
  _PRICEPERLITER = EEPROM.get(0, _PRICEPERLITER);
  lcd.setCursor(0, 1);
  lcd.print("1л = " + String(_PRICEPERLITER) + " руб");
}

void settings_1() {
  inMenu = true;
  set1 = true;

  Serial.println("Settings 1");
  Serial.println(set1);
  Serial.println(set2);
  Serial.println(set3);
  Serial.println(set4);
  Serial.println(set5);
  Serial.println(set6);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Настройки цены");
  lcd.setCursor(0, 1);
  _PRICEPERLITER = EEPROM.get(0, _PRICEPERLITER);
  lcd.print("1л = " + String(_PRICEPERLITER) + " руб");
  lcd.setCursor(5, 1);
  lcd.cursor();
  lcd.blink();
}

void settings_2() {
  inMenu = true;
  set1 = false;
  set2 = true;

  Serial.println("Settings 2");
  Serial.println(set1);
  Serial.println(set2);
  Serial.println(set3);
  Serial.println(set4);
  Serial.println(set5);
  Serial.println(set6);

  lcd.noCursor();
  lcd.noBlink();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Калиб имп");
  lcd.setCursor(13, 0);
  lcd.print("1л");
  lcd.setCursor(0, 1);
  lcd.print(String(_PULSEPERLITER));
}

//??????????????????????? Нужен ли подсчет суммы рублей ?????????????????????????????????
void settings_3() {
  inMenu = true;
  set2 = false;
  set3 = true;

  Serial.println("Settings 3");
  Serial.println(set1);
  Serial.println(set2);
  Serial.println(set3);
  Serial.println(set4);
  Serial.println(set5);
  Serial.println(set6);

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

  Serial.println("Settings 4");
  Serial.println(set1);
  Serial.println(set2);
  Serial.println(set3);
  Serial.println(set4);
  Serial.println(set5);
  Serial.println(set6);

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

  Serial.println("Settings 5");
  Serial.println(set1);
  Serial.println(set2);
  Serial.println(set3);
  Serial.println(set4);
  Serial.println(set5);
  Serial.println(set6);

  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("t Вкл");
  lcd.setCursor(0, 1);
  lcd.print(String(_TEMPON));
  lcd.setCursor(10, 0);
  lcd.print("t Выкл");
  lcd.setCursor(1, 10);
  lcd.print(String(_TEMPOFF));
}

void settings_6() {
  inMenu = true;
  set5 = false;
  set6 = true;

  Serial.println("Settings 6");
  Serial.println(set1);
  Serial.println(set2);
  Serial.println(set3);
  Serial.println(set4);
  Serial.println(set5);
  Serial.println(set6);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Телефон");
  lcd.setCursor(0, 1);
  lcd.print("89895099647");
}

void updatePrice() {
  lcd.setCursor(5, 1);
  lcd.print("            ");
  lcd.setCursor(5, 1);
  lcd.print(String(_PRICEPERLITER) + " руб");
  lcd.setCursor(cursorPos, 1);
}

void setup() {
  Serial.begin(9600);

  pinMode(_btnBack, INPUT);
  pinMode(_btnOk, INPUT);
  pinMode(_btnNext, INPUT);

  firstScreen();
}

void loop() {
  int value = analogRead(btnMenu);
  bool btnNext = digitalRead(_btnNext);
  bool btnBack = digitalRead(_btnBack);
  bool btnOk = digitalRead(_btnOk);

  if (value < THRESHOLD) {  // Кнопка нажата
    if (buttonPressTime == 0) {
      buttonPressTime = millis();  // Фиксируем момент нажатия
    }

    if (millis() - buttonPressTime >= HOLD_TIME && !buttonHeld) {
      // Кнопка удерживается 3 секунды — переключаем inMenu
      buttonHeld = true;
      inMenu = !inMenu;
      if (inMenu) {
        settings_1();
      } else {
        EEPROM.put(0, _PRICEPERLITER);
        firstScreen();
      }
    }
  } else {
    if (buttonPressTime > 0 && !buttonHeld && set1) {
      settings_2();  // Кратковременное нажатие
    }
    if (buttonPressTime > 0 && !buttonHeld && set2) {
      settings_3();  // Кратковременное нажатие
    }
    if (buttonPressTime > 0 && !buttonHeld && set3) {
      settings_4();  // Кратковременное нажатие
    }
    if (buttonPressTime > 0 && !buttonHeld && set4) {
      settings_5();  // Кратковременное нажатие
    }
    if (buttonPressTime > 0 && !buttonHeld && set5) {
      settings_6();  // Кратковременное нажатие
    }
    buttonPressTime = 0;
    buttonHeld = false;
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

  btnNextPrev = btnNext;  // Запоминаем состояние кнопки
  btnBackPrev = btnBack;  // Запоминаем состояние кнопки
  btnOkPrev = btnOk;
}
