#include <ESP32Servo.h>
#include <Arduino.h>
#include "SD.h"
#include "FS.h"
#include <SPI.h>
#include <textparser.h>
#include <stdio.h>

/*
PO POWAŻNIEJSZYCH ZMIANACH ZMIENIC NUMER WERSJI PONIŻEJ
*/

char string[] = "alfa preview";

/*
PLUH
*/

int thermoDO = 19;
int thermoCS = 23;
int thermoCLK = 4;

int przycisk = 21;
int starter = 25;
int zaplon = 33;
int wtrysk = 32;
int sterowanie_reczne = 13; 
int overtemp = 12;
int pin_pot = 35;
int uzbrojony = 15;

//piny do sterowania menu

int menu_gora = 16;
int menu_dol = 17;
int menu_lewo = 18;
int menu_prawo = 19

int SD_CS 5;

int STARTER_FREQ = 5000;     //*******************************
int POMPA_FREQ = 5000;       //
int ZAPLON_FREQ = 5000;      // CZESTOTLIWOSCI SYGNALOW PWM (DO SPRAWDZENIA)
                             //*******************************    
                             //
                             //******************************    
int max_temperatura = 800;   // NA PEWNO DO ZMIANY SPYTAC SIE JAKA TEMPERATURA MAX
                             //******************************
                          
int liczba_probek=1000000;

QueueHandle_t kolejka_temp, kolejka_rpm;

