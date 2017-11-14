# Adafruit_MP3
This library sets up and performs *native* MP3 audio decoding on various Arduino-compatible platforms including Metro/Feather M4 (SAMD51 chip), teensy 3.6, teensy 3.2, and teensy 3.1. Audio output via the DAC pins. Uses Helix as underlying decoding. On the SAMD51 boards, TC2 is used to create the sample timer (usually 44.1khz or 48khz). Uses a decent chunk of RAM at this time!

See [documentation](https://adafruit.github.io/Adafruit_MP3/classAdafruit__mp3.html).
