#include <TimerTC3.h>
//#include <TimerTCC0.h>
//#include <stdlib.h> // div, div_t
//#include <avr/io.h>

//#include <Arduino.h>//midi
//#include <Adafruit_TinyUSB.h>
#include "MIDIUSB.h"
//#include <MIDI.h>

// USB MIDI object
//Adafruit_USBD_MIDI usb_midi;
//MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, usbMIDI);//midi

//char time = 0;

#define SIGNAL_STARTER 2 //starter, alebo sinxronizacne miesto
#define SIGNAL_CLOCK 3 //hodinove impulzi
#define SIGNAL_INPUT 1 //zbernica
#define SIGNAL_INPUT2 4 //zbernicca
#define SIGNAL_INPUT3 0 //zbernicca

#define NUM_BUTTONS  40 //48 kláves (realne ich je troxa menej)
#define CHORD_LOWEST_TONE 60
#define BASS_LOWEST_TONE 42


unsigned int input_raw[NUM_BUTTONS * 3];
unsigned char inputMidiMem[NUM_BUTTONS * 3];
int vstupmax[NUM_BUTTONS * 3];
unsigned int inputInbornBias[NUM_BUTTONS * 3]; // bias od poskladania (asi ho dam do EEPROM)
unsigned int onMem[NUM_BUTTONS * 3];
int inputFluctBias[NUM_BUTTONS * 3]; // dotiahnutie kludoveho stavu k nule
int inputStaticBias = 0; //spuosobení hlavne osvetlením
unsigned int kalibracia = 100;
int inputPre[NUM_BUTTONS * 3]; //predspracovane vstupi
int inputNorm[NUM_BUTTONS * 3]; //normalizovane vstupi
float inputNormLast[NUM_BUTTONS * 3]; //normalizovane vstupi stare
int buttonFunction[NUM_BUTTONS * 3] = {34, 21, 33, 16, 28, 23, 35, 18, 30, 13, 25, 20, 32, 15, 27, 22, 34, 17, 29, 12, 24, 19, 31, 14, 26, 21, 33, 16, 28, 23, 35, 18, 30, 13, 25, 20, 32, 15, 27, 22,
                                       50, 45, 57, 40, 52, 47, 59, 42, 54, 37, 49, 44, 56, 39, 51, 46, 58, 41, 53, 36, 48, 43, 55, 38, 50, 45, 57, 40, 52, 47, 59, 42, 54, 37, 49, 44, 56, 39, 51, 46,
                                       10, 1, 9, 8, 4, 3, 11, 10, 6, 5, 1, 0, 8, 7, 3, 2, 10, 9, 5, 4, 0, 11, 7, 6, 2, 1, 9, 8, 4, 3, 11, 10, 6, 5, 1, 0, 8, 7, 3, 2
                                      }; //nacita z eepromky ktori snimac ma ktori ton
int actualSensorNumber = 0;
bool cycleDone = 0;

#define BUFFER_SIZE 20
int sendBufferNr = 0;
int midiNoteBuf[BUFFER_SIZE];
int midiVeloBuf[BUFFER_SIZE];
int midiChBuf[BUFFER_SIZE];
bool midiOnBuf[BUFFER_SIZE];


void setup()
{
  //SerialUSB.begin(115200);
  //while(!SerialUSBUSB);
  TimerTc3.initialize(100);
  TimerTc3.attachInterrupt(timerIsr);

  pinMode(SIGNAL_INPUT, INPUT);  //zbernica z fototranzistorov
  pinMode(SIGNAL_INPUT2, INPUT);  //zbernica z fototranzistorov
  pinMode(SIGNAL_INPUT3, INPUT);  //zbernica z fototranzistorov
  pinMode(SIGNAL_CLOCK, OUTPUT);
  pinMode(SIGNAL_STARTER, OUTPUT); //na rozbehnutie potrebujem podrzať tento pin v 1, potom podla neho budem sinxronizovať
  digitalWrite(SIGNAL_STARTER, HIGH); // startovanie

  for (int i = 0; i < NUM_BUTTONS * 3; i++) vstupmax[i] = 80; //abi to hneď zo začiatku ňebolo take citlive

  for (int i = 0; i < 10; i++) //naseka 10 jednotiek do radu
  { digitalWrite(SIGNAL_CLOCK, HIGH);
    delay(1);
    digitalWrite(SIGNAL_CLOCK, LOW);
    delay(1);
  }
  digitalWrite(SIGNAL_STARTER, LOW);//vipne SIGNAL_STARTER
  pinMode(SIGNAL_STARTER, INPUT);

//  usbMIDI.begin();

  // Attach the handleNoteOn function to the MIDI Library. It will
  // be called whenever the Bluefruit receives MIDI Note On messages.
  //usbMIDI.setHandleNoteOn(handleNoteOn);

  // Do the same for MIDI Note Off messages.
  //usbMIDI.setHandleNoteOff(handleNoteOff);

  SerialUSB.begin(2000000);

  // wait until device mounted
 // while( !USBDevice.mounted() ) delay(1);
}

