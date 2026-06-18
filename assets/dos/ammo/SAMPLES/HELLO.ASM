.MODEL TINY
.CODE
ORG 100h
start:
    mov ah,9
    mov dx,offset msg
    int 21h
    mov ax,4C00h
    int 21h
.DATA
msg db 'Hello RTX-DOS 7.0',13,10,'$'
END start
