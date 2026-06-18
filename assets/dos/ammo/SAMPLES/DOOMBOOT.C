/* DOOMBOOT - AMMOCC launcher with field absolutes */
#include "FIELD.H"
#include "RTX.H"
void main() {
    int fwd;
    int rev;
    int bus;
    fwd = TESLA_R_FWD_MILLI;
    rev = TESLA_R_REV_MILLI;
    bus = FIELD_BUS_TESLA;
    rtx_puts("DOOMBOOT AMMOCC Field Die\r\n");
    if (fwd < rev) rtx_puts("Tesla valve OK\r\n");
    if (bus == 31) rtx_puts("Bus slot 31\r\n");
    rtx_puts("Run C:\\GAMES\\DOOM\\DOOM.EXE\r\n");
    return 0;
}
