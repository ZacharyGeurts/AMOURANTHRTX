; HIMEM.SYS — L0 XMS extended memory manager
.MODEL TINY
.CODE
ORG 0
    db 0EBh, 2Eh, 90h
    db 'HIMEM',0,0,0
    db 'HIMEM630'
    db 'RTXFL'
    db 0
    dw 2F30h
    dw 0630h
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
    db 'HIMEM.SYS v6.30 XMS L0 loaded',13,10,'$'
ioctl_thunk:
    mov ax,5201h
    mov bl,0
    mov cx,2F30h
    int 2fh
    ret
END init