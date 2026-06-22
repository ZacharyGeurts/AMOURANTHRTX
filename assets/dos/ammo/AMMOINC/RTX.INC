; RTX-AMMOS AMMOINC v4 - MASM intrinsics
RTX_PUTS MACRO msg
    mov ah,9
    mov dx,offset msg
    int 21h
ENDM
RTX_OUT MACRO ch
    mov ah,2
    mov dl,ch
    int 21h
ENDM
RTX_EXIT MACRO code:=0
    mov ax,4C00h + code
    int 21h
ENDM
