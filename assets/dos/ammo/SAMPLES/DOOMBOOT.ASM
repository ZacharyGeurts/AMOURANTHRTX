INCLUDE FIELD.INC
.MODEL TINY
.CODE
ORG 100h
start:
    mov ah,9
    mov dx,offset msg
    int 21h
    mov ax,TESLA_R_FWD_MILLI
    cmp ax,180
    je ok
    mov ah,9
    mov dx,offset bad
    int 21h
    mov ax,4C01h
    int 21h
ok:
    mov ah,9
    mov dx,offset go
    int 21h
    mov ax,4C00h
    int 21h
.DATA
msg db 'DOOMBOOT RTX-DOS 7.0 Field Die',13,10,'$'
bad db 'Tesla valve mismatch',13,10,'$'
go  db 'Launch GAMES/DOOM/DOOM.EXE',13,10,'$'
END start
