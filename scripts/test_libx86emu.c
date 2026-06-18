#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86emu.h>
#include <mem.h>

#define DISK_BASE  0x20000u
#define DISK_BYTES 737280u

static unsigned char hd_buf[8u * 1024u * 1024u];
static unsigned hd_sz;
#define BDA_BASE   0x400u
#define VGA_BASE   0xB8000u
#define DPT_ADDR   0xEFC0u

static int is_hd(u8 dl) { return dl == 0x80 && hd_sz > 0; }

static unsigned floppy_off(u16 cyl, u16 head, u16 sector)
{
    if (sector < 1 || sector > 9) return 0xFFFFFFFFu;
    return DISK_BASE + (cyl * 18u + head * 9u + (sector - 1u)) * 512u;
}

static unsigned hd_lba_off(unsigned lba)
{
    if (!hd_sz || lba * 512u >= hd_sz) return 0xFFFFFFFFu;
    return lba * 512u;
}

static unsigned hd_off(u16 cyl, u16 head, u16 sector)
{
    if (!hd_sz || sector < 1) return 0xFFFFFFFFu;
    unsigned lba = cyl * 16u * 63u + head * 63u + (sector - 1u);
    return hd_lba_off(lba);
}

static unsigned field_memio(x86emu_t *emu, u32 addr, u32 *val, unsigned type)
{
    const unsigned op = (type >> 8) & 0xF;
    if (op == X86EMU_MEMIO_O) return 0;
    if (op == X86EMU_MEMIO_I) {
        const unsigned w = type & 3;
        if (w == X86EMU_MEMIO_8) *val = 0xFF;
        else *val = 0xFFFF;
        return 0;
    }
    return vm_memio(emu, addr, val, type);
}

static void seed_stack(x86emu_t *emu)
{
    x86emu_write_word(emu, 0x12CE0 + 0x38CF, 0x0100);
    x86emu_write_word(emu, 0x12CE0 + 0x38CF + 2, 0x0268);
}

static u16 bda_cursor(x86emu_t *emu)
{
    return (u16)(x86emu_read_byte(emu, BDA_BASE + 0x51) * 80u
               + x86emu_read_byte(emu, BDA_BASE + 0x50));
}

static void vga_put(x86emu_t *emu, u16 pos, u8 ch, u8 attr)
{
    u32 off = VGA_BASE + pos * 2u;
    x86emu_write_byte(emu, off, ch);
    x86emu_write_byte(emu, off + 1, attr);
}

static void init_bios(x86emu_t *emu)
{
    x86emu_write_word(emu, BDA_BASE + 0x10, 0x0027);
    x86emu_write_word(emu, BDA_BASE + 0x17, 0x0280);
    x86emu_write_byte(emu, BDA_BASE + 0x49, 3);
    x86emu_write_byte(emu, BDA_BASE + 0x4A, 80);
    x86emu_write_byte(emu, BDA_BASE + 0x50, 0);
    x86emu_write_byte(emu, BDA_BASE + 0x51, 0);
    x86emu_write_byte(emu, BDA_BASE + 0x75, 1);
    const u8 dpt[] = { 0xDF,0x02,0x25,0x02,0x09,0x2A,0xFF,0x50,0xF6,0x0F,0x08 };
    for (int i = 0; i < 11; ++i) x86emu_write_byte(emu, DPT_ADDR + (u32)i, dpt[i]);
    x86emu_write_word(emu, BDA_BASE + 0x78, DPT_ADDR);
    x86emu_write_word(emu, BDA_BASE + 0x7A, 0);
    x86emu_write_word(emu, 0x1E * 4, DPT_ADDR);
    x86emu_write_word(emu, 0x1E * 4 + 2, 0);
    seed_stack(emu);
}

static int ivt_empty(x86emu_t *emu, u8 num)
{
    u32 iv = num * 4u;
    return x86emu_read_word(emu, iv) == 0 && x86emu_read_word(emu, iv + 2) == 0;
}

