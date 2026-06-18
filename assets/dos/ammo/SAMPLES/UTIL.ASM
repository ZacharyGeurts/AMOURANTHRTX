.MODEL TINY
.CODE
PUBLIC RTXPUT
RTXPUT:
    mov ah,2
    mov dl,'*'
    int 21h
    ret
END
