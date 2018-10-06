#include "Adafruit_MP3.h"
#include <SPI.h>
#include <SD.h>

//set this to a value between 0 and 4095 to raise/lower volume
#define VOLUME_MAX 1023

#if defined(__MK66FX1M0__) || defined(__MK64FX512__)  // teensy 3.6 or 3.5
const int chipSelect = BUILTIN_SDCARD ;
#elif defined(__MK20DX256__) //teensy 3.1
const int chipSelect = 10;
#else
const int chipSelect = 11;
#endif
const char *filename = "test.mp3";

File dataFile;
Adafruit_MP3 player;

void writeDacs(int16_t l, int16_t r){
  uint16_t vall = map(l, -32768, 32767, 0, VOLUME_MAX);
  uint16_t valr = map(r, -32768, 32767, 0, VOLUME_MAX);
#if defined(__SAMD51__) // feather/metro m4
  analogWrite(A0, vall);
  analogWrite(A1, valr);
#elif defined(__MK66FX1M0__) || defined(__MK64FX512__)  // teensy 3.6 or 3.5
  analogWrite(A21, vall);
  analogWrite(A22, valr);
#elif defined(NRF52)
  analogWrite(27, vall);
#elif defined(__MK20DX256__) //teensy 3.2
  analogWrite(A14, vall); //this board only has one dac, so play only left channel (or mono)
#endif
}

int getMoreData(uint8_t *writeHere, int thisManyBytes){
  int bytesRead = 0;
  int toRead = min(thisManyBytes, 768); //limit the number of bytes we can read at a time so the file isn't interrupted
  while(dataFile.available() && bytesRead < toRead){
    *writeHere = dataFile.read();
    writeHere++;
    bytesRead++;
  }
  return bytesRead;
}

// the setup routine runs once when you press reset:
void setup() {

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Serial.println("Native MP3 decoding!");
  Serial.print("Initializing SD card...");

  // see if the card is present and can be initialized:
#if defined(__MK66FX1M0__) || defined(__MK64FX512__) || defined(__MK20DX256__)  // teensy 3.6, 3.5, or 3.1/2
  analogWriteResolution(12);
  while (!SD.begin(chipSelect)) {
#else
  while (!SD.begin(12000000, chipSelect)) {
#endif
    Serial.println("Card failed, or not present");
    delay(2000);
  }
  Serial.println("card initialized.");
  
  dataFile = SD.open(filename);
  if(!dataFile){
    Serial.println("could not open file!");
    while(1);
  }
  
  player.begin();
  
  //do this when there are samples ready
  player.setSampleReadyCallback(writeDacs);
  
  //do this when more data is required
  player.setBufferCallback(getMoreData);
  
  player.play();
}

// the loop routine runs over and over again forever:
void loop() {
  player.tick();
}