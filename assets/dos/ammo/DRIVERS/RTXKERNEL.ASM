; RTXKERNEL.SYS — Supercore INT 2Fh field mux (L0-L9)
.MODEL TINY
.CODE
ORG 0
    db 0EBh, 2Eh, 90h
    db 'RTXKNL',0,0
    db 'SUPRCR01'
    db 'RTXFL'
    db 0
    dw 2F50h
    dw 0100h
    db 'LAYR'
    db 'STRT'
    db 'INTR'
    dw 16
    db 0,0,0,0,0,0
init:
    mov ah,9
    mov dx,offset banner
    int 21h
    mov ax,5200h
    int 2fh
    mov ax,5202h
    int 2fh
    ret
banner:
    db 'RTXKERNEL Supercore v1.0 - INT 2Fh field mux L0-L9',13,10,'$'
END init