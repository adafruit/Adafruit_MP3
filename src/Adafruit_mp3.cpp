#include "Adafruit_mp3.h"
#include "mp3common.h"

#define WAIT_TC16_REGS_SYNC(x) while(x->COUNT16.SYNCBUSY.bit.ENABLE);

volatile bool activeOutbuf;
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
	writePtr = inBuf;
	
	outbufs[0].count = 0;
	outbufs[1].count = 0;
	playing = false;
}

int Adafruit_mp3::tick(){
	noInterrupts();
	if(outbufs[activeOutbuf].count == 0 && outbufs[!activeOutbuf].count > 0){
		//time to swap the buffers
		activeOutbuf = !activeOutbuf;
		outptr = outbufs[activeOutbuf].buffer;
	}
	interrupts();
	
	//if we are running out of samples, and don't yet have another buffer ready, get busy.
	if(outbufs[activeOutbuf].count < BUFFER_LOWER_THRESH && outbufs[!activeOutbuf].count < (OUTBUF_SIZE) >> 1){
		
		//dumb, but we need to move any bytes to the beginning of the buffer
		if(readPtr != inBuf && bytesLeft < BUFFER_LOWER_THRESH){
			memmove(inBuf, readPtr, bytesLeft);
			readPtr = inBuf;
			writePtr = inBuf + bytesLeft;
		}
		
		//get more data from the user application
		if(bufferCallback != NULL){
			if(inbufend - writePtr > 0){
				int bytesRead = bufferCallback(writePtr, inbufend - writePtr);
				writePtr += bytesRead;
				bytesLeft += bytesRead;
			}
		}
		
		MP3FrameInfo frameInfo;
		int err, offset;
		
		while(!playing){
			/* Find start of next MP3 frame. Assume EOF if no sync found. */
			offset = MP3FindSyncWord(readPtr, bytesLeft);
			if(offset >= 0){
				readPtr += offset;
				bytesLeft -= offset;
			}
			
			err = MP3GetNextFrameInfo(hMP3Decoder, &frameInfo, readPtr);
			if(err != ERR_MP3_INVALID_FRAMEHEADER){
				if(frameInfo.samprate != 44100)
				{
					//TODO: set the output timer for the sample rate of the file
					// For this example, we want only data which
					// was sampled at 44100 Hz. Ignore this frame.
					return 1;
				}
				else{
					playing = true;
					channels = frameInfo.nChans;
				}
			}
		}
		
		offset = MP3FindSyncWord(readPtr, bytesLeft);
		if(offset >= 0){
			readPtr += offset;
			bytesLeft -= offset;
				
			//fil the inactive outbuffer
			err = MP3Decode(hMP3Decoder, &readPtr, (int*) &bytesLeft, outbufs[!activeOutbuf].buffer + outbufs[!activeOutbuf].count, 0);
			MP3DecInfo *mp3DecInfo = (MP3DecInfo *)hMP3Decoder;
			outbufs[!activeOutbuf].count += mp3DecInfo->nGrans * mp3DecInfo->nGranSamps * mp3DecInfo->nChans;

			if (err) {
				return err;
			}
		}
	}
	return 0;
}

void MP3_Handler()
{
	//disableTimer();
	
	if(outbufs[activeOutbuf].count >= channels){
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