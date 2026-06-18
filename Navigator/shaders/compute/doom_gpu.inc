// GPU Doom on Field Die panel — FreeDoom assets in guest RAM, logic + raycast in shader.
const uint CTRL_DOS_PANEL_FS = 256u;
const uint CTRL_GPU_DOOM = 128u;
const uint DOOM_PAL_BYTE = 0x90000u;
const uint DOOM_TITLE_BYTE = 0x90300u;
const uint DOOM_FB_BYTE = 0xA0000u;
const uint DOOM_STATE_BYTE = 0xAFA00u;
const uint DOOM_W = 320u;
const uint DOOM_H = 200u;
const float DOOM_VIEW_W = 640.0;
const float DOOM_VIEW_H = 480.0;
const float DOOM_PIXEL_SCALE = 2.0;

struct DoomViewport {
    bool inside;
    bool letterbox;
    vec2 gameUv;
    vec2 p0;
    vec2 p1;
};

DoomViewport doomViewportMap(vec2 pix, vec2 img) {
    DoomViewport v;
    v.inside = false;
    v.letterbox = false;
    v.gameUv = vec2(0.0);
    v.p0 = vec2(0.0);
    v.p1 = vec2(0.0);
    bool panelFs = (field.control & CTRL_DOS_PANEL_FS) != 0u;
    vec2 p0;
    vec2 p1;
    if (panelFs) {
        const float aspect = 4.0 / 3.0;
        const float imgAspect = img.x / max(img.y, 1.0);
        if (imgAspect > aspect) {
            const float w = img.y * aspect;
            p0 = vec2((img.x - w) * 0.5, 0.0);
            p1 = vec2(p0.x + w, img.y);
        } else {
            const float h = img.x / aspect;
            p0 = vec2(0.0, (img.y - h) * 0.5);
            p1 = vec2(img.x, p0.y + h);
        }
    } else {
        vec2 vpHalf = vec2(DOOM_VIEW_W, DOOM_VIEW_H) * 0.5;
        vec2 c = img * 0.5;
        p0 = c - vpHalf;
        p1 = c + vpHalf;
    }
    v.p0 = p0;
    v.p1 = p1;
    if (pix.x < p0.x || pix.y < p0.y || pix.x >= p1.x || pix.y >= p1.y)
        return v;
    v.inside = true;
    vec2 vpSize = p1 - p0;
    vec2 local = (pix - p0) / vpSize;
    uint vmode = ramByte(0x449u);
    if (vmode == 0x13u) {
        float gameW = float(DOOM_W) * DOOM_PIXEL_SCALE;
        float gameH = float(DOOM_H) * DOOM_PIXEL_SCALE;
        float scale = min(vpSize.x / gameW, vpSize.y / gameH);
        float drawW = gameW * scale;
        float drawH = gameH * scale;
        float padX = (vpSize.x - drawW) * 0.5;
        float padY = (vpSize.y - drawH) * 0.5;
        vec2 lp = local * vpSize;
        if (lp.x < padX || lp.x >= vpSize.x - padX || lp.y < padY || lp.y >= vpSize.y - padY) {
            v.letterbox = true;
            return v;
        }
        v.gameUv = vec2((lp.x - padX) / max(drawW, 1.0), (lp.y - padY) / max(drawH, 1.0));
    } else {
        uint cols = max(field.data_bus[44], 40u);
        uint rows = max(field.data_bus[45], 25u);
        float textAspect = (float(cols) * 8.0) / (float(rows) * 16.0);
        float padX = max(0.0, (1.0 - textAspect / (4.0 / 3.0)) * 0.5);
        if (local.x < padX || local.x >= 1.0 - padX) {
            v.letterbox = true;
            return v;
        }
        v.gameUv = vec2((local.x - padX) / max(1.0 - 2.0 * padX, 0.001), local.y);
    }
    return v;
}

bool doomPixelInViewport(vec2 pix, vec2 img) {
    return doomViewportMap(pix, img).inside;
}

bool dosPanelBorderHit(vec2 pix, DoomViewport v) {
    vec2 vpSize = v.p1 - v.p0;
    vec2 lp = pix - v.p0;
    return lp.x < 1.5 || lp.y < 1.5 || lp.x >= vpSize.x - 1.5 || lp.y >= vpSize.y - 1.5;
}

const uint DOOM_MODE_TITLE = 0u;
const uint DOOM_MODE_MENU = 1u;
const uint DOOM_MODE_PLAY = 2u;
const uint KEY_UP = 328u;
const uint KEY_DOWN = 336u;
const uint KEY_LEFT = 331u;
const uint KEY_RIGHT = 333u;
const uint KEY_FIRE = 17u; // Ctrl

uint doomState(uint slot) {
    uint idx = (DOOM_STATE_BYTE + slot * 4u) / 4u;
    if (idx < uint(die.RAM.length())) return die.RAM[idx];
    return 0u;
}

