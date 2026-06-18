; RTXVGA.SYS — L1 VGA/VESA display field driver
.MODEL TINY
.CODE
ORG 0
    db 0EBh, 2Eh, 90h
    db 'RTXVGA',0,0
    db 'RTXVGA1 '
    db 'RTXFL'
    db 1
    dw 2F00h
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
    db 'RTXVGA field layer L1 - VGA/VESA text and graphics loaded',13,10,'$'
ioctl_thunk:
    mov ax,5201h
    mov bl,1
    mov cx,2F00h
    int 2fh
    ret
END init