#!/usr/bin/env runhaskell
module Main where

import Control.Monad
import Data.List
import Development.Shake
import Development.Shake.AVR
import Development.Shake.FilePath

srcDir          = "src"
asfDir          = "asf"
-- probably can't just wget this, atmel wants you to log in.
-- git repo hasnt' been updated since 3.5.2, though, so for this
-- build it's probably necessary to manually download the latest
-- ASF directly from Atmel.
asfRemoteURL    = "http://www.atmel.com/images/asf-standalone-archive-3.25.0.20.zip"
buildRoot       = "build"

localBuildDir   = buildRoot </> "local"
asfBuildDir     = buildRoot </> "asf"

elfFile         = "hid.elf"
mapFile         = "hid.map"

commonFlags     = ["-pipe"] ++ optFlags
optFlags        = ["-Os", "-ffunction-sections", "-fdata-sections", "-fno-strict-aliasing"]

cppFlags        = ["-Iconf", "-Isrc"] ++ asfDefines ++ map (("-I" ++) . (asfDir </>)) asfIncludes

cFlags = commonFlags ++ cppFlags ++ ["-Wall", "-Werror", "-std=gnu99", "-mcpu=cortex-m0", "-mthumb"]

asFlags = commonFlags ++ cppFlags
    ++ ["-x", "assembler-with-cpp", "-mrelax", "-D__ASSEMBLY__"]
    ++ map (("-Wa,-I" ++) . (asfDir </>)) asfIncludes

ldFlags = commonFlags ++ ["-Wl,--relax", "-Wl,--gc-sections", "-Wl,--section-start=.BOOT=0x20000"]
    ++ ["-Wl,-Map=" ++ mapFile ++ ",--cref"]
    ++ ["-Wl,--entry=Reset_Handler"]
    ++ ["-Wl,--cref"]

asfDefines =
    [ "-DBOARD=SAMD11_XPLAINED_PRO"
    , "-D__SAMD11D14AM__"
    , "-DEXTINT_CALLBACK_MODE=true"
    , "-Dprintf=iprintf"
    ]

asfIncludes =
    [ "common/boards"
    , "common/services/sleepmgr"
    , "common/services/usb"
    , "common/services/usb/class/hid"
    , "common/services/usb/class/hid/device"
    , "common/services/usb/class/hid/device/generic"
    , "common/services/usb/udc"
    , "common/utils"
    , "sam0/boards"
    , "sam0/drivers/extint"
    , "sam0/drivers/port"
    , "sam0/drivers/system"
    , "sam0/drivers/system/clock"
    , "sam0/drivers/system/clock/clock_samd10_d11"
    , "sam0/drivers/system/interrupt"
    , "sam0/drivers/system/interrupt/system_interrupt_samd10_d11"
    , "sam0/drivers/system/pinmux"
    , "sam0/drivers/system/power/power_sam_d_r"
    , "sam0/drivers/system/reset/reset_sam_d_r"
    , "sam0/drivers/usb"
    , "sam0/drivers/usb/stack_interface"
    , "sam0/utils"
    , "sam0/utils/cmsis/samd11/source"
    , "sam0/utils/cmsis/samd11/include"
    , "sam0/utils/header_files"
    , "sam0/utils/preprocessor"
    , "thirdparty/CMSIS/Include"
    ]

asfSources = 
    [ "common/services/sleepmgr/samd/sleepmgr.c"
    , "common/services/usb/class/hid/device/generic/example/samd11d14a_samd11_xplained_pro/ui.c"
    , "common/services/usb/class/hid/device/generic/udi_hid_generic.c"
    , "common/services/usb/class/hid/device/generic/udi_hid_generic_desc.c"
    , "common/services/usb/class/hid/device/udi_hid.c"
    , "common/services/usb/udc/udc.c"
    , "common/utils/interrupt/interrupt_sam_nvic.c"
    , "sam0/drivers/extint/extint_sam_d_r/extint.c"
    , "sam0/drivers/system/clock/clock_samd10_d11/clock.c"
    , "sam0/drivers/system/clock/clock_samd10_d11/gclk.c"
    , "sam0/drivers/system/interrupt/system_interrupt.c"
    , "sam0/drivers/system/pinmux/pinmux.c"
    , "sam0/drivers/system/system.c"
    , "sam0/drivers/usb/stack_interface/usb_device_udd.c"
    , "sam0/drivers/usb/stack_interface/usb_dual.c"
    , "sam0/drivers/usb/usb_sam_d_r/usb.c"
    , "sam0/utils/syscalls/gcc/syscalls.c"
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
            ".c" -> avr_gcc' "arm-none-eabi-gcc" cFlags src out
            ".s" -> avr_gcc' "arm-none-eabi-gcc" asFlags src out
            _   -> fail ("don't know how to compile this source: " ++ show src)

main = shakeArgs shakeOptions $ do
    want ["size"]
    
    "size"      ~> avr_size elfFile
    "clean"     ~> removeFilesAfter "." [elfFile, mapFile, buildRoot]
    
    -- asfDir *> \out -> do
    --     exists <- doesDirectoryExist out
    --     when (not exists) $ command_ [] "git" ["clone", asfRemoteURL, out]
    
    [elfFile, mapFile] &*> \_ -> do
        need [asfDir]
        localSources <- getDirectoryFiles srcDir ["//*.c"]
        let localObjs = [localBuildDir </> src <.> "o" | src <- localSources]
            asfObjs   = [asfBuildDir   </> src <.> "o" | src <- asfSources]
            cmsisLib  = ["asf/thirdparty/CMSIS/Lib/GCC/libarm_cortexM0l_math.a"]
        avr_ld' "arm-none-eabi-gcc" ldFlags (localObjs ++ asfObjs ++ cmsisLib) elfFile
    
    compileRules asfDir asfBuildDir
    compileRules srcDir localBuildDir
