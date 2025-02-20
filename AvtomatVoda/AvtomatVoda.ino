//---Подключение дополнительных библиотек---//
#include <LiquidCrystal.h>
#include <EEPROM.h>
//------------------------------------------//

//---Настройка пинов---//
#define BUTTON_MENU 6
#define BUTTON_NEXT 5
#define BUTTON_BACK 4
#define BUTTON_EXIT 3
//---------------------//

LiquidCrystal lcd();

//---Переменные---//
float PricePerLiter;
int min = 5.0;
int sec = 60;
float liter = 0;
//----------------//

//---Запуск LCD экрана---//
void beginlcd(){
  lcd.init();
  lcd.backlight();
  lcd.setCoursor(0,0);
}
//-----------------------//

//---Главный экран---//
void startScreen(){
  lcd.setCoursor(0,0);
  lcd.print("Готов");
  lcd.setCoursor(0,1);
  lcd.print("1 литр = " + PricePerLiter +" руб");
}
//-------------------//

//---Обновление цены за литр---//
void updatePrice(){
  if(BUTTON_NEXT){
    PricePerLiter = PricePerLiter + 0.5;
  }
  if(BUTTON_BACK){
    PricePerLiter = PricePerLiter - 0.5;
  }
  if(BUTTON_EXIT){
    EEPROM.write(PricePerLiter);
    startScreen();
  }
}
//-----------------------------//

if(BUTTON_MENU > )

void setup() {
  PricePerLiter = EEPROM.read(PricePerLiter);
}

void loop() {

}

