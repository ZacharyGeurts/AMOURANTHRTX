/* DOOMBOOT - AMMOCC launcher with field absolutes */
#include "FIELD.H"
#include "RTX.H"
void main() {
    int fwd;
    int rev;
    fwd = TESLA_R_FWD_MILLI;
    rev = TESLA_R_REV_MILLI;
    rtx_puts("DOOMBOOT AMMOCC Field Die\r\n");
    if (fwd < rev) rtx_puts("Tesla valve OK\r\n");
    rtx_puts("Run GAMES/DOOM/DOOM.EXE\r\n");
    return 0;
}
