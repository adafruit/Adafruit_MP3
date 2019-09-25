# Adafruit_MP3 [![Build Status](https://travis-ci.com/adafruit/Adafruit_MP3.svg?branch=master)](https://travis-ci.com/adafruit/Adafruit_MP3)

This library sets up and performs *native* MP3 audio decoding on various Arduino-compatible platforms including Metro/Feather M4 (SAMD51 chip), teensy 3.6, teensy 3.2, and teensy 3.1. Audio output via the DAC pins. Uses Helix as underlying decoding. On the SAMD51 boards, TC2 is used to create the sample timer (usually 44.1khz or 48khz). Uses a decent chunk of RAM at this time!

See [documentation](https://adafruit.github.io/Adafruit_MP3/classAdafruit__mp3.html).

## UPDATE 6 October 2018 ##

- Fixed issues with Teensy support - The code `NVIC_DisableIRQ(MP3_IRQn);` applies only to the feather and metro m4 but was applied to all boards which resulted in MP3\_IRQn being undefined. (Adafruit\_MP3.cpp)
- Added support for Teensy 3.5 - Added `defined(__MK64FX512__)` wherever `defined(__MK66FX1M0__)` was found
- Added/modified comments on compiler directives for Teensy boards
- Included test.mp3 sample file (58.3 KB)
- Fix applied to library and the "play_from_SD.ino" example only. *(Other examples may have the same errors so use the above notes to fix other examples if needed.)*

This release was tested with the Teensy 3.5 only. It is expected that it will work with all other supported boards. *(At least as supported as the previous version.)*

### Some notes regarding audio file format ###

The tutorial posted on [https://learn.adafruit.com/native-mp3-decoding-on-arduino](https://learn.adafruit.com/native-mp3-decoding-on-arduino "https://learn.adafruit.com/native-mp3-decoding-on-arduino") states:
> This function is called from an interrupt, so it should be short and sweet. It's getting called 44,100 to 48,000 times per second for most MP3 files.
 
This means that the MP3 files should be encoded within this range. The code is written for stereo MP3 files so if you have a mono MP3 file, it will not play back using this code as is.

Best practice for producing playable MP3 files

- Encode at constant 40kbps with a sample rate of 44,100 kHz
- Stereo only

Resources for encoding audio

- Online converter to strip audio from video or convert audio to MP3 or change bitrate - [https://online-audio-converter.com/](https://online-audio-converter.com/ "https://online-audio-converter.com/")
- Desktop software for conversion - [https://www.audacityteam.org/](https://www.audacityteam.org/ "Audacity")
- How to use Audacity to convert mono to stereo - [https://www.wikihow.com/Change-a-Mono-Track-Into-Stereo-Track-Using-Audacity](https://www.wikihow.com/Change-a-Mono-Track-Into-Stereo-Track-Using-Audacity)

Other notes



- SD Cards (at least on Teensy 3.5 & 3.6 need to be formatted with FAT32 file system. If a small card is used with FAT formatting, it will not work.
- If you receive errors with Teensy 3.5 & 3.6 code regarding the parameters for SD card initialization, look for warnings regarding the SD library. Most likely you have multiple libraries installed and the Teensy SD library is being skipped/ignored. If this is the case, you will need to move or remove the one being used, at least temporarily, so that the Teensy library is used by the Arduino IDE.