void setDoomState(uint slot, uint v) {
    uint idx = (DOOM_STATE_BYTE + slot * 4u) / 4u;
    if (idx < uint(die.RAM.length())) die.RAM[idx] = v;
}

vec3 doomPal(uint idx) {
    uint b = DOOM_PAL_BYTE + (idx & 0xFFu) * 3u;
    uint r = ramByte(b);
    uint g = ramByte(b + 1u);
    uint b0 = ramByte(b + 2u);
    return vec3(float(r), float(g), float(b0)) / 255.0;
}

uint doomTitlePix(uint x, uint y) {
    if (x >= DOOM_W || y >= DOOM_H) return 0u;
    return ramByte(DOOM_TITLE_BYTE + y * DOOM_W + x);
}

uint doomMapCell(int mx, int my) {
    if (mx < 0 || my < 0 || mx >= 16 || my >= 16) return 1u;
    const uint MAP[256] = uint[256](
        1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,
        1u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,1u,
        1u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,1u,
        1u,0u,0u,1u,1u,0u,0u,1u,1u,0u,0u,1u,1u,0u,0u,1u,
        1u,0u,0u,1u,0u,0u,0u,0u,0u,0u,0u,0u,1u,0u,0u,1u,
        1u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,1u,
        1u,0u,0u,0u,0u,1u,1u,1u,1u,1u,0u,0u,0u,0u,0u,1u,
        1u,0u,0u,0u,0u,1u,0u,0u,0u,1u,0u,0u,0u,0u,0u,1u,
        1u,0u,0u,0u,0u,1u,0u,0u,0u,1u,0u,0u,0u,0u,0u,1u,
        1u,0u,0u,0u,0u,1u,1u,0u,1u,1u,0u,0u,0u,0u,0u,1u,
        1u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,1u,
        1u,0u,0u,1u,0u,0u,0u,0u,0u,0u,0u,1u,0u,0u,0u,1u,
        1u,0u,0u,1u,1u,0u,0u,0u,0u,0u,1u,1u,0u,0u,0u,1u,
        1u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,1u,
        1u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,1u,
        1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u);
    return MAP[uint(my) * 16u + uint(mx)];
}

float doomWallHeight(int mx, int my) {
    uint c = doomMapCell(mx, my);
    if (c == 0u) return 0.0;
    return ((mx + my) & 1) != 0 ? 1.0 : 0.65;
}

vec3 doomRayColor(float px, float py, float pa) {
    float fdx = cos(pa);
    float fdy = sin(pa);
    float rdx = -sin(pa);
    float rdy = cos(pa);
    float depth = 0.01;
    float wallH = 0.0;
    uint wallCol = 96u;
    for (uint step = 0u; step < 96u; ++step) {
        float sx = px + fdx * depth;
        float sy = py + fdy * depth;
        int mx = int(sx);
        int my = int(sy);
        if (doomMapCell(mx, my) != 0u) {
            wallH = doomWallHeight(mx, my);
            wallCol = uint(16 + ((mx + my) & 7) * 3);
            break;
        }
        depth += 0.04;
    }
    if (wallH <= 0.0) return doomPal(192u);
    float shade = clamp(1.4 - depth * 0.22, 0.25, 1.0);
    vec3 col = doomPal(wallCol) * shade;
    float ceilGrad = clamp(py / 16.0, 0.0, 1.0);
    float floorGrad = 1.0 - ceilGrad;
    float sx = px + rdx * 0.2;
    float sy = py + rdy * 0.2;
    if (doomMapCell(int(sx), int(sy)) != 0u)
        return col;
    return mix(doomPal(192u) * 0.35, doomPal(32u) * 0.45, floorGrad);
}

vec3 doomMenuColor(uint x, uint y, uint sel, uint item, ThemePal th) {
    if (item == sel) {
        if ((((x >> 3) ^ (y >> 3)) & 1u) == 0u) return doomPal(80u);
    }
    return doomPal(4u);
}

void doomDrawMenuItem(uint item, uint sel, uint y0) {
    uint labels[5][12] = uint[5][12](
        uint[12](78u,69u,87u,32u,71u,65u,77u,69u,0u,0u,0u,0u),
        uint[12](79u,80u,84u,73u,79u,78u,83u,0u,0u,0u,0u,0u),
        uint[12](76u,79u,65u,68u,32u,71u,65u,77u,69u,0u,0u,0u),
        uint[12](83u,65u,86u,69u,32u,71u,65u,77u,69u,0u,0u,0u),
        uint[12](81u,85u,73u,84u,32u,71u,65u,77u,69u,0u,0u,0u));
    for (uint c = 0u; c < 12u; ++c) {
        uint ch = labels[item][c];
        if (ch == 0u) break;
        for (uint px = 0u; px < 8u; ++px)
            for (uint py = 0u; py < 16u; ++py)
                if (fontOn(ch, px, py)) {
                    uint x = 90u + c * 10u + px;
                    uint y = y0 + py;
                    if (x < DOOM_W && y < DOOM_H)
                        ramWriteByte(DOOM_FB_BYTE + y * DOOM_W + x, item == sel ? 80u : 4u);
                }
    }
}

