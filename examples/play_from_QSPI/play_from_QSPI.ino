#include "Adafruit_MP3.h"
#include <SPI.h>
#include <SdFat.h>
#include <Adafruit_SPIFlash.h>

#if defined(__SAMD51__) || defined(NRF52840_XXAA)
  Adafruit_FlashTransport_QSPI flashTransport(PIN_QSPI_SCK, PIN_QSPI_CS, PIN_QSPI_IO0, PIN_QSPI_IO1, PIN_QSPI_IO2, PIN_QSPI_IO3);
#else
  #if (SPI_INTERFACES_COUNT == 1 || defined(ADAFRUIT_CIRCUITPLAYGROUND_M0))
    Adafruit_FlashTransport_SPI flashTransport(SS, &SPI);
  #else
    Adafruit_FlashTransport_SPI flashTransport(SS1, &SPI1);
  #endif
#endif

Adafruit_SPIFlash flash(&flashTransport);

// file system object from SdFat
FatFileSystem fatfs;

//set this to a value between 0 and 4095 to raise/lower volume
#define VOLUME_MAX 1023

const char *filename = "test.mp3";

File dataFile;
Adafruit_MP3 player;

void writeDacs(int16_t l, int16_t r){
  uint16_t vall = map(l, -32768, 32767, 0, VOLUME_MAX);
  uint16_t valr = map(r, -32768, 32767, 0, VOLUME_MAX);
#if defined(__SAMD51__) // feather/metro m4
  analogWrite(A0, vall);
  analogWrite(A1, valr);
#elif defined(__MK66FX1M0__)  // teensy 3.6
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

  analogWriteResolution(12);
#if defined(__SAMD51__)
  analogWrite(A0, 2048);
  analogWrite(A1, 2048);
#endif

  Serial.println("Native MP3 decoding!");
  Serial.print("Initializing QSPI storage...");

  // Initialize flash library and check its chip ID.
  if (!flash.begin()) {
    Serial.println("Error, failed to initialize flash chip!");
    while(1);
  }
  
  Serial.println("QSPI initialized.");
  Serial.print("Flash chip JEDEC ID: 0x"); Serial.println(flash.getJEDECID(), HEX);

  // First call begin to mount the filesystem.  Check that it returns true
  // to make sure the filesystem was mounted.
  if (!fatfs.begin(&flash)) {
    Serial.println("Error, failed to mount newly formatted filesystem!");
    Serial.println("Was the flash chip formatted with the fatfs_format example?");
    while(1);
  }
  Serial.println("Mounted filesystem!");

  
  dataFile = fatfs.open(filename);
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