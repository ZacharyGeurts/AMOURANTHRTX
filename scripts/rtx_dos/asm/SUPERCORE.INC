; RTX-AMMOS Supercore field-layer constants (AMMOINC)
; Matches mk_supercore.py / FieldLayer::LayerId / RTXFL .SYS headers

RTXFL_MAGIC     EQU  'RTXFL'
LAYR_MAGIC      EQU  'LAYR'
RTX_MUX_AH      EQU  052h          ; INT 2Fh multiplex id ('R')
RTX_MUX_IDENT   EQU  000h
RTX_MUX_IOCTL   EQU  001h
RTX_MUX_ACTIV   EQU  002h

; Field layer IDs (L0-L9)
LAYER_RAM       EQU  0
LAYER_VGA       EQU  1
LAYER_FAT       EQU  2
LAYER_DRIVES    EQU  3
LAYER_VIEWPORT  EQU  4
LAYER_AUDIO     EQU  5
LAYER_MSCDEX    EQU  6
LAYER_INPUT     EQU  7
LAYER_IO        EQU  8
LAYER_BIOS      EQU  9

; IOCTL bases (FieldRtxDrivers)
IOCTL_RTXVGA    EQU  02F00h
IOCTL_RTXDRV    EQU  02F05h
IOCTL_RTXAPI    EQU  02F20h
IOCTL_HIMEM     EQU  02F30h
IOCTL_AMMOFAT   EQU  02F10h
IOCTL_RTXHOST   EQU  02F40h
IOCTL_RTXCD     EQU  01500h
IOCTL_RTXSB     EQU  02200h

; RTX multiplex helpers — host FieldBios dispatches to FieldRtxDrivers::ioctl
RTX_IDENT MACRO
    mov ax,(RTX_MUX_AH SHL 8) OR RTX_MUX_IDENT
    int 2Fh
ENDM

RTX_IOCTL MACRO ioctl_code, layer_id
    mov ax,(RTX_MUX_AH SHL 8) OR RTX_MUX_IOCTL
    mov bl,layer_id
    mov cx,ioctl_code
    int 2Fh
ENDM

RTX_ACTIV MACRO
    mov ax,(RTX_MUX_AH SHL 8) OR RTX_MUX_ACTIV
    int 2Fh
ENDM