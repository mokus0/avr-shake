module Development.Shake.AVR
    ( avr_gcc,      avr_gcc'
    , avr_ld,       avr_ld'
    , avr_objcopy,  avr_objcopy'
    , avr_objdump,  avr_objdump'
    , avrdude,      avrdude'
    ) where

import Development.Shake
import Development.Shake.Command

gccDeps cc cFlags src = do
    Stdout cppOut <- command [Traced ""] cc (cFlags ++ ["-M", "-MG", "-E", src])
    return (filter (/= "\\") (drop 2 (words cppOut)))

avr_gcc = avr_gcc' "avr-gcc"
avr_gcc' cc cFlags src out = do
    need [src]
    need =<< gccDeps cc cFlags src
    system' cc (cFlags ++ ["-c", src, "-o", out])

avr_ld = avr_ld' "avr-ld"
avr_ld' ld ldFlags objs out = do
    need objs
    system' ld (ldFlags ++ ["-o", out] ++ objs)

avr_objcopy = avr_objcopy' "avr-objcopy"
avr_objcopy' objcopy fmt flags src out = do
    need [src]
    system' objcopy (flags ++ ["-O", fmt, src, out])

avr_objdump = avr_objdump' "avr-objdump"
avr_objdump' objdump src out = do
    need [src]
    Stdout lss <- command [] objdump ["-h", "-S", src]
    writeFileChanged out lss

avrdude = avrdude' "avrdude"
avrdude' avrdudeBin flags hex usbPort = do
    alwaysRerun
    need [usbPort, hex]
    system' avrdudeBin (flags ++ ["-P", usbPort, "-Uflash:w:" ++ hex])
