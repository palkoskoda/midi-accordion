/*
    ArduinoPrintADC.ino
    Author: Seb Madgwick
    Sends up to all 6 analogue inputs values in ASCII as comma separated values
    over serial.  Each line is terminated with a carriage return character ('\r').
    The number of channels is sent by sending a character value of '1' to '6' to
    the Arduino.

    Tested with "arduino-1.0.3" and "Arduino Uno".
*/

#define SIGNAL_CLOCK 3 //hodinove impulzi
#define SIGNAL_STARTER A0 //starter, alebo sinxronizacne miesto
#define SIGNAL_INPUT A1 //zbernica
#define TRANSPO 39 //prvi ton je dis = 0 od neho to posunie o TRANSPO poltonov pre midi
#define NUM_BUTTONS  48 //48 kláves (realne ich je troxa menej)

#include <stdlib.h> // div, div_t
#include <avr/io.h>
#include <avr/interrupt.h>
#include <EEPROM.h>

unsigned int input_raw[NUM_BUTTONS];
unsigned char inputMidiMem[NUM_BUTTONS];
unsigned int vstupmax[NUM_BUTTONS];
unsigned int inputInbornBias[NUM_BUTTONS]; // bias od poskladania (asi ho dam do EEPROM)
int inputFluctBias[NUM_BUTTONS]; // dotiahnutie kludoveho stavu k nule
//int inputStaticBias = 0; //spuosobení hlavne osvetlením
unsigned int kalibracia = 100;
int inputPre[NUM_BUTTONS]; //predspracovane vstupi
int inputNorm[NUM_BUTTONS]; //normalizovane vstupi
int notePitches[NUM_BUTTONS]; //nacita z eepromky ktori snimac ma ktori ton


void setup() {
  pinMode(SIGNAL_INPUT, INPUT);  //zbernica z fototranzistorov
  pinMode(SIGNAL_CLOCK, OUTPUT);
  pinMode(SIGNAL_STARTER, OUTPUT); //na rozbehnutie potrebujem podrzať tento pin v 1, potom podla neho budem sinxronizovať
  digitalWrite(SIGNAL_STARTER, HIGH); // startovanie
  Serial.begin(115200);

  for (int i = 0; i <= NUM_BUTTONS; i++) { //nacitanie viski tonov
    EEPROM.get(i * 4, notePitches[i]);
  }

  for (int i = 0; i < 10; i++) //naseka 10 jednotiek do radu
  { digitalWrite(SIGNAL_CLOCK, HIGH);
    delay(1);
    digitalWrite(SIGNAL_CLOCK, LOW);
    delay(1);
  }
  digitalWrite(SIGNAL_STARTER, LOW);//vipne SIGNAL_STARTER
  pinMode(SIGNAL_STARTER, INPUT);

}

void loop() {

  serialRefresh(); // príkazi zo serioveho portu (set)

  for (int i = 0; i < NUM_BUTTONS ; i++) //ciklus merania klapiek
  {
    digitalWrite(SIGNAL_CLOCK, LOW);
    //delay(30);

    input_raw[i] = analogRead(SIGNAL_INPUT);
    // delayMicroseconds(60);
    digitalWrite(SIGNAL_CLOCK, HIGH);


    ///midi
    inputPre[i] = input_raw[i] - inputInbornBias[i] ; //predspracovane vstupi
    inputNorm[i] = (inputPre[i]) * 100 / vstupmax[i];

    if (kalibracia == 0) midiSend(i);


    if (analogRead(SIGNAL_STARTER) > 400)   {  //tu snima SIGNAL_STARTER, podla neho sinchronizuje meranie mala by na nom bit log 1, ale ked je inputPre napajani tak radsej 400/1024
      i = 60; break;
    }
    //delayMicroseconds(1000);

  }

  //////////////////////koniec meracej slucki/////////////////////////////////////////////////////

  if (kalibracia == 0) {

    //inputStaticBias = calcInputStaticBias();
    calcInputBias();

    for (int i = 0; i < NUM_BUTTONS; i++)  { //meria priebezne maximum vstupov
      vstupmax[i] = max(vstupmax[i], inputPre[i]); //toto bude velmi citlive na nahodne spicki, treba to spravit zivsie, alebo ukladat do pamete pri montazi
    }
  }

  if (kalibracia != 0) { //prve kola nekalibruje
    if (kalibracia < 20) { //poslednich 20 si robí priemer
      for (int x = 0; x < NUM_BUTTONS; x++)  {
        inputInbornBias[x] = (inputInbornBias[x] * 3 + input_raw[x]) / 4;
      }
    }
    if (kalibracia == 1) Serial.println("skalibrovane"); //posledne kolo vipise
    kalibracia--;
  }

}




void midiSend(int i) {
  if (inputPre[i] > 30) {
    //Serial.println("cislo: " + String(i) + ", nazov: " + String(notePitches[i]) + " hodnota: " + String(inputPre)); //toto sa zide pri nastavovani
    if (inputMidiMem[notePitches[i]] != 0) {
      Serial.write(0x91); //cmd
      Serial.write(notePitches[i] + TRANSPO); //pitch
      Serial.write(127); //velocity
    }
    inputMidiMem[notePitches[i]] = 1;
  }
  if (inputMidiMem[notePitches[i]] == 0) { // ak pride 1 zahra ton, nastavi do nuli, ak je v nule tak uz nezahra a ak nepride znova 1 tak nastavi do -1 a vipne ton
    Serial.write(0x81); //cmd
    Serial.write(notePitches[i] + TRANSPO); //pitch
    Serial.write(127); //velocity
    inputMidiMem[notePitches[i]] = -1;
  }
  if (inputMidiMem[notePitches[i]] == 1) inputMidiMem[notePitches[i]] = 0;
}

void calcInputBias() {
  byte i;
  int avg = 0;
  for (i = 0; i < NUM_BUTTONS / 2; i++) {//vipocitam priemer
    avg += inputPre[i];
  }
  avg=avg / i;

  byte j = 0;
  int avg2 = 0;
  for (i = 0; i < NUM_BUTTONS / 2; i++) { //z toho priemeru podpriemernix
    if (inputPre[i] < avg) {
      avg2 += inputPre[i];
      j++;
    }
  }
  char staticBias = avg2 / j;

  for (i = 0; i < NUM_BUTTONS; i++) { //toto treba este spomalit
    if (10 > (inputPre[i] - staticBias) > 1)
      inputInbornBias[i] += 1;
    if ((inputPre[i] - staticBias) < -1)
      inputInbornBias[i] -= 1;
  }
}



