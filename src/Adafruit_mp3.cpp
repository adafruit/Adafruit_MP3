﻿#include "Adafruit_mp3.h"

#define WAIT_TC16_REGS_SYNC(x) while(x->COUNT16.SYNCBUSY.bit.ENABLE);

bool activeOutbuf;
Adafruit_mp3_outbuf outbufs[2];
volatile int16_t *outptr;
static void (*sampleReadyCallback)(int16_t, int16_t);
	
volatile uint8_t channels;

static inline void enableTimer(){
	
	MP3_TC->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE;
	WAIT_TC16_REGS_SYNC(MP3_TC)
}

static inline void disableTimer(){
	
	MP3_TC->COUNT16.CTRLA.bit.ENABLE = 0;
	WAIT_TC16_REGS_SYNC(MP3_TC)
}

static inline void resetTC (Tc* TCx)
{
	// Disable TCx
	TCx->COUNT16.CTRLA.reg &= ~TC_CTRLA_ENABLE;
	WAIT_TC16_REGS_SYNC(TCx)

	// Reset TCx
	TCx->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
	WAIT_TC16_REGS_SYNC(TCx)
	while (TCx->COUNT16.CTRLA.bit.SWRST);
}

bool Adafruit_mp3::begin()
{	
	sampleReadyCallback = NULL;
	bufferCallback = NULL;
	
	NVIC_DisableIRQ(MP3_IRQn);
	NVIC_ClearPendingIRQ(MP3_IRQn);
	NVIC_SetPriority(MP3_IRQn, 0);
	
	GCLK->PCHCTRL[MP3_GCLK_ID].reg = GCLK_PCHCTRL_GEN_GCLK0_Val | (1 << GCLK_PCHCTRL_CHEN_Pos);
	
	resetTC(MP3_TC);
	
	//configure timer for 44.1khz
	MP3_TC->COUNT16.WAVE.reg = TC_WAVE_WAVEGEN_MFRQ;  // Set TONE_TC mode as match frequency

	MP3_TC->COUNT16.CTRLA.reg |= TC_CTRLA_MODE_COUNT16 | TC_CTRLA_PRESCALER_DIV4;
	WAIT_TC16_REGS_SYNC(MP3_TC)

	//TODO: calculate based on timer clock
	MP3_TC->COUNT16.CC[0].reg = (uint16_t) 650;
	WAIT_TC16_REGS_SYNC(MP3_TC)

	// Enable the TONE_TC interrupt request
	MP3_TC->COUNT16.INTENSET.bit.MC0 = 1;
	
	if ((hMP3Decoder = MP3InitDecoder()) == 0)
	{
		return false;
	}
}

void Adafruit_mp3::setSampleReadyCallback(void (*fun_ptr)(int16_t, int16_t)) { sampleReadyCallback = fun_ptr; }

void Adafruit_mp3::play()
{
	//start the playback timer
	enableTimer();
	NVIC_EnableIRQ(MP3_IRQn);
	bytesLeft = 0;
	activeOutbuf = 0;
	readPtr = inBuf;
	
	outbufs[0].count = 0;
	outbufs[1].count = 0;
	playing = false;
}

void Adafruit_mp3::tick(){
		
	if(outbufs[activeOutbuf].count == 0 && outbufs[!activeOutbuf].count > 0){
		//time to swap the buffers
		activeOutbuf = !activeOutbuf;
		outptr = outbufs[activeOutbuf].buffer;
	}
	
	//if we are running out of samples, and don't yet have another buffer ready, get busy.
	if(outbufs[activeOutbuf].count < BUFFER_LOWER_THRESH && outbufs[!activeOutbuf].count == 0){
		
		//dumb, but we need to move any bytes to the beginning of the buffer
		if(readPtr != inBuf){
			memmove(inBuf, readPtr, bytesLeft);
			readPtr = inBuf;
		}
		
		//get more data from the user application
		if(bufferCallback != NULL)
			bytesLeft += bufferCallback(readPtr + bytesLeft, (INBUF_SIZE - bytesLeft));
		
		MP3FrameInfo frameInfo;
		int err, offset;
		if(!playing){
			err = MP3GetNextFrameInfo(hMP3Decoder, &frameInfo, readPtr);
			if(frameInfo.samprate != 44100)
			{
				// For this example, we want only data which
				// was sampled at 44100 Hz. Ignore this frame.
				__BKPT();
			}
			channels = frameInfo.nChans;
		}
		playing = true;
		
		/* Find start of next MP3 frame. Assume EOF if no sync found. */
		offset = MP3FindSyncWord(readPtr, bytesLeft);

		if(offset >= 0){
			readPtr += offset;
			bytesLeft -= offset;
				
			//fil the inactive outbuffer
			int bytesLeftBeforeDecode = bytesLeft;
			err = MP3Decode(hMP3Decoder, &readPtr, (int*) &bytesLeft, outbufs[!activeOutbuf].buffer, 0);
			outbufs[!activeOutbuf].count = bytesLeftBeforeDecode - bytesLeft;

			if (err) {
				__BKPT();
			}
		}
	}
}

void MP3_Handler()
{
	//disableTimer();
	
	if(outbufs[activeOutbuf].count > channels - 1){
		//it's sample time!
		if(sampleReadyCallback != NULL){
			if(channels == 1)
				sampleReadyCallback(*outptr, 0);
			else
				sampleReadyCallback(*outptr, *(outptr + 1));
			
			//increment the read position and decrement the remaining sample count
			outptr += channels;
			outbufs[activeOutbuf].count -= channels;
		}
	}
	//enableTimer();
	
	if (MP3_TC->COUNT16.INTFLAG.bit.MC0 == 1) {
		MP3_TC->COUNT16.INTFLAG.bit.MC0 = 1;
	}
}