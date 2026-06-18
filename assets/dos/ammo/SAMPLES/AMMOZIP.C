/* AMMOZIP sample — RTX-DOS PKZIP extractor (host DevKit dispatches real logic). */
#include "rtx.h"

int main(void) {
    rtx_puts("AMMOZIP RTX-DOS 7.0 Field Die\r\n");
    rtx_puts("Usage: AMMOZIP archive.zip [dest\\]\r\n");
    rtx_puts("Example: AMMOZIP C:\\GAMES\\WOLF3D\\WOLF3D.ZIP\r\n");
    return 0;
}