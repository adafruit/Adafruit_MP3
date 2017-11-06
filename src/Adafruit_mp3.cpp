#include "Adafruit_mp3.h"
#include "mp3common.h"

#define WAIT_TC16_REGS_SYNC(x) while(x->COUNT16.SYNCBUSY.bit.ENABLE);

volatile bool activeOutbuf;
Adafruit_mp3_outbuf outbufs[2];
volatile int16_t *outptr;
static void (*sampleReadyCallback)(int16_t, int16_t);
	
volatile uint8_t channels;

/**
 *****************************************************************************************
 *  @brief      enable the playback timer
 *
 *  @return     none
 ****************************************************************************************/
static inline void enableTimer(){
	
	MP3_TC->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE;
	WAIT_TC16_REGS_SYNC(MP3_TC)
}

/**
 *****************************************************************************************
 *  @brief      disable the playback timer
 *
 *  @return     none
 ****************************************************************************************/
static inline void disableTimer(){
	
	MP3_TC->COUNT16.CTRLA.bit.ENABLE = 0;
	WAIT_TC16_REGS_SYNC(MP3_TC)
}

/**
 *****************************************************************************************
 *  @brief      reset the playback timer
 *
 *  @return     none
 ****************************************************************************************/
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

/**
 *****************************************************************************************
 *  @brief      Begin the mp3 player. This initializes the playback timer and necessary interrupts.
 *
 *  @return     none
 ****************************************************************************************/
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
	MP3_TC->COUNT16.CC[0].reg = (uint16_t)( (SystemCoreClock >> 2) / MP3_SAMPLE_RATE_DEFAULT);
	WAIT_TC16_REGS_SYNC(MP3_TC)

	// Enable the TONE_TC interrupt request
	MP3_TC->COUNT16.INTENSET.bit.MC0 = 1;
	
	if ((hMP3Decoder = MP3InitDecoder()) == 0)
	{
		return false;
	}
}

/**
 *****************************************************************************************
 *  @brief      Set the function the player will call when it's buffers need to be filled. 
 *				Care must be taken to ensure that the callback function is efficient.
 *				If the callback takes too long to fill the buffer, playback will be choppy
 *
 *	@param		fun_ptr the pointer to the callback function. This function must take a pointer
 *				to the location the bytes will be written, as well as an integer that represents
 *				the maximum possible bytes that can be written. The function should return the 
 *				actual number of bytes that were written.
 *
 *  @return     none
 ****************************************************************************************/
void Adafruit_mp3::setBufferCallback(int (*fun_ptr)(uint8_t *, int)){ bufferCallback = fun_ptr; }

/**
 *****************************************************************************************
 *  @brief      Set the function that the player will call when the playback timer fires.
 *				The callback is called inside of an ISR, so it should be short and efficient.
 *				This will usually just be writing samples to the DAC.
 *
 *	@param		fun_ptr the pointer to the callback function. The function must take two 
 *				unsigned 16 bit integers. The first argument to the callback will be the
 *				left channel sample, and the second channel will be the right channel sample.
 *				If the played file is mono, only the left channel data is used.
 *
 *  @return     none
 ****************************************************************************************/
void Adafruit_mp3::setSampleReadyCallback(void (*fun_ptr)(int16_t, int16_t)) { sampleReadyCallback = fun_ptr; }

/**
 *****************************************************************************************
 *  @brief      Play an mp3 file. This function resets the buffers and should only be used
 *				when beginning playback of a new mp3 file. If playback has been stopped
 *				and you would like to resume playback at the current location, use Adafruit_mp3::resume instead.
 *
 *  @return     none
 ****************************************************************************************/
void Adafruit_mp3::play()
{
	bytesLeft = 0;
	activeOutbuf = 0;
	readPtr = inBuf;
	writePtr = inBuf;
	
	outbufs[0].count = 0;
	outbufs[1].count = 0;
	playing = false;

	//start the playback timer
	enableTimer();
	NVIC_EnableIRQ(MP3_IRQn);
}

/**
 *****************************************************************************************
 *  @brief      Stop playback. This function stops the playback timer.
 *
 *  @return     none
 ****************************************************************************************/
void Adafruit_mp3::stop()
{
	disableTimer();
}

/**
 *****************************************************************************************
 *  @brief      Resume playback. This function re-enables the playback timer. If you are
 *				starting playback of a new file, use Adafruit_mp3::play instead
 *
 *  @return     none
 ****************************************************************************************/
void Adafruit_mp3::resume()
{
	enableTimer();
}

/**
 *****************************************************************************************
 *  @brief      Get the number of bytes until the end of the ID3 tag.
 *
 *	@param		readPtr current read pointer
 *
 *  @return     none
 ****************************************************************************************/
int Adafruit_mp3::findID3Offset(uint8_t *readPtr)
{
	char header[10];
	memcpy(header, readPtr, 10);
	//http://id3.org/id3v2.3.0#ID3v2_header
	if(header[0] == 0x49 && header[1] == 0x44 && header[2] == 0x33 && header[3] < 0xFF){
		//this is a tag
		uint32_t sz = ((uint32_t)header[6] << 23) | ((uint32_t)header[7] << 15) | ((uint32_t)header[8] << 7) | header[9];
		return sz;
	}
	else{
		//this is not a tag
		return 0;
	}
}

/**
 *****************************************************************************************
 *  @brief      The main loop of the mp3 player. This function should be called as fast as
 *				possible in the loop() function of your sketch. This checks to see if the
 *				buffers need to be filled, and calls the buffer callback function if necessary.
 *				It also calls the functions to decode another frame of mp3 data.
 *
 *  @return     none
 ****************************************************************************************/
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
				if(frameInfo.samprate != MP3_SAMPLE_RATE_DEFAULT)
				{
					disableTimer();
					MP3_TC->COUNT16.CC[0].reg = (uint16_t)( (SystemCoreClock >> 2) / frameInfo.samprate);
					WAIT_TC16_REGS_SYNC(MP3_TC);
					enableTimer();
				}
				playing = true;
				channels = frameInfo.nChans;
			}
			else return 1; //we couldn't find a frame header. It might be ok once we get more data though.
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

/**
 *****************************************************************************************
 *  @brief      The IRQ function that gets called whenever the playback timer fires.
 *
 *  @return     none
 ****************************************************************************************/
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