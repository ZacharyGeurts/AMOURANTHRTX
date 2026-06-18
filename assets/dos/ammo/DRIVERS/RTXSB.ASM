; RTXSB.SYS — L5 audio field driver (SB16/OPL/GUS rack)
.MODEL TINY
.CODE
ORG 0
    db 0EBh, 2Eh, 90h
    db 'RTXSB',0,0,0
    db 'RTXSB001'
    db 'RTXFL'
    db 5
    dw 2200h
    dw 0100h
    db 'LAYR'
    db 'STRT'
    db 'INTR'
    dw 10
    db 0,0,0,0,0,0
init:
    mov ah,9
    mov dx,offset banner
    int 21h
    mov ax,5200h
    int 2fh
    ret
banner:
    db 'RTXSB field layer L5 - SB16/OPL/GUS audio rack loaded',13,10,'$'
ioctl_thunk:
    mov ax,5201h
    mov bl,5
    mov cx,2200h
    int 2fh
    ret
END init