avr-shake ![Build Status](https://travis-ci.org/mokus0/avr-shake.svg?branch=master)
==========

A set of shake build actions for use with avr-gcc as a project build tool for Atmel microcontrollers (specifically, those based on the "AVR" instruction set, which includes the "ATmega" series used by the original Arduino). The basic parts would probably work with any GCC setup, but it's really not very sophisticated. At this point the examples are probably of more interest than the library itself - the hardest part of setting up these builds tends to be deciding how to lay out the build directories, especially when integrating 3rd-party code and/or building for multiple target processors.

If you happen to be interested to see some specific applications, I have a few public projects using it (in increasing order of build-system complexity):

    * https://github.com/mokus0/avr-candle/blob/master/shake.hs

    * https://github.com/bondians/interocitor/blob/master/shake

    * https://github.com/bondians/vTree-2.9/blob/master/shake.hs