void setup() {

  kolejka_temp xQueueCreate(1,sizeof(float));
  kolejka_rpm xQueueCreate(1,sizeof(int));
  
  int temperatura = 0;
  int rpm = 0; //albo tu albo w pomiarowce

  bool ledcAttach(wtrysk,POMPA_FREQ,12);
  bool ledcAttach(starter,STARTER_FREQ,12);
  bool ledcAttach(zaplon,ZAPLON_FREQ,12);

  void ReczneSterowanie(void *parameters){
  if(uxQueueMessagesWaiting(kolejka_temp)>0){
    uQueueRecieve(kolejka_temp,&temp,0);
  }
  else{
    continue;
  }
  if(uxQueueMessagesWaiting(kolejka_rpm)>0){
    uQueueRecieve(kolejka_rpm,&rpm,0);
  }
  else{
    continue;
  }

 if(digitalRead(sterowanie_reczne)==HIGH && temp<max_temperatura && digitalRead(uzbrojony==HIGH)){ //TUTAJ TEZ DO ZMIANY TEMPERATURA

  int przepustnica = analogRead(pin_pot);   
  ledcWrite(wtrysk,przepustnica);          
  Serial.print("| Przepustnica: ");                   //
  Serial.print(przepustnica * 0.0244140625);          //
  Serial.print("%  |  ");                             //
  Serial.print("Temperatura: ")                       // TĄ SEKCJĘ WYWALIĆ NA EKRAN W FINALNEJ WERSJI
  Serial.print(temp);                                 //
  Serial.print(" C");                                 //
  Serial.print("%  |  ");                             //
  Serial.print("RPM: ");                              //
  Serial.print(rpm);                                  //
  Serial.println("  |");                              //

}
else{

  digitalWrite(sterowanie_reczne,LOW);
  digitalWrite(wtrysk,LOW);
  digitalWrite(overtemp,HIGH);
  ledcWrite(stan_przepustnicy,0);
  Serial.print("DANGER TO THE MANIFOLD, FLOOD THE FLUX CAPACITOR !!!");   //TUTAJ TEZ WYWALIC NA EKRAN
  vTaskDelete(NULL);
  }
  }

  void OdczytTemperatur(void *parameters){

    odczyt_temp = thermocouple.readCelsius();
    odczyt_rpm = 10000;//odczyt rpm trzeba dodac pozniej jak sie ogarnie jak to zrobic

    xQueueSend(kolejka_temp,odczyt_temp,0);
}

Serial.begin(115200);
  
  SD.begin(SD_CS);  
  if(!SD.begin(SD_CS)) {
    Serial.println("Card mount sie nie udal");
    return;
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE) {
    Serial.println("Nie ma karty SD w gniezdzie");
    return;
  }
  Serial.println("Inicjalizowanie...");
  if (!SD.begin(SD_CS)) {
    Serial.println("Cos sie nie udalo");
    return;    
  }

  pinMode(sterowanie_reczne,OUTPUT);  // WSKAZNIKI LED
  pinMode(overtemp,OUTPUT);           //
  pinMode(przycisk, INPUT_PULLUP);
  pinMode(menu_gora, INPUT_PULLUP);
  pinMode(menu_dol, INPUT_PULLUP);
  pinMode(menu_lewo, INPUT_PULLUP);
  pinMode(menu_prawo, INPUT_PULLUP); // przycisk do masy

  Serial.println("Sterownik silnika GTM140");
  Serial.print("Wersja: ");
  Serial.println(wersja);


/*
TUTAJ BEDZIE JAK NIC DO POPRAWY SPRAWDZANIE WCISNIEC ALE TO MOZNA ZROBIC KIEDY INDZIEJ
*/

  int kontrolka = 0;
  int ilosc_ustawien = 5;

  while(1){
    switch(kontrolka){
      case(0){
        Serial.println("Menu główne");
        Serial.println("Gora - Start");
        Serial.println("Dol - Ustawienia");
        Serial.println("Prawo - Reczna obsluga systemow")
        if(menu_gora==HIGH) {goto wyjscie;}
        if(menu_dol==HIGH) {kontrolka = 1;}
        if(menu_prawo==HIGH) {kontrolka = 2};
      }
      case(1){
        TextParser parser_ust{","};
        int numer_ustawienia = 0;
        ustawienia = SD.open("ustawienia.txt");
        char *array = {};                               //
        parser_ust.parseLine(ustawienia.c_str(),array); //tutaj moze byc cos nie tak
        /***************
        DOKONCZYC USTAWIENIA
        ******************/
      }
      case(2){
        int system = 0;
        char *systemy = {"Wlacz pompe", "Wlacz starter", "Wlacz swiece"};
        

      }
    }
  }

  /**************************************************************************************************
  Serial.println("Przycisk wciśnięty! Start sekwencji...");
  ledcWrite(stan_przepustnicy,0);
  digitalWrite(starter, HIGH);
  delay(2000); //tutaj dac odczyt rpm i powyzej jakichstam dac wtrysk i zaplon
  digitalWrite(wtrysk,HIGH);
  ledcWrite(stan_przepustnicy,60);
  delay(800); //tu tez jest przesadzony delay
  digitalWrite(zaplon,HIGH);

  bool warunek = true;

  while(warunek == true){
  int temperatura = thermocouple.readCelsius();
  if(temperatura>=50){
    digitalWrite(starter,LOW);
    digitalWrite(zaplon,LOW);
    digitalWrite(sterowanie_reczne,HIGH);
    warunek = false;
  Serial.println("Sekwencja zapłonu zakończona. Przechodzę do sterowania recznego.");
  }
}
***************************************************************************************************/

wyjscie: TextParser parser{","};

int[] profil_przepustnicy_plik = SD.open("profil_przepustnicy.txt");
uint8_t profil_przepustnicy[liczba_probek];
parser.parseLine(profil_przepustnicy_plik.c_str(),profil_przepustnicy);
profil_przepustnicy_plik.close();

int[] profil_startera_plik = SD.open("profil_startera.txt");
uint8_t profil_startera[liczba_probek];
parser.parseLine(profil_startera_plik.c_str(),profil_startera);
profil_startera_plik.close();

/*******************************************

EWENTUALNIE DODAC PROFIL SWIECY ALE TO MOZNA ZROBIC POPRZEZ SPRAWDZENIE NA KTOREJ PROBCE SIE JEST

MOZNA TEZ DODAC SPRAWDZANIE NA KTOREJ PROBCE POWINNA WZROSNAC TEMPERATURA I JAK, WTEDY MOZNA DAC JAKIEKOLWIEK ZABEZPIECZENIA

********************************************/

for(int i=0;i<liczba_probek;i++){

  wtrysk = profil_przepustnicy[i];
  starter = profil_startera[i];

  /*

  int czas_na_iskre = 213769;
  int czas_na_brak_iskry = 694200;

  if(i >= czas_na_ogien && i <= czas_na_brak_iskry){

  zaplon = HIGH;

  }
  */

}

xTaskCreate(OdczytTemperatur, "odczTemp", 1000, NULL, 2, &odczTemp)
xTaskCreate(ReczneSterowanie, "sterowanie", 10000, NULL, 2, &sterowanie);


}

void loop() {
  // put your main code here, to run repeatedly:

}
