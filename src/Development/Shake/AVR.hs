module Development.Shake.AVR
    ( avr_gcc,      avr_gcc'
    , avr_ld,       avr_ld'
    , avr_objcopy,  avr_objcopy'
    , avr_objdump,  avr_objdump'
    , avr_size,     avr_size'
    
    , avrdude,      avrdude'
    , AVRDUDE.MemType(..)
    , AVRDUDE.Dir(..)
    , AVRDUDE.Op(..)
    , AVRDUDE.Format(..)
    , AVRDUDE.ActionsM
    , AVRDUDE.Actions
    , AVRDUDE.action
    , AVRDUDE.r, AVRDUDE.v, AVRDUDE.w, AVRDUDE.imm
    ) where

import Development.Shake
import qualified System.Command.AVRDUDE as AVRDUDE

gccDeps cc cFlags src = do
    Stdout cppOut <- command [Traced ""] cc (cFlags ++ ["-M", "-MG", "-E", src])
    return (filter (/= "\\") (drop 2 (words cppOut)))

avr_gcc = avr_gcc' "avr-gcc"
avr_gcc' cc cFlags src out = do
    need [src]
    need =<< gccDeps cc cFlags src
    command_ [] cc (cFlags ++ ["-c", src, "-o", out])

avr_ld = avr_ld' "avr-ld"
avr_ld' ld ldFlags objs out = do
    need objs
    command_ [] ld (ldFlags ++ ["-o", out] ++ objs)

avr_objcopy = avr_objcopy' "avr-objcopy"
avr_objcopy' objcopy fmt flags src out = do
    need [src]
    command_ [] objcopy (flags ++ ["-O", fmt, src, out])

avr_objdump = avr_objdump' "avr-objdump"
avr_objdump' objdump src out = do
    need [src]
    Stdout lss <- command [] objdump ["-h", "-S", src]
    writeFileChanged out lss

avr_size = avr_size' "avr-size"
avr_size' avrsizeBin src = do
    need [src]
    Stdout lss <- command [] avrsizeBin [src]
    putNormal lss

avrdude = avrdude' "avrdude"
avrdude' avrdudeBin mcu opts actions = do
    alwaysRerun
    need (fst (AVRDUDE.actionFiles actions))
    command_ [] avrdudeBin (["-p", mcu] ++ opts ++ AVRDUDE.encodeActions actions)