static int int21_early(x86emu_t *emu, u8 ah)
{
    if (ah == 0x25) {
        u8 vint = (u8)emu->x86.R_EAX;
        u16 off = (u16)emu->x86.R_EDX;
        u16 seg = (u16)emu->x86.R_DS;
        x86emu_write_word(emu, vint * 4u, off);
        x86emu_write_word(emu, vint * 4u + 2, seg);
        return 1;
    }
    if (ah == 0x35) {
        u8 vint = (u8)emu->x86.R_EAX;
        u16 off = x86emu_read_word(emu, vint * 4u);
        u16 seg = x86emu_read_word(emu, vint * 4u + 2);
        x86emu_set_seg_register(emu, emu->x86.R_ES_SEL, seg);
        emu->x86.R_EBX = (emu->x86.R_EBX & 0xFFFF0000u) | off;
        return 1;
    }
    if (ah == 0x30) {
        emu->x86.R_EAX = (emu->x86.R_EAX & 0xFFFF0000u) | 0x0007;
        emu->x86.R_EBX = (emu->x86.R_EBX & 0xFFFF00FFu) | (0x10u << 8);
        return 1;
    }
    if (ah == 0x1A) { emu->x86.R_EBX = (emu->x86.R_EBX & 0xFFFF0000u) | 0x0080; return 1; }
    if (ah == 0x19) { emu->x86.R_EAX = (emu->x86.R_EAX & 0xFFFFFF00u) | 2; return 1; }
    if (ah == 0x09) {
        u16 off = (u16)emu->x86.R_EDX, seg = (u16)emu->x86.R_DS;
        u32 addr = ((u32)seg << 4) + off;
        for (;;) {
            u8 ch = x86emu_read_byte(emu, addr++);
            if (ch == '$') break;
            if (ch != '\r' && ch != '\n') {
                u16 pos = bda_cursor(emu);
                vga_put(emu, pos, ch, 0x07);
                pos++;
                if ((pos % 80) == 0) pos += 80;
                x86emu_write_byte(emu, BDA_BASE + 0x50, pos % 80);
                x86emu_write_byte(emu, BDA_BASE + 0x51, pos / 80);
            }
        }
        return 1;
    }
    emu->x86.R_EAX &= 0xFFFF0000u;
    return 1;
}

