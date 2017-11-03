#include "Adafruit_mp3.h"
#include <SPI.h>
#include <SD.h>

const int chipSelect = 10;
const char *filename = "test.mp3";

File dataFile;
Adafruit_mp3 player;

void writeDacs(int16_t l, int16_t r){
  uint16_t vall = map(l, -32768, 32767, 0, 4095);
  uint16_t valr = map(r, -32768, 32767, 0, 4095);
  analogWrite(A0, vall);
  analogWrite(A1, valr);
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
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    while(1);
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
