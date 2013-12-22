#!/usr/bin/env runhaskell
module Main where

import Development.Shake
import Development.Shake.AVR
import Development.Shake.FilePath

device          = "atxmega16a4u"
clock           = round 32e6

avrdudeFlags    = ["-b", "115200", "-c", "dragon_pdi", "-p", device]
usbPort         = "usb"

cFlags = ["-Wall", "-Os", 
    "-DF_CPU=" ++ show clock ++ "UL",
    "-mmcu=" ++ device]

main = shakeArgs shakeOptions $ do
    want ["blink.hex"]
    
    phony "clean" $ do
        removeFilesAfter "." ["*.o", "*.elf", "*.hex"]
    
    phony "flash" $ do
        need ["blink.hex"]
        avrdude avrdudeFlags "blink.hex" usbPort
    
    "blink.elf" *> \out -> do
        srcs <- getDirectoryFiles "." ["*.c"]
        let objs = [src `replaceExtension` "o" | src <- srcs]
        avr_ld' "avr-gcc" cFlags objs out
    
    "*.hex" *> \out -> do
        let elf = out `replaceExtension` "elf"
        avr_objcopy "ihex" ["-j", ".text", "-j", ".data"] elf out
    
    "*.o" *> \out -> do
        let src = out `replaceExtension` "c"
        avr_gcc cFlags src out
