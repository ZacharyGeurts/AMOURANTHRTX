; RTX-AMMOS Supercore — field-layer INT 2Fh multiplex (reference listing)
; Assembled by: python3 AmmoDOS/v6.3/build/mk_supercore.py
;
; Production path: GPU Field Die C++ layers own FAT/CD/drives; guest .SYS stubs
; carry RTXFL headers so CONFIG.SYS DEVICE= loads identify field IOCTL bases.
; libx86emu QA: FieldBios::handleInt traps INT 2Fh AH=52h → FieldRtxDrivers::ioctl.

        .MODEL TINY
        .CODE

INCLUDE SUPERCORE.INC

; --- RTXKERNEL.SYS init (strategy entry) ---
;   RTX_PUTS banner
;   RTX_IDENT  → BX=0700h CX=0100h DX=10 layers
;   RTX_ACTIV  → sync field layers on host
;   ret

; --- Per-layer .SYS IOCTL thunk (offset INTR in RTXFL header) ---
ioctl_thunk PROC NEAR
        RTX_IOCTL IOCTL_AMMOFAT, LAYER_FAT
        ret
ioctl_thunk ENDP

        END