void doomComposeMenu() {
    for (uint y = 0u; y < DOOM_H; ++y)
        for (uint x = 0u; x < DOOM_W; ++x) {
            uint ti = doomTitlePix(x, y);
            ramWriteByte(DOOM_FB_BYTE + y * DOOM_W + x, ti);
        }
    uint sel = doomState(1);
    doomDrawMenuItem(0u, sel, 120u);
    doomDrawMenuItem(1u, sel, 140u);
    doomDrawMenuItem(2u, sel, 160u);
    doomDrawMenuItem(3u, sel, 180u);
    doomDrawMenuItem(4u, sel, 190u);
}

vec3 doomGpuSampleGame(vec2 gameUv) {
    gameUv = clamp(gameUv, vec2(0.0), vec2(0.9999));
    float fx = gameUv.x * float(DOOM_W);
    float fy = gameUv.y * float(DOOM_H);
    uint mode = doomState(0);
    if (mode == DOOM_MODE_PLAY) {
        float px = float(int(doomState(2))) / 65536.0;
        float py = float(int(doomState(3))) / 65536.0;
        float pa = float(int(doomState(4))) / 65536.0 * 6.2831853;
        float rx = (fx / float(DOOM_W) - 0.5) * 1.2;
        float ry = (fy / float(DOOM_H) - 0.5);
        return doomRayColor(px, py, pa + rx * 0.55);
    }
    uint x = uint(floor(fx));
    uint y = uint(floor(fy));
    uint idx = ramByte(DOOM_FB_BYTE + y * DOOM_W + x);
    if (idx == 0u && mode == DOOM_MODE_TITLE) idx = doomTitlePix(x, y);
    return doomPal(idx);
}

vec3 doomViewportColor(vec2 pix, vec2 img, ThemePal th) {
    DoomViewport v = doomViewportMap(pix, img);
    if (!v.inside) return vec3(-1.0);
    if (dosPanelBorderHit(pix, v)) return vec3(0.04, 0.04, 0.05);
    if (v.letterbox) return vec3(0.048, 0.052, 0.058);
    return clamp(doomGpuSampleGame(v.gameUv), 0.0, 1.0);
}

void doomGpuStep() {
    uint mode = doomState(0);
    uint key = field.data_bus[32];
    uint tick = doomState(5) + 1u;
    setDoomState(5, tick);

    if (mode == DOOM_MODE_TITLE) {
        for (uint y = 0u; y < DOOM_H; ++y)
            for (uint x = 0u; x < DOOM_W; ++x)
                ramWriteByte(DOOM_FB_BYTE + y * DOOM_W + x, doomTitlePix(x, y));
        if (key != 0u || tick > 180u) {
            setDoomState(0, DOOM_MODE_MENU);
            setDoomState(1, 0u);
        }
        return;
    }

    if (mode == DOOM_MODE_MENU) {
        uint sel = doomState(1);
        if (key == KEY_UP && sel > 0u) setDoomState(1, sel - 1u);
        else if (key == KEY_DOWN && sel < 4u) setDoomState(1, sel + 1u);
        else if (key == 13u || key == 32u) {
            if (sel == 0u) {
                setDoomState(0, DOOM_MODE_PLAY);
                setDoomState(2, 0x00028000u);
                setDoomState(3, 0x00028000u);
                setDoomState(4, 0u);
            } else if (sel == 4u) setDoomState(0, DOOM_MODE_TITLE);
        }
        doomComposeMenu();
        return;
    }

    if (mode == DOOM_MODE_PLAY) {
        float px = float(int(doomState(2))) / 65536.0;
        float py = float(int(doomState(3))) / 65536.0;
        float pa = float(int(doomState(4))) / 65536.0 * 6.2831853;
        float spd = 0.08;
        float rot = 0.12;
        if (key == KEY_LEFT) pa -= rot;
        if (key == KEY_RIGHT) pa += rot;
        float dx = cos(pa) * spd;
        float dy = sin(pa) * spd;
        if (key == KEY_UP) { px += dx; py += dy; }
        if (key == KEY_DOWN) { px -= dx; py -= dy; }
        if (key == 27u) { setDoomState(0, DOOM_MODE_MENU); return; }
        if (doomMapCell(int(px), int(py)) == 0u) {
            setDoomState(2, uint(int(px * 65536.0)));
            setDoomState(3, uint(int(py * 65536.0)));
        }
        while (pa < 0.0) pa += 6.2831853;
        while (pa >= 6.2831853) pa -= 6.2831853;
        setDoomState(4, uint(pa / 6.2831853 * 65535.0));
    }
}