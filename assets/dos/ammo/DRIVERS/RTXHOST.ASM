; RTXHOST.SYS — L7 host bridge (E:) field driver
.MODEL TINY
.CODE
ORG 0
    db 0EBh, 2Eh, 90h
    db 'RTXHOST',0
    db 'RTXHOST1'
    db 'RTXFL'
    db 8
    dw 2F40h
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
    db 'RTXHOST field layer L7 - host bridge E drive loaded',13,10,'$'
ioctl_thunk:
    mov ax,5201h
    mov bl,8
    mov cx,2F40h
    int 2fh
    ret
END init