static int field_int(x86emu_t *emu, u8 num, unsigned type)
{
    (void)type;
    u8 ah = (u8)(emu->x86.R_EAX >> 8);

    if (num == 0x08 || num == 0x1C) return ivt_empty(emu, num) ? 1 : 0;

    if (num == 0x10) {
        if (ah == 0x00 && (emu->x86.R_EAX & 0xFF) == 3) {
            for (int i = 0; i < 80 * 25; ++i) vga_put(emu, (u16)i, ' ', 0x07);
            x86emu_write_byte(emu, BDA_BASE + 0x50, 0);
            x86emu_write_byte(emu, BDA_BASE + 0x51, 0);
        }
        if (ah == 0x0E) {
            u16 pos = bda_cursor(emu);
            vga_put(emu, pos, (u8)emu->x86.R_EAX, 0x07);
            pos++;
            if ((pos % 80) == 0) pos += 80;
            x86emu_write_byte(emu, BDA_BASE + 0x50, pos % 80);
            x86emu_write_byte(emu, BDA_BASE + 0x51, pos / 80);
        }
        return 1;
    }
    if (num == 0x11) { emu->x86.R_EAX = (emu->x86.R_EAX & 0xFFFF0000u) | 0x0027; return 1; }
    if (num == 0x12) { emu->x86.R_EAX = (emu->x86.R_EAX & 0xFFFF0000u) | 0x0280; return 1; }
    if (num == 0x13) {
        if (ah == 0x00) { emu->x86.R_FLG &= ~F_CF; return 1; }
        if (ah == 0x41) {
            if ((emu->x86.R_EBX & 0xFFFF) == 0x55AA) {
                emu->x86.R_EBX = (emu->x86.R_EBX & 0xFFFF0000u) | 0xAA55;
                emu->x86.R_ECX = (emu->x86.R_ECX & 0xFFFF0000u) | 1;
                emu->x86.R_FLG &= ~F_CF;
            } else emu->x86.R_FLG |= F_CF;
            return 1;
        }
        if (ah == 0x42) {
            u8 dl = (u8)emu->x86.R_EDX;
            u32 dap = (emu->x86.R_DS << 4) + (emu->x86.R_ESI & 0xFFFF);
            u16 count = x86emu_read_word(emu, dap + 2);
            u16 bufOff = x86emu_read_word(emu, dap + 4);
            u16 bufSeg = x86emu_read_word(emu, dap + 6);
            u32 lba = x86emu_read_word(emu, dap + 8)
                    | ((u32)x86emu_read_word(emu, dap + 10) << 16);
            u32 dst = ((u32)bufSeg << 4) + bufOff;
            if (!is_hd(dl) || !hd_sz) { emu->x86.R_FLG |= F_CF; return 1; }
            for (u16 b = 0; b < count; ++b) {
                u32 src = hd_lba_off(lba + b);
                if (src == 0xFFFFFFFFu) { emu->x86.R_FLG |= F_CF; return 1; }
                for (int i = 0; i < 512; ++i)
                    x86emu_write_byte(emu, dst + b * 512u + (u32)i, hd_buf[src + (u32)i]);
            }
            emu->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah == 0x02) {
            u8 dl = (u8)emu->x86.R_EDX;
            u8 n = (u8)emu->x86.R_EAX;
            u16 head = (u16)((emu->x86.R_EDX >> 8) & 0xFF);
            u16 cyl = (u16)(((emu->x86.R_ECX >> 8) & 0xFF) | ((emu->x86.R_ECX & 0xC0u) << 2));
            u16 sector = emu->x86.R_ECX & 0x3Fu;
            if (!n || !sector || (!is_hd(dl) && sector > 9)) {
                emu->x86.R_EAX |= 1u << 8; emu->x86.R_FLG |= F_CF; return 1;
            }
            u32 dst = (emu->x86.R_ES << 4) + (emu->x86.R_EBX & 0xFFFF);
            for (u8 i = 0; i < n; ++i) {
                u32 src = is_hd(dl) ? hd_off(cyl, head, sector + i) : floppy_off(cyl, head, sector + i);
                if (src == 0xFFFFFFFFu) {
                    emu->x86.R_EAX |= 1u << 8; emu->x86.R_FLG |= F_CF; return 1;
                }
                for (int b = 0; b < 512; ++b) {
                    u8 byte = is_hd(dl) ? hd_buf[src + (u32)b] :
                        (u8)x86emu_read_byte(emu, src + (u32)b);
                    x86emu_write_byte(emu, dst + i * 512u + (u32)b, byte);
                }
            }
            emu->x86.R_EAX = (emu->x86.R_EAX & 0xFFFFFF00u) | n;
            emu->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah == 0x08) {
            u8 dl = (u8)emu->x86.R_EDX;
            if (is_hd(dl) && hd_sz) {
                const unsigned sectors = hd_sz / 512u;
                const u8 hdMaxCyl = (u8)(((sectors > 0u ? sectors - 1u : 0u) / (16u * 63u)) & 0xFFu);
                const u8 cl = (u8)(63u | (((hdMaxCyl >> 6) & 3u) << 6));
                const u8 ch = (u8)(hdMaxCyl & 0xFFu);
                emu->x86.R_ECX = (emu->x86.R_ECX & 0xFFFF0000u)
                               | ((u32)cl << 8) | ch;
                emu->x86.R_EDX = (emu->x86.R_EDX & 0xFFFF00FFu) | (15u << 8);
                emu->x86.R_EDX = (emu->x86.R_EDX & 0xFFFFFF00u) | 0x80;
            } else {
                emu->x86.R_ECX = (emu->x86.R_ECX & 0xFFFF0000u) | 0x094F;
                emu->x86.R_EDX = (emu->x86.R_EDX & 0xFFFF00FFu) | (1u << 8);
                emu->x86.R_EDX = (emu->x86.R_EDX & 0xFFFFFF00u) | 0x01;
            }
            emu->x86.R_FLG &= ~F_CF;
            return 1;
        }
        if (ah == 0x15) {
            emu->x86.R_EAX = (emu->x86.R_EAX & 0xFFFF00FFu) | (1u << 8);
            emu->x86.R_FLG &= ~F_CF; return 1;
        }
        if (ah == 0x16) { emu->x86.R_EAX &= 0xFFFF00FFu; emu->x86.R_FLG &= ~F_CF; return 1; }
        emu->x86.R_EAX |= 1u << 8; emu->x86.R_FLG |= F_CF; return 1;
    }
    if (num == 0x15) {
        if (ah == 0x88) { emu->x86.R_EAX &= 0xFFFF0000u; emu->x86.R_FLG &= ~F_CF; return 1; }
        emu->x86.R_EAX |= 1u << 8; emu->x86.R_FLG |= F_CF; return 1;
    }
    if (num == 0x16) {
        u16 key = 0;
        if (emu->x86.R_TSC > 12000000u) key = 0x3F00;
        if (ah == 0x00 || ah == 0x10) {
            emu->x86.R_EAX = (emu->x86.R_EAX & 0xFFFF0000u) | key;
            if (key) emu->x86.R_EAX |= 0x0100u;
            return 1;
        }
        if (ah == 0x01) {
            if (key) emu->x86.R_EAX = (emu->x86.R_EAX & 0xFFFF0000u) | key | 0x0100u;
            else emu->x86.R_EAX &= 0xFFFF00FFu;
            return 1;
        }
        return 1;
    }
    if (num == 0x1A) {
        u32 t = (u32)(emu->x86.R_TSC & 0xFFFFFFFFu);
        emu->x86.R_ECX = (emu->x86.R_ECX & 0xFFFF0000u) | ((t >> 16) & 0xFFFF);
        emu->x86.R_EDX = (emu->x86.R_EDX & 0xFFFF0000u) | (t & 0xFFFF);
        return 1;
    }
    if (num == 0x21) {
        if (!ivt_empty(emu, num)) return 0;
        return int21_early(emu, ah);
    }
    return ivt_empty(emu, num) ? 1 : 0;
}

