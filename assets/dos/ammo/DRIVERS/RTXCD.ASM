; RTXCD.SYS — L6 MSCDEX CD-ROM field driver
.MODEL TINY
.CODE
ORG 0
    db 0EBh, 2Eh, 90h
    db 'RTXCD',0,0,0
    db 'RTXCD001'
    db 'RTXFL'
    db 6
    dw 1500h
    dw 0201h
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
    db 'RTXCD/MSCDEX 2.1 field layer L6 - driver RTXCD001 loaded',13,10,'$'
ioctl_thunk:
    mov ax,5201h
    mov bl,6
    mov cx,1500h
    int 2fh
    ret
END init