void loop()
{
  //PrintInt(analog2);
  if (cycleDone == 1)
  {
    if (kalibracia != 0) { //prve kola nekalibruje
      if (kalibracia < 20) { //poslednich 20 si robí priemer
        for (int x = 0; x < NUM_BUTTONS * 3; x++)  {
          inputInbornBias[x] = (inputInbornBias[x] * 3 + input_raw[x]) / 4;
        }
      }
      if (kalibracia == 1) SerialUSB.println("skalibrovane"); //posledne kolo vipise
      kalibracia--;
    }
    if (kalibracia == 0) {
      for (int i = 0; i < NUM_BUTTONS * 3; i++)  { //meria priebezne maxima vstupov
        vstupmax[i] = max(vstupmax[i], inputPre[i]); //toto bude velmi citlive na nahodne spicki, treba to spravit zivsie, alebo ukladat do pamete pri montazi
      }
    }
    cycleDone = 0;
  }

  if (sendBufferNr > 0)
  {
    sendMidiMessage();
  }
  //usbMIDI.read(); 
}


void timerIsr()
{
  digitalWrite(SIGNAL_CLOCK, LOW);  //delay(2);
  input_raw[actualSensorNumber]      = analogRead(SIGNAL_INPUT);
  input_raw[actualSensorNumber + 40] = analogRead(SIGNAL_INPUT2);
  input_raw[actualSensorNumber + 80] = analogRead(SIGNAL_INPUT3);
  digitalWrite(SIGNAL_CLOCK, HIGH);

  if (kalibracia == 0) {
    for (int y = 0; y < 3; y++)
    {
      int i = actualSensorNumber + 40 * y;
      
      inputNormLast[i] = inputNorm[i];
      inputPre[i] = input_raw[i] - inputInbornBias[i]; //predspracovane vstupi
      inputNorm[i] = inputPre[i] * 100 / vstupmax[i];
      
      inputNormLast[i]=sqrt((inputNorm[i] - inputNormLast[i]+1)/1.5)*15;
      
      /*if (i>=0 && i<80 && inputPre[i] > 15) {
        if (onMem[i] != 0) standardToMidi(buttonFunction[i], 127 , 1);
        onMem[i] = 1;
      }
      else */if (i>=80 && i<120 && inputNorm[i] > 30) {
        if (onMem[i] != 0) standardToMidi(buttonFunction[i], constrain(int(inputNormLast[i]), 20, 127) , 1);
        onMem[i] = 1;
      }
      if (onMem[i] == 0) { // ak pride 1 zahra ton, nastavi do nuly, ak je v nule tak uz nezahra a ak nepride znova 1 tak nastavi do -1 a vipne ton
        standardToMidi(buttonFunction[i], 0 , 0);
        onMem[i] = 2;
      }
      if (onMem[i] == 1) onMem[i] = 0;
    }
  }


  //skontroluje podmienky a da do bafera na odosielanie
  //ked budem menit víraz tónu, tak bude odosielať do bafra az po ukoncení tonu

  actualSensorNumber++;

  if (analogRead(SIGNAL_STARTER) > 400)   {  //tu snima SIGNAL_STARTER, podla neho sinchronizuje meranie mala by na nom bit log 1, ale ked je inputPre napajani tak radsej 400/1024
    actualSensorNumber = 0;
    cycleDone = 1;
    //PrintInt(1000);
  }
}


