{-# LANGUAGE DataKinds #-}
{-# LANGUAGE GADTs #-}
{-# LANGUAGE GeneralizedNewtypeDeriving #-}
{-# LANGUAGE TypeOperators #-}
module System.Command.AVRDUDE
    ( MemType(..)
    , Dir(..)
    , Op(..)
    , Format(..)
    , ActionsM
    , Actions
    , action
    , r, v, w, imm
    , encodeActions
    , actionFiles
    , avrdude
    ) where

import Control.Applicative
import Control.Monad.Writer
import Data.GADT.Compare
import Data.List
import Data.Word
import System.Exit
import System.Process
import Text.Printf

data MemType
    = Calibration
    | EEPROM
    | EFuse
    | Flash
    | Fuse
    | HFuse
    | LFuse
    | Lock
    | Signature
    | FuseN !Integer
    | Application
    | AppTable
    | Boot
    | ProdSig
    | UserSig
    | OtherMemType !String

encodeMemType :: MemType -> String
encodeMemType Calibration           = "calibration"
encodeMemType EEPROM                = "eeprom"
encodeMemType EFuse                 = "efuse"
encodeMemType Flash                 = "flash"
encodeMemType Fuse                  = "fuse"
encodeMemType HFuse                 = "hfuse"
encodeMemType LFuse                 = "lfuse"
encodeMemType Lock                  = "lock"
encodeMemType Signature             = "signature"
encodeMemType (FuseN n)             = "fuse" ++ show n
encodeMemType Application           = "application"
encodeMemType AppTable              = "apptable"
encodeMemType Boot                  = "boot"
encodeMemType ProdSig               = "prodsig"
encodeMemType UserSig               = "usersig"
encodeMemType (OtherMemType other)  = other

data Dir = In | Out

data Op dir where
    R :: Op Out
    V :: Op Out
    W :: Op In

encodeOp :: Op dir -> Char
encodeOp R = 'r'
encodeOp V = 'v'
encodeOp W = 'w'

data Format dir t where
    IHex        :: Format dir FilePath
    SRec        :: Format dir FilePath
    Raw         :: Format dir FilePath
    Immediate   :: Format In  [Word8]
    Auto        :: Format dir FilePath
    Dec         :: Format Out FilePath
    Hex         :: Format Out FilePath
    Oct         :: Format Out FilePath
    Bin         :: Format Out FilePath

encodeFormat :: Format dir t -> (Char, Maybe (t := FilePath), t -> String)
encodeFormat IHex       = ('i', Just Refl, id)
encodeFormat SRec       = ('s', Just Refl, id)
encodeFormat Raw        = ('r', Just Refl, id)
encodeFormat Immediate  = ('m', Nothing,   encodeImmediate)
encodeFormat Auto       = ('a', Just Refl, id)
encodeFormat Dec        = ('d', Just Refl, id)
encodeFormat Hex        = ('h', Just Refl, id)
encodeFormat Oct        = ('o', Just Refl, id)
encodeFormat Bin        = ('b', Just Refl, id)

encodeImmediate :: [Word8] -> String
encodeImmediate = intercalate "," . map encodeHex

-- TODO: see if there's an escape mechanism for strings with colons
    
encodeHex :: Word8 -> String
encodeHex = printf "0x%02x"

data Action where
    Action :: MemType -> Op dir -> t -> Format dir t -> Action

encodeAction :: Action -> String
encodeAction (Action memType op name format) = 
    intercalate ":"
        [ encodeMemType memType
        , [encodeOp      op]
        , encodeName    name
        , [encodedFormat]
        ]
    where
        (encodedFormat, _, encodeName) = encodeFormat format

actionFile :: Action -> Maybe FilePath
actionFile (Action _ _ name format) = case encodeFormat format of
    (_, Just Refl, _) -> Just name
    (_, Nothing,   _) -> Nothing

actionNeedsFile :: Action -> Bool
actionNeedsFile (Action _ W _ _)  = True
actionNeedsFile (Action _ V _ _)  = True
actionNeedsFile _                 = False

newtype ActionsM t = ActionsM (Writer [Action] t)
    deriving (Monad, Functor, Applicative)

type Actions = ActionsM ()

instance Monoid t => Monoid (ActionsM t) where
    mempty  = pure mempty
    mappend = liftA2 mappend

runActions :: Actions -> [Action]
runActions (ActionsM x) = execWriter x

action :: MemType -> Op dir -> t -> Format dir t -> Actions
action memType op name format = ActionsM (tell [Action memType op name format])

r :: MemType -> FilePath -> Actions
r memType path = action memType R path Auto

v :: MemType -> FilePath -> Actions
v memType path = action memType V path Auto

w :: MemType -> FilePath -> Actions
w memType path = action memType W path Auto

imm :: MemType -> [Word8] -> Actions
imm memType path = action memType W path Immediate

encodeActions :: Actions -> [String]
encodeActions = map (("-U" ++) . encodeAction) . runActions

actionFiles :: Actions -> ([FilePath], [FilePath])
actionFiles actions = (map snd needed, map snd produced)
    where
        ~(needed, produced) = partition fst
            [ (actionNeedsFile action, file)
            | action    <- runActions actions
            , Just file <- [actionFile action]
            ]

avrdude :: [String] -> Actions -> IO ExitCode
avrdude args actions = rawSystem "avrdude" (args ++ encodeActions actions)

