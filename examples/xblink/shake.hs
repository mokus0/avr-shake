#!/usr/bin/env runhaskell
module Main where

import Development.Shake
import Development.Shake.AVR
import Development.Shake.FilePath

device          = "atxmega128a4u"
clock           = round 32e6

avrdudeFlags    = ["-c", "flip2"]

cFlags = ["-Wall", "-Os", 
    "-DF_CPU=" ++ show clock ++ "UL",
    "-mmcu=" ++ device]

main = shakeArgs shakeOptions $ do
    want ["blink.elf"]
    
    "clean" ~> removeFilesAfter "." ["*.o", "*.elf"]
    "flash" ~> avrdude device avrdudeFlags (w Application "blink.elf")
    
    "blink.elf" *> \out -> do
        srcs <- getDirectoryFiles "." ["*.c"]
        let objs = map (<.> "o") srcs
        avr_ld' "avr-gcc" cFlags objs out
    
    "*.o" *> \out -> do
        avr_gcc cFlags (dropExtension out) out
