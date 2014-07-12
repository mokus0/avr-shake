#!/usr/bin/env runhaskell
module Main where

import Control.Monad
import Data.List
import Development.Shake
import Development.Shake.AVR
import Development.Shake.FilePath

srcDir          = "src"
asfDir          = "asf"
asfRemoteURL    = "https://anonymous@spaces.atmel.com/git/asf"
buildRoot       = "build"

localBuildDir   = buildRoot </> "local"
asfBuildDir     = buildRoot </> "asf"

elfFile         = "dfu.elf"
mapFile         = "dfu.map"

device          = "atxmega128a4u"

avrdudeFlags    = ["-c", "dragon_pdi"]

commonFlags     = ["-pipe", "-mmcu=" ++ device] ++ optFlags
optFlags        = ["-Os", "-ffunction-sections", "-fdata-sections"]

cppFlags        = ["-Iconf", "-Isrc"] ++ asfDefines ++ map (("-I" ++) . (asfDir </>)) asfIncludes

cFlags = commonFlags ++ cppFlags
    ++ ["-Wall", "-Werror", "-mrelax", "-std=gnu99"]

asFlags = commonFlags ++ cppFlags
    ++ ["-x", "assembler-with-cpp", "-mrelax", "-D__ASSEMBLY__"]
    ++ map (("-Wa,-I" ++) . (asfDir </>)) asfIncludes

ldFlags = commonFlags
    ++ ["-Wl,--section-start=.text=0x20000"]
    ++ ["-Wl,--section-start=.BOOT=0x21fbc"]
    ++ ["-Wl,--relax", "-Wl,--gc-sections"]
    ++ ["-Wl,-Map=" ++ mapFile ++ ",--cref"]

asfDefines =
    [ "-DBOARD=USER_BOARD"
    ]

asfIncludes =
    [ "common/boards"
    , "common/boards/user_board"
    , "common/services/clock"
    , "common/services/cpu"
    , "common/services/isp/flip"
    , "common/services/usb"
    , "common/services/usb/class/dfu_flip"
    , "common/services/usb/class/dfu_flip/device"
    , "common/services/usb/udc"
    , "common/utils"
    , "xmega/drivers/cpu"
    , "xmega/drivers/nvm"
    , "xmega/drivers/pmic"
    , "xmega/drivers/sleep"
    , "xmega/utils"
    , "xmega/utils/preprocessor"
    ]

asfSources = 
    [ "common/services/clock/xmega/sysclk.c"
    , "common/services/usb/class/dfu_flip/device/bootloader/xmega/sysclk_opt.c"
    , "common/services/usb/class/dfu_flip/device/udi_dfu_atmel.c"
    , "common/services/usb/class/dfu_flip/device/udi_dfu_atmel_desc.c"
    , "common/services/usb/udc/udc.c"
    , "xmega/drivers/cpu/ccp.s"
    , "xmega/drivers/nvm/nvm.c"
    , "xmega/drivers/nvm/nvm_asm.s"
    , "xmega/drivers/usb/usb_device.c"
    ]

-- defines rules to compile from a source dir to a build dir, mirroring
-- the directory layout, appending '.o' to all source names, and
-- invoking known compilers as needed.
compileRules fromDir toDir = 
    toDir ++ "//*.o" *> \out -> do
        src <- case stripPrefix toDir (dropExtension out) of
            Just ('/':rest) -> return (fromDir </> rest)
            _               -> fail $ unwords
                ["Build rule matched", show out,
                "which does not start with", show toDir]
        
        case takeExtension src of
            ".c" -> avr_gcc cFlags src out
            ".s" -> avr_gcc asFlags src out
            _   -> fail ("don't know how to compile this source: " ++ show src)

main = shakeArgs shakeOptions $ do
    want ["size"]
    
    "size"      ~> avr_size elfFile
    "clean"     ~> removeFilesAfter "." [elfFile, mapFile, buildRoot]
    "veryclean" ~> do need ["clean"]; removeFilesAfter "." [asfDir]
    "flash"     ~> avrdude device avrdudeFlags (w Boot elfFile)
            
    "fuses"     ~> do
        avrdude device avrdudeFlags $ sequence_
            [ w (FuseN n) elfFile
            | n <- [1,2,4,5]
            ]
    
    asfDir *> \out -> do
        exists <- doesDirectoryExist out
        when (not exists) $ command_ [] "git" ["clone", asfRemoteURL, out]
    
    [elfFile, mapFile] &*> \_ -> do
        need [asfDir]
        localSources <- getDirectoryFiles srcDir ["//*.c"]
        let localObjs = [localBuildDir </> src <.> "o" | src <- localSources]
            asfObjs   = [asfBuildDir   </> src <.> "o" | src <- asfSources]
        avr_ld' "avr-gcc" ldFlags (localObjs ++ asfObjs) elfFile
    
    compileRules asfDir asfBuildDir
    compileRules srcDir localBuildDir
