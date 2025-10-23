/**********************************

SA JAKIES POWAZNE PROBLEMY Z SYNCHRONIZACJA, JAK NA RAZIE DZIALA ALE TAK LEDWO, NA PEWNO BEDZIE POTRZEBNY SZYBSZY PRZETWORNIK TERMOPAROWY
JAKBY ZROBIC KRZYWE SYSTEMOWE ODPOWIEDNIO TO PEWNIEBY I ODPALIL SILNIK
Z RACJI NA TO ZMIENIAM WERSJE NA ALFA_1.0

**********************************/


#include <ESP32Servo.h>
#include <Arduino.h>
#include "SD.h"
#include "FS.h"
#include <SPI.h>
#include <textparser.h>
#include <stdio.h>
#include <LiquidCrystal_I2C.h>
#include <driver/ledc.h>
#include <Wire.h>

/*
PO POWAŻNIEJSZYCH ZMIANACH ZMIENIC NUMER WERSJI PONIŻEJ
*/

const char *wersja = "alfa_1.0";

/*
DO SPRAWDZENIA PINY SPI I EWENTUALNA ZMIANA WSKAZNIKOW ALBO INNYCH PINOW FUNKCJONALNYCH
*/

int thermoDO = 19;
int thermoCS = 23;
int thermoCLK = 5;

int starter = 25;
int zaplon = 33;
int wtrysk = 32;
int sterowanie_reczne = 13;
int overtemp = 12;
int pin_pot = 35;
int uzbrojony = 15;  //TO MUSI BYC PRZELACZNIK A NIE PRZYCISK, NAJLEPIEJ W ODDZIELNYM PUDELKU ODDALONYM OD GLOWNEGO STEROWNIKA

float temp = NAN;
int rpm = 0;
int stan_dzialania = 0;
//piny do sterowania menu

int menu_gora = 16;
int menu_dol = 17;
int menu_lewo = 14;
int menu_prawo = 26;

/*
PINY DO KARTY SD
*/
int SD_CS = 2;

/*
PINY I USTAWIENIA DO EKRANU LCD
*/
int SDA_EKRAN = 21;
int SCL_EKRAN = 22;
int lcd_kolumny = 20;
int lcd_rzedy = 4;

int STARTER_FREQ = 5000;    //*******************************
int POMPA_FREQ = 5000;      // CZESTOTLIWOSCI SYGNALOW PWM
int ZAPLON_FREQ = 5000;     //*******************************
                            //******************************
int max_temperatura = 800;  // NA PEWNO DO ZMIANY SPYTAC SIE JAKA TEMPERATURA MAX
                            //******************************

int liczba_probek = 4095;  //TU BEDZIE DO ZMIANY JAK NIC

int FSM_state = 0; //kontrolka do glownego FSMa, pewnie bedzie do zmiany na taska z FSMem w srodku

LiquidCrystal_I2C lcd(0x27, lcd_kolumny, lcd_rzedy);

TaskHandle_t handleOdczytTemperatur = NULL;
TaskHandle_t handleReczneSterowanie = NULL;

QueueHandle_t handleTemperatury = NULL;
QueueHandle_t handleTemperaturyEkran = NULL;

SemaphoreHandle_t spiMutex;

SPIClass spiMAX6675(VSPI);

float readMAX6675() {
  uint16_t v;

  digitalWrite(thermoCS, LOW);
  delayMicroseconds(2);

  v = spiMAX6675.transfer16(0x00);

  digitalWrite(thermoCS, HIGH);

  if (v & 0x4) {
    return NAN;  // brak termopary
  }

  v >>= 3;
  return v * 0.25;
}

