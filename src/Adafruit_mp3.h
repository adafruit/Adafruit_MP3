#ifndef LIB_ADAFRUIT_MP3_H
#define LIB_ADAFRUIT_MP3_H

#include "Arduino.h"
#include "arm_math.h"
#include "mp3dec.h"

//TODO: decide on a reasonable buffer size
#define OUTBUF_SIZE (16 * 1024)
#define INBUF_SIZE (16 * 1024)

#define BUFFER_LOWER_THRESH (8 * 1024)

#define MP3_SAMPLE_RATE_DEFAULT 44100

#define MP3_TC TC2
#define MP3_IRQn TC2_IRQn
#define MP3_Handler TC2_Handler
#define MP3_GCLK_ID TC2_GCLK_ID

struct Adafruit_mp3_outbuf {
	volatile int count;
	int16_t buffer[OUTBUF_SIZE];	
};

class Adafruit_mp3 {
public:
	Adafruit_mp3() : hMP3Decoder() { inbufend = (inBuf + INBUF_SIZE); }
	~Adafruit_mp3() { MP3FreeDecoder(hMP3Decoder); };
	bool begin();
	void setBufferCallback(int (*fun_ptr)(uint8_t *, int));
	void setSampleReadyCallback(void (*fun_ptr)(int16_t, int16_t));
		
	void play();
	void stop();
	void resume();
	
	int tick();
	
private:
	Tc *_tc;
	HMP3Decoder hMP3Decoder;
	
	volatile int bytesLeft;
	uint8_t *readPtr;
	uint8_t *writePtr;
	uint8_t inBuf[INBUF_SIZE];
	uint8_t *inbufend;
	bool playing = false;
	
	int (*bufferCallback)(uint8_t *, int);
	int findID3Offset(uint8_t *readPtr);
	
};

#endif