static void recover_halt(x86emu_t *emu)
{
    emu->x86.mode &= ~_MODE_HALTED;
    emu->x86.R_FLG |= F_IF;
}

static int has_prompt(x86emu_t *emu)
{
    for (int row = 0; row < 25; ++row) {
        for (int col = 0; col < 76; ++col) {
            u32 off = VGA_BASE + (u32)((row * 80 + col) * 2);
            u8 d = x86emu_read_byte(emu, off);
            if ((d == 'C' || d == 'A') &&
                x86emu_read_byte(emu, off + 2) == ':' &&
                x86emu_read_byte(emu, off + 4) == '\\' &&
                x86emu_read_byte(emu, off + 6) == '>')
                return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "assets/dos/rtx_dos720.img";
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return 1; }

    x86emu_t *emu = x86emu_new(X86EMU_PERM_R | X86EMU_PERM_W | X86EMU_PERM_X,
        X86EMU_PERM_R | X86EMU_PERM_W);
    x86emu_set_memio_handler(emu, field_memio);
    x86emu_set_intr_handler(emu, field_int);
    init_bios(emu);

    FILE *hf = fopen("assets/dos/rtx_dos_hd.img", "rb");
    if (hf) {
        fseek(hf, 0, SEEK_END);
        hd_sz = (unsigned)ftell(hf);
        fseek(hf, 0, SEEK_SET);
        if (hd_sz > sizeof hd_buf) hd_sz = (unsigned)sizeof hd_buf;
        fread(hd_buf, 1, hd_sz, hf);
        fclose(hf);
        x86emu_write_byte(emu, BDA_BASE + 0x75, 2);
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz > (long)DISK_BYTES) sz = DISK_BYTES;
    for (long i = 0; i < sz; ++i) {
        int c = fgetc(f);
        if (c == EOF) break;
        x86emu_write_byte(emu, DISK_BASE + (u32)i, (u8)c);
        if (i < 512) x86emu_write_byte(emu, 0x7C00u + (u32)i, (u8)c);
    }
    fclose(f);

    x86emu_set_seg_register(emu, emu->x86.R_CS_SEL, 0);
    x86emu_set_seg_register(emu, emu->x86.R_DS_SEL, 0);
    x86emu_set_seg_register(emu, emu->x86.R_ES_SEL, 0);
    x86emu_set_seg_register(emu, emu->x86.R_SS_SEL, 0);
    emu->x86.R_EIP = 0x7C00;
    emu->x86.R_ESP = 0x7C00;
    emu->x86.R_FLG = 0x0202;

    int got_prompt = 0;
    for (int round = 0; round < 80; ++round) {
        if (emu->x86.R_TSC >= 12800 && emu->x86.R_TSC <= 13100)
            seed_stack(emu);
        recover_halt(emu);
        u64 t0 = emu->x86.R_TSC;
        emu->max_instr = t0 + 10000000;
        x86emu_run(emu, X86EMU_RUN_MAX_INSTR);
        recover_halt(emu);
        if (has_prompt(emu)) {
            got_prompt = 1;
            printf("PROMPT at tsc=%llu cs=%04x ip=%04x\n",
                (unsigned long long)emu->x86.R_TSC,
                (unsigned)emu->x86.R_CS, (unsigned)emu->x86.R_EIP);
            break;
        }
        if (round % 10 == 0)
            printf("round %d tsc=%llu cs=%04x ip=%04x ivt13=%04x:%04x ivt21=%04x:%04x\n", round,
                (unsigned long long)emu->x86.R_TSC,
                (unsigned)emu->x86.R_CS, (unsigned)emu->x86.R_EIP,
                x86emu_read_word(emu, 0x13 * 4 + 2), x86emu_read_word(emu, 0x13 * 4),
                x86emu_read_word(emu, 0x21 * 4 + 2), x86emu_read_word(emu, 0x21 * 4));
    }

    printf("done tsc=%llu ip=%04x cs=%04x ax=%04x ivt21=%04x:%04x\n",
        (unsigned long long)emu->x86.R_TSC,
        (unsigned)emu->x86.R_EIP, (unsigned)emu->x86.R_CS, (unsigned)emu->x86.R_EAX,
        x86emu_read_word(emu, 0x21 * 4 + 2), x86emu_read_word(emu, 0x21 * 4));

    for (int row = 0; row < 25; ++row) {
        for (int col = 0; col < 80; ++col) {
            u32 off = VGA_BASE + (u32)((row * 80 + col) * 2);
            putchar((char)x86emu_read_byte(emu, off));
        }
        putchar('\n');
    }

    x86emu_done(emu);
    return got_prompt ? 0 : 1;
}