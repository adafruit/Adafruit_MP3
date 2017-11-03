#include "Adafruit_mp3.h"
#include "sine.h"

Adafruit_mp3 player;

void writeDacs(int16_t l, int16_t r){
  uint8_t val = map(l, -32768, 32767, 0, 4095);
  analogWrite(A0, val);
}

uint8_t *currentPtr;
uint32_t thisManyBytesLeft;

int getMoreData(uint8_t *writeHere, int thisManyBytes){
  int toWrite = min(thisManyBytesLeft, thisManyBytes);
  memcpy(writeHere, currentPtr, toWrite);
  currentPtr += toWrite;
  thisManyBytesLeft -= toWrite;
  return toWrite;
}

// the setup routine runs once when you press reset:
void setup() {         
  
  currentPtr = (uint8_t*)sine_mp3;
  thisManyBytesLeft = sizeof(sine_mp3);
  
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