void ReczneSterowanie(void *parameters) {

  float tempLocalR = 0;
  float tempLocal = 0;

  while (1) {
  float tempLocalR = NAN;
  float tempLocal = NAN;
    if(xQueueReceive(handleTemperatury, &tempLocal, portMAX_DELAY)){
      tempLocalR = tempLocal;
    }

    if (digitalRead(sterowanie_reczne) == HIGH && tempLocalR <= max_temperatura && digitalRead(uzbrojony) == LOW) {

      int przepustnica = analogRead(pin_pot);
      ledcWrite(wtrysk, przepustnica);
      Serial.print("| Przepustnica: ");           //
      Serial.print(przepustnica * 0.0244140625);  //
      Serial.print("%  |  ");                     //
      Serial.print("Temperatura: ");              // TĄ SEKCJĘ WYWALIĆ NA EKRAN W FINALNEJ WERSJI
      Serial.print(tempLocalR);                   // W FORMIE
      Serial.print(" C");                         //
      Serial.print("  |  ");                      // RPM: rpm     TEMP: temp
      Serial.print("RPM: ");                      // PRZEP: przepustnica w %
      Serial.print(rpm);                          //
      Serial.println("  |");                      // STAN: stan
      vTaskDelay(pdMS_TO_TICKS(200));
    }
    if (tempLocalR >= max_temperatura) {

      digitalWrite(sterowanie_reczne, LOW);
      digitalWrite(wtrysk, LOW);
      digitalWrite(overtemp, HIGH);
      ledcWrite(wtrysk, 0);
      Serial.print("DANGER TO THE MANIFOLD, FLOOD THE FLUX CAPACITOR !!!");  //TUTAJ TEZ WYWALIC NA EKRAN
      stan_dzialania = 9;
      FSM_state = 2;      //******************* W TYM WYPADKU TRZEBABY ZROBIC JAKIS TASK Z AWARYJNYM CHLODZENIEM
      digitalWrite(sterowanie_reczne,LOW);
      vTaskDelay(pdMS_TO_TICKS(2000)); 
      vTaskDelete(NULL);
    }

    if(digitalRead(uzbrojony) == HIGH)
    {
      digitalWrite(sterowanie_reczne,LOW);
      FSM_state = 2;
      stan_dzialania = 10;
      vTaskDelete(NULL);
    }
  }
}

