#include "Adafruit_MP3.h"
#include "sine.h"

#include <Adafruit_ZeroDMA.h>
#include "utility/dma.h"

#define VOLUME_MAX 2047

Adafruit_ZeroDMA myDMA;
ZeroDMAstatus    stat; // DMA status codes returned by some functions
Adafruit_MP3_DMA player;
DmacDescriptor *desc;

//the output data buffers
int16_t *ping, *pong;

uint8_t *currentPtr;
uint32_t thisManyBytesLeft;

//this gets called when the player wants more data
int getMoreData(uint8_t *writeHere, int thisManyBytes){
  int toWrite = min(thisManyBytesLeft, thisManyBytes);
  memcpy(writeHere, currentPtr, toWrite);
  currentPtr += toWrite;
  thisManyBytesLeft -= toWrite;
  return toWrite;
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
    myDMA.abort();
  }
}

// the setup routine runs once when you press reset:
void setup() {

  //set up the DMA channel
  myDMA.setTrigger(MP3_DMA_TRIGGER);
  myDMA.setAction(DMA_TRIGGER_ACTON_BEAT);
  stat = myDMA.allocate();

  //ask for the buffers we're going to use
  player.getBuffers(&ping, &pong);

  //make the descriptors
  desc = myDMA.addDescriptor(
    ping,                    // move data from here
    (void *)(&DAC->DATA[0]), // to here (M4)
  MP3_OUTBUF_SIZE,                      // this many...
    DMA_BEAT_SIZE_HWORD,               // bytes/hword/words
    true,                             // increment source addr?
    false);                           // increment dest addr?
  desc->BTCTRL.bit.BLOCKACT = DMA_BLOCK_ACTION_INT;

  desc = myDMA.addDescriptor(
    pong,                    // move data from here
    (void *)(&DAC->DATA[0]), // to here (M4)
  MP3_OUTBUF_SIZE,                      // this many...
    DMA_BEAT_SIZE_HWORD,               // bytes/hword/words
    true,                             // increment source addr?
    false);                           // increment dest addr?
  desc->BTCTRL.bit.BLOCKACT = DMA_BLOCK_ACTION_INT;

  //make the descriptor list loop
  myDMA.loop(true);
  myDMA.setCallback(dma_callback);

  //set the DAC to the center of the range
  analogWrite(A0, 2048);

  currentPtr = (uint8_t*)sine_mp3;
  thisManyBytesLeft = sizeof(sine_mp3);

  //begin the player
  player.begin();

  //this will be how the player asks for data
  player.setBufferCallback(getMoreData);

  //this will be how the player asks you to clean the data
  player.setDecodeCallback(decodeCallback);

  player.play(); //this will automatically fill the first buffer

  //the DMA controller will dictate what happens from here on out
  myDMA.startJob();
}

// the loop routine runs over and over again forever:
void loop() {

}