void standardToMidi(int number, int velocity, bool on) {
  if ((number >= 0) && (number < 12))
  {
    sendToMidiBuffer((((-BASS_LOWEST_TONE + number) % 12) + BASS_LOWEST_TONE), velocity, on, 0);
  }
  else if ((number >= 12) && (number < 24))
  {
    int tone1 = (((-CHORD_LOWEST_TONE + number) % 12) + CHORD_LOWEST_TONE);
    int tone2 = (((-CHORD_LOWEST_TONE + number + 4) % 12) + CHORD_LOWEST_TONE);
    int tone3 = (((-CHORD_LOWEST_TONE + number + 7) % 12) + CHORD_LOWEST_TONE);
    sendToMidiBuffer(tone1, 127, on, 2);
    sendToMidiBuffer(tone2, 127, on, 2);
    sendToMidiBuffer(tone3, 127, on, 2);
    }
  else if ((number >= 24) && (number < 36))
  {
    int tone1 = (((-CHORD_LOWEST_TONE + number) % 12) + CHORD_LOWEST_TONE);
    int tone2 = (((-CHORD_LOWEST_TONE + number + 3) % 12) + CHORD_LOWEST_TONE);
    int tone3 = (((-CHORD_LOWEST_TONE + number + 7) % 12) + CHORD_LOWEST_TONE);
    sendToMidiBuffer(tone1, 127, on, 2);
    sendToMidiBuffer(tone2, 127, on, 2);
    sendToMidiBuffer(tone3, 127, on, 2);
  }
  else if ((number >= 36) && (number < 48))
  {
    int tone1 = (((-CHORD_LOWEST_TONE + number) % 12) + CHORD_LOWEST_TONE);
    int tone2 = (((-CHORD_LOWEST_TONE + number + 4) % 12) + CHORD_LOWEST_TONE);
    int tone3 = (((-CHORD_LOWEST_TONE + number + 7) % 12) + CHORD_LOWEST_TONE);
    int tone4 = (((-CHORD_LOWEST_TONE + number + 10) % 12) + CHORD_LOWEST_TONE);
    sendToMidiBuffer(tone1, 127, on, 2);
    sendToMidiBuffer(tone2, 127, on, 2);
    sendToMidiBuffer(tone3, 127, on, 2);
    sendToMidiBuffer(tone4, 127, on, 2);
  }
  else if ((number >= 48) && (number < 60))
  {
    int tone1 = (((-CHORD_LOWEST_TONE + number) % 12) + CHORD_LOWEST_TONE);
    int tone2 = (((-CHORD_LOWEST_TONE + number + 3) % 12) + CHORD_LOWEST_TONE);
    int tone3 = (((-CHORD_LOWEST_TONE + number + 6) % 12) + CHORD_LOWEST_TONE);
    int tone4 = (((-CHORD_LOWEST_TONE + number + 9) % 12) + CHORD_LOWEST_TONE);
    sendToMidiBuffer(tone1, 127, on, 2);
    sendToMidiBuffer(tone2, 127, on, 2);
    sendToMidiBuffer(tone3, 127, on, 2);
    sendToMidiBuffer(tone4, 127, on, 2);
  }
}

void sendToMidiBuffer(int midiNote, int midiVelo, bool midiOn, int channel) {
  if (sendBufferNr < BUFFER_SIZE) {
    sendBufferNr++;
    midiNoteBuf[sendBufferNr] = midiNote;
    midiVeloBuf[sendBufferNr] = midiVelo;
    midiOnBuf[sendBufferNr] = midiOn;
    midiChBuf[sendBufferNr] = channel;
  }
}
void sendMidiMessage() {
    SerialUSB.print(midiNoteBuf[sendBufferNr]);
    int i=sendBufferNr;
    sendBufferNr--;
    if (midiOnBuf[i]) {
        noteOn(midiChBuf[i], midiNoteBuf[i], midiVeloBuf[i]);

        MidiUSB.flush();
      //usbMIDI.sendNoteOn(midiNoteBuf[i], midiVeloBuf[i], 1);
      SerialUSB.print(" ");
      SerialUSB.print(midiVeloBuf[i]);
      SerialUSB.println(" on ");
    }
    else {
      noteOff(midiChBuf[i], midiNoteBuf[i], 0);

        MidiUSB.flush();//usbMIDI.sendNoteOn(midiNoteBuf[i], 0, 1); 
      SerialUSB.println(" off ");
    }
    //midiNoteBuf[i] = midiNote;
    //midiVeloBuf[i] = midiVelo;
    //midiOnBuf[i] = midiOn;
    
}

void noteOn(byte channel, byte pitch, byte velocity) {

  midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};

  MidiUSB.sendMIDI(noteOn);
}

void noteOff(byte channel, byte pitch, byte velocity) {

  midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};

  MidiUSB.sendMIDI(noteOff);
}

