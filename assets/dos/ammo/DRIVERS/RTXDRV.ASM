; RTXDRV.SYS — L0 floppy / RTXBOOT field driver
.MODEL TINY
.CODE
ORG 0
    db 0EBh, 2Eh, 90h
    db 'RTXDRV',0,0
    db 'RTXDRV01'
    db 'RTXFL'
    db 0
    dw 2F05h
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
    db 'RTXDRV field layer L0 - floppy A drive RTXBOOT loaded',13,10,'$'
ioctl_thunk:
    mov ax,5201h
    mov bl,0
    mov cx,2F05h
    int 2fh
    ret
END init