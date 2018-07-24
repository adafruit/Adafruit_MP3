/* 
 *  Play a stereo MP3 File from an SD card 
 */
#include "Adafruit_MP3.h"
#include <SPI.h>
#include <SD.h>

#include <Adafruit_ZeroDMA.h>
#include "utility/dma.h"

#define VOLUME_MAX 2047
const char *filename = "test.mp3";
const int chipSelect = 10;

Adafruit_ZeroDMA leftDMA;
Adafruit_ZeroDMA rightDMA;
ZeroDMAstatus    stat; // DMA status codes returned by some functions
File dataFile;
Adafruit_MP3_DMA player;
DmacDescriptor *desc;

//the output data buffers
int16_t *ping, *pong;

//this gets called when the player wants more data
int getMoreData(uint8_t *writeHere, int thisManyBytes){
  int bytesRead = 0;
  while(dataFile.available() && bytesRead < thisManyBytes){
    *writeHere = dataFile.read();
    writeHere++;
    bytesRead++;
  }
  return bytesRead;
}


//this will get called when data has been decoded
void decodeCallback(int16_t *data, int len){
  for(int i=0; i<len; i++){
    int val = map(*data, -32768, 32767, 0, VOLUME_MAX);
    *data++ = val;
  }
}

void dma_callback(Adafruit_ZeroDMA *dma) {
  
  //try to fill the next buffer
  if(player.fill()){
    //stop
    leftDMA.abort();
    rightDMA.abort();
  }
}

void doNothing(Adafruit_ZeroDMA *dma) {
  
}

// the setup routine runs once when you press reset:
void setup() {
  Serial.begin(9600);

  //######### LEFT CHANNEL DMA ##############//
  
  //set up the DMA channels
  leftDMA.setTrigger(MP3_DMA_TRIGGER);
  leftDMA.setAction(DMA_TRIGGER_ACTON_BEAT);
  stat = leftDMA.allocate();

  //ask for the buffers we're going to use
  player.getBuffers(&ping, &pong);

  //make the descriptors
  desc = leftDMA.addDescriptor(
    ping,                    // move data from here
    (void *)(&DAC->DATA[0]), // to here (M4)
    MP3_OUTBUF_SIZE >> 1,                      // this many...
    DMA_BEAT_SIZE_HWORD,               // bytes/hword/words
    true,                             // increment source addr?
    false,
    DMA_ADDRESS_INCREMENT_STEP_SIZE_2,
    DMA_STEPSEL_SRC);                           // increment dest addr?
  desc->BTCTRL.bit.BLOCKACT = DMA_BLOCK_ACTION_INT;

  desc = leftDMA.addDescriptor(
    pong,                    // move data from here
    (void *)(&DAC->DATA[0]), // to here (M4)
    MP3_OUTBUF_SIZE >> 1,                      // this many...
    DMA_BEAT_SIZE_HWORD,               // bytes/hword/words
    true,                             // increment source addr?
    false,
    DMA_ADDRESS_INCREMENT_STEP_SIZE_2,
    DMA_STEPSEL_SRC);                           // increment dest addr?
  desc->BTCTRL.bit.BLOCKACT = DMA_BLOCK_ACTION_INT;

  //make the descriptor list loop
  leftDMA.loop(true);
  leftDMA.setCallback(dma_callback);

  //######### RIGHT CHANNEL DMA ##############//

  rightDMA.setTrigger(MP3_DMA_TRIGGER);
  rightDMA.setAction(DMA_TRIGGER_ACTON_BEAT);
  stat = rightDMA.allocate();

  //make the descriptors
  desc = rightDMA.addDescriptor(
    ping + 1,                    // move data from here
    (void *)(&DAC->DATA[1]), // to here (M4)
    MP3_OUTBUF_SIZE >> 1,                      // this many...
    DMA_BEAT_SIZE_HWORD,               // bytes/hword/words
    true,                             // increment source addr?
    false,
    DMA_ADDRESS_INCREMENT_STEP_SIZE_2,
    DMA_STEPSEL_SRC);                           // increment dest addr?
  desc->BTCTRL.bit.BLOCKACT = DMA_BLOCK_ACTION_INT;

  desc = rightDMA.addDescriptor(
    pong + 1,                    // move data from here
    (void *)(&DAC->DATA[1]), // to here (M4)
    MP3_OUTBUF_SIZE >> 1,                      // this many...
    DMA_BEAT_SIZE_HWORD,               // bytes/hword/words
    true,                             // increment source addr?
    false,
    DMA_ADDRESS_INCREMENT_STEP_SIZE_2,
    DMA_STEPSEL_SRC);                           // increment dest addr?
  desc->BTCTRL.bit.BLOCKACT = DMA_BLOCK_ACTION_INT;

  //make the descriptor list loop
  rightDMA.loop(true);
  rightDMA.setCallback(doNothing);

  
  while (!SD.begin(12000000, chipSelect)) {
    Serial.println("Card failed, or not present");
    delay(2000);
  }
  Serial.println("card initialized.");

  dataFile = SD.open(filename);
  if(!dataFile){
    Serial.println("could not open file!");
    while(1);
  }

  //set the DAC to the center of the range
  analogWrite(A0, 2048);
  analogWrite(A1, 2048);

  //begin the player
  player.begin();

  //this will be how the player asks for data
  player.setBufferCallback(getMoreData);

  //this will be how the player asks you to clean the data
  player.setDecodeCallback(decodeCallback);

  player.play(); //this will automatically fill the first buffer

  //the DMA controller will dictate what happens from here on out
  rightDMA.startJob();
  leftDMA.startJob();
}

// the loop routine runs over and over again forever:
void loop() {

}
