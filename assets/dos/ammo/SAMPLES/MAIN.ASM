.MODEL TINY
.CODE
ORG 100h
EXTRN RTXPUT
start:
    call RTXPUT
    mov ah,9
    mov dx,offset msg
    int 21h
    mov ax,4C00h
    int 21h
.DATA
msg db ' MULTI v4',13,10,'$'
END start