void OdczytTemperatur(void *parameters) {
  while (1) {
    if (xSemaphoreTake(spiMutex, portMAX_DELAY)) {
      temp = readMAX6675();
      xSemaphoreGive(spiMutex);
    }
    Serial.print(temp);
    xQueueSend(handleTemperatury, &temp, portMAX_DELAY);
    xQueueSend(handleTemperaturyEkran, &temp, portMAX_DELAY);
    rpm = 10000;  //odczyt rpm trzeba dodac pozniej jak sie ogarnie jak to zrobic
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void SterowanieEkranem(void *parameters) {
  int poprzedni_stan = -1;  // zapamiętuje poprzedni stan
  unsigned long lastUpdate = 0; // do okresowego odświeżania

  lcd.init();
  lcd.backlight();
  lcd.clear();

  while (1) {
    // --- Zmiana ekranu przy zmianie stanu ---
    if (stan_dzialania != poprzedni_stan) {
      poprzedni_stan = stan_dzialania;
      lcd.clear();

      switch (stan_dzialania) {
        case 1:
          lcd.setCursor(0, 0);
          lcd.print("Sterownik GTM140");
          lcd.setCursor(0, 1);
          lcd.print("Projekt Hydrogen");
          lcd.setCursor(0, 2);
          lcd.print(wersja);
          vTaskDelay(pdMS_TO_TICKS(1000));
          break;

        case 2:
          lcd.setCursor(0, 0);
          lcd.print("MENU GLOWNE");
          lcd.setCursor(0, 1);
          lcd.print("Gora - start");
          lcd.setCursor(0, 2);
          lcd.print("Dol - ustawienia");
          lcd.setCursor(0, 3);
          lcd.print("Prawo - reczne syst.");
          break;

        case 3:
          lcd.setCursor(7, 1);
          lcd.print("START");
          lcd.setCursor(6, 2);
          lcd.print("SILNIKA");
          break;

        case 4:
          lcd.setCursor(0,0);
          lcd.print("SILNIK NIEUZBROJONY");
          lcd.setCursor(0,2);
          lcd.print("PRZED URUCHOMIENIEM");
          lcd.setCursor(0,3);
          lcd.print("UZBROJ SYSTEM");
          break;

        case 5:
          lcd.setCursor(4,0);
          lcd.print("USTAWIENIA");
          lcd.setCursor(0,1);
          lcd.print("SEKCJA W BUDOWIE");
          lcd.setCursor(0,2);
          lcd.print("LEWO - POWROT DO MENU");
          break;

        case 6:
          lcd.setCursor(0,0);
          lcd.print(" RECZNA OPERACJA");
          lcd.setCursor(0,1);
          lcd.print("TRZYMAJ GORA - POMPA");
          lcd.setCursor(0,2);
          lcd.print("TRZYMAJ DOL - BLDC");
          lcd.setCursor(0,3);
          lcd.print("LEWO - POWROT DO MENU");
          break;

        case 7:
          lcd.setCursor(7,1);
          lcd.print("STARTUP");
          break;

        case 8:
          FSM_state = 1;
          lcd.setCursor(0,0);
          lcd.print("Temp: ");
          lcd.setCursor(10,0);
          lcd.print("RPM: ");
          lcd.setCursor(0,1);
          lcd.print("Przep: ");
          break;

        case 9:
        lcd.setCursor(0,0);
        lcd.print("PRZEGRZANIE SILNIKA");
        lcd.setCursor(0,2);
        lcd.print("ROZPOCZECIE PROCEDURY");
        lcd.setCursor(0,3);
        lcd.print("CHLODZENIA");
        break;

        case 10:
        lcd.setCursor(4,2);
        lcd.print("COOLDOWN");
        break;

        case 11:
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("KONIEC PRACY");
        vTaskDelay(pdMS_TO_TICKS(2000));
        FSM_state = 0;
        break;

        default:
          lcd.setCursor(0,0);
          lcd.print("COS POSZLO NIE TAK");
          lcd.setCursor(0,2);
          lcd.print("JESLI MNIE CZYTASZ");
          lcd.setCursor(0,3);
          lcd.print("TO ZRESETUJ STEROWNIK");
          break;
      }
    }

    // --- Dynamiczne odświeżanie danych w stanie 8 ---
    if (stan_dzialania == 8) {
      float tempLocal = 0;
      float tempLocalR = 0;
      unsigned long now = millis();
      if (now - lastUpdate > 100) { // odśwież co 500 ms
        lastUpdate = now;

        if(xQueueReceive(handleTemperaturyEkran, &tempLocal, portMAX_DELAY)){
        tempLocalR = tempLocal;
        }
        // Temperatura
        lcd.setCursor(6, 0);
        lcd.print("     "); // wyczyść poprzednie dane
        lcd.setCursor(6, 0);
        lcd.print(tempLocal, 1); // 1 miejsce po przecinku
        

        // RPM
        lcd.setCursor(15, 0);
        lcd.print("    ");
        lcd.setCursor(15, 0);
        lcd.print(rpm);

        //Przepustnica

        lcd.setCursor(7,1);
        lcd.print("       ");
        lcd.setCursor(7,1);
        lcd.print(analogRead(pin_pot)*0.0244140625,2);
      }
    }

    // --- Zwolnij CPU ---
    vTaskDelay(pdMS_TO_TICKS(500)); // opóźnienie dla płynności
  }
}

void FSMTask(void *pvParameters) {
  while (1) {
    switch (FSM_state) {

      case 0: {
        stan_dzialania = 1;
        vTaskDelay(pdMS_TO_TICKS(2000));
        int kontrolka = 0;
        bool wyswietlono = false;
        bool running = true;

        unsigned long lastPressTime = 0;
        const unsigned long debounceDelay = 200;

        bool pompaWlaczona = false;
        bool starterWlaczony = false;

        stan_dzialania = 2;

        // zatrzymaj inne taski, jeśli działają
        if (handleOdczytTemperatur != NULL) {
          vTaskDelete(handleOdczytTemperatur);
          handleOdczytTemperatur = NULL;
        }
        if (handleReczneSterowanie != NULL) {
          vTaskDelete(handleReczneSterowanie);
          handleReczneSterowanie = NULL;
        }

        while (running && FSM_state == 0) {
          unsigned long teraz = millis();

          switch (kontrolka) {
            case 0: // MENU GŁÓWNE
              if (!wyswietlono) {
                Serial.println("=== MENU GŁÓWNE ===");
                Serial.println("Góra  - Start");
                Serial.println("Dół   - Ustawienia");
                Serial.println("Prawo - Ręczna obsługa systemów");
                wyswietlono = true;
              }

              if (digitalRead(menu_gora) == LOW && (teraz - lastPressTime > debounceDelay)) {
                lastPressTime = teraz;

                if (digitalRead(uzbrojony) == LOW) {
                  stan_dzialania = 3;
                  Serial.println("System uzbrojony – start!");
                  delay(1000);
                  running = false;
                } else {
                  stan_dzialania = 4;
                  Serial.println("System nieuzbrojony – nie można uruchomić!");
                  delay(1000);
                  stan_dzialania = 2;
                }
              }

              if (digitalRead(menu_dol) == LOW && (teraz - lastPressTime > debounceDelay)) {
                lastPressTime = teraz;
                kontrolka = 1;
                stan_dzialania = 5;
                wyswietlono = false;
              }

              if (digitalRead(menu_prawo) == LOW && (teraz - lastPressTime > debounceDelay)) {
                lastPressTime = teraz;
                kontrolka = 2;
                stan_dzialania = 6;
                wyswietlono = false;
              }
              break;

            case 1: // USTAWIENIA
              if (!wyswietlono) {
                Serial.println("=== USTAWIENIA ===");
                Serial.println("Lewo - Wróć do menu głównego");
                wyswietlono = true;
              }

              if (digitalRead(menu_lewo) == LOW && (teraz - lastPressTime > debounceDelay)) {
                lastPressTime = teraz;
                kontrolka = 0;
                stan_dzialania = 2;
                wyswietlono = false;
              }
              break;

            case 2: // RĘCZNA OBSŁUGA
              if (!wyswietlono) {
                Serial.println("=== RĘCZNA OBSŁUGA ===");
                Serial.println("Przytrzymaj Góra  - Pompa");
                Serial.println("Przytrzymaj Dół   - Starter");
                Serial.println("Lewo - Wróć do menu głównego");
                wyswietlono = true;
              }

              if (digitalRead(menu_gora) == LOW) {
                if (!pompaWlaczona) {
                  Serial.println("Pompa WŁĄCZONA");
                  ledcWrite(wtrysk, 1500);
                  pompaWlaczona = true;
                }
              } else if (pompaWlaczona) {
                Serial.println("Pompa WYŁĄCZONA");
                ledcWrite(wtrysk, 0);
                pompaWlaczona = false;
              }

              if (digitalRead(menu_dol) == LOW) {
                if (!starterWlaczony) {
                  Serial.println("Starter WŁĄCZONY");
                  ledcWrite(starter, 1500);
                  starterWlaczony = true;
                }
              } else if (starterWlaczony) {
                Serial.println("Starter WYŁĄCZONY");
                ledcWrite(starter, 0);
                starterWlaczony = false;
              }

              if (digitalRead(menu_lewo) == LOW && (teraz - lastPressTime > debounceDelay)) {
                lastPressTime = teraz;
                kontrolka = 0;
                stan_dzialania = 2;
                wyswietlono = false;
              }
              break;
          }

          vTaskDelay(pdMS_TO_TICKS(20));
        }

        // --- START SILNIKA ---
        Serial.println("START SILNIKA");
        stan_dzialania = 7;
        delay(500);

        for (int i = 0; i < liczba_probek; i++) {
          ledcWrite(starter, liczba_probek - i - 1000);
          ledcWrite(wtrysk, i);
          if (i > 500 && i < 2000) digitalWrite(zaplon, HIGH);
          else digitalWrite(zaplon, LOW);
          delay(1);
        }

        digitalWrite(sterowanie_reczne, HIGH);
        stan_dzialania = 8;
        FSM_state = 1;
      }
      break;

      case 1: {
        // Tryb ręczny – uruchamiamy taski tylko jeśli nie istnieją
      xTaskCreatePinnedToCore(OdczytTemperatur,"odczTemp",10000, NULL, 2, &handleOdczytTemperatur,0);
      xTaskCreatePinnedToCore(ReczneSterowanie, "reczSter", 10000, NULL, 2, &handleReczneSterowanie,0);
      }
      break;

      case 2: {
        // cooldown – zatrzymujemy taski
        if (handleOdczytTemperatur != NULL) {
          vTaskDelete(handleOdczytTemperatur);
          handleOdczytTemperatur = NULL;
        }
        if (handleReczneSterowanie != NULL) {
          vTaskDelete(handleReczneSterowanie);
          handleReczneSterowanie = NULL;
        }

        for (int i = 4095; i >= 0; i--) {
          ledcWrite(starter, 4095 - i);
          ledcWrite(wtrysk, i);
          delay(1);
        }
        ledcWrite(starter,0);
        stan_dzialania = 11;
        FSM_state = 0;
      }
      break;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
void setup() {

  ledcAttach(wtrysk, POMPA_FREQ, 12);
  ledcAttach(starter, STARTER_FREQ, 12);
  ledcAttach(zaplon, ZAPLON_FREQ, 12);



  Serial.begin(115200);
 
  /* WYLACZONE DO TESTOW WSTEPNYCH
  SD.begin(SD_CS);
  if (!SD.begin(SD_CS)) {
    Serial.println("Card mount sie nie udal");
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("Nie ma karty SD w gniezdzie");
    return;
  }
  Serial.println("Inicjalizowanie...");
  if (!SD.begin(SD_CS)) {
    Serial.println("Cos sie nie udalo");
    return;
  }
*/
  pinMode(zaplon, OUTPUT);
  pinMode(sterowanie_reczne, OUTPUT);  // WSKAZNIKI LED
  pinMode(overtemp, OUTPUT);           //
  pinMode(menu_gora, INPUT_PULLUP);
  pinMode(menu_dol, INPUT_PULLUP);
  pinMode(menu_lewo, INPUT_PULLUP);
  pinMode(menu_prawo, INPUT_PULLUP);
  pinMode(uzbrojony, INPUT_PULLUP);  // przycisk do masy HIGH-nieklikniety LOW-klikniety

  handleTemperatury = xQueueCreate(5,sizeof(float));
  if(handleTemperatury == NULL){
    Serial.println("Kolejka niezainicjalizowana");
  }

  handleTemperaturyEkran = xQueueCreate(5,sizeof(float));
  if(handleTemperaturyEkran == NULL){
    Serial.println("Kolejka ekranowa niezainicjalizowana");
  }

  pinMode(thermoCS, OUTPUT);
  digitalWrite(thermoCS, HIGH);
  spiMAX6675.begin(thermoCLK, thermoDO, -1, thermoCS);

  spiMutex = xSemaphoreCreateMutex();

  delay(1000);

  xTaskCreatePinnedToCore(SterowanieEkranem,"ekran",10000,NULL,2,NULL,1);
  xTaskCreatePinnedToCore(FSMTask,"fsm",20000,NULL,2,NULL,1);

  Serial.println("Sterownik silnika GTM140");
  Serial.print("Wersja: ");
  Serial.println(wersja);


  }


void loop() {
  // put your main code here, to run repeatedly:
}
