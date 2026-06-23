// Left-docked taskbar — Start (lifted) → compact icon tabs | clock right.

#ifndef AOS_TASKBAR_LAYOUT_INC
#define AOS_TASKBAR_LAYOUT_INC

struct AosTbLayout {
    float vpW;
    float vpH;
    float scale;
    float taskH;
    float pad;
    float lift;
    float startX;
    float startW;
    float startH;
    float quickX;
    float quickW;
    float quickGap;
    float tabX;
    float tabW;
    float tabGap;
    float tabY;
    float tabH;
    float clockW;
    float clockX0;
    float clockX1;
    float barY0;
    float barY1;
};

AosTbLayout aosTbLayout(vec2 vp) {
    AosTbLayout L;
    L.vpW = vp.x;
    L.vpH = vp.y;
    L.scale = aosUiScale(vp);
    L.taskH = 56.0 * L.scale;
    L.pad = 6.0 * L.scale;
    L.lift = 22.0 * L.scale;
    L.barY0 = vp.y - L.taskH;
    L.barY1 = vp.y;
    L.startX = L.pad;
    L.startW = AOS_START_BTN_W * L.scale;
    L.startH = L.taskH - L.pad + L.lift;
    L.quickGap = 4.0 * L.scale;
    L.quickW = AOS_QUICK_BTN_W * L.scale;
    L.quickX = L.startX + L.startW + L.quickGap;
    L.tabX = L.quickX + float(AOS_QUICK_LAUNCH_N) * (L.quickW + L.quickGap) + 6.0 * L.scale;
    L.tabW = 40.0 * L.scale;
    L.tabGap = 4.0 * L.scale;
    L.tabY = L.barY0 + 5.0 * L.scale;
    L.tabH = L.taskH - 10.0 * L.scale;
    L.clockW = AOS_CLOCK_W * L.scale;
    L.clockX1 = vp.x - L.pad;
    L.clockX0 = L.clockX1 - L.clockW;
    return L;
}

bool aosTbOnStrip(vec2 pix, AosTbLayout L) {
    return pix.y >= L.barY0 && pix.y < L.barY1 && pix.x >= 0.0 && pix.x < L.vpW;
}

void aosTbStartBounds(AosTbLayout L, out vec2 b0, out vec2 b1) {
    b0 = vec2(L.startX, L.barY0 + L.pad - L.lift);
    b1 = vec2(L.startX + L.startW, L.barY1 - L.pad);
}

void aosTbQuickBounds(AosTbLayout L, int idx, out vec2 b0, out vec2 b1) {
    float x0 = L.quickX + float(idx) * (L.quickW + L.quickGap);
    b0 = vec2(x0, L.barY0 + L.pad);
    b1 = vec2(x0 + L.quickW, L.barY1 - L.pad);
}

void aosTbTabBounds(AosTbLayout L, int slot, out vec2 b0, out vec2 b1) {
    float x0 = L.tabX + float(slot) * (L.tabW + L.tabGap);
    b0 = vec2(x0, L.tabY);
    b1 = vec2(x0 + L.tabW, L.tabY + L.tabH);
}

float aosTbTaskH(vec2 vp) {
    return aosTbLayout(vp).taskH;
}

#endif