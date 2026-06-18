; AMMOFAT.SYS — L2 GPU-native FAT16 field driver
.MODEL TINY
.CODE
ORG 0
    db 0EBh, 2Eh, 90h
    db 'AMMOFAT',0
    db 'AMMOFAT1'
    db 'RTXFL'
    db 2
    dw 2F10h
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
    db 'AMMOFAT v1.0 field layer L2 - GPU FAT16 plus loaded',13,10,'$'
ioctl_thunk:
    mov ax,5201h
    mov bl,2
    mov cx,2F10h
    int 2fh
    ret
END init