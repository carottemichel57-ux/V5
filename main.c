// Saity V1.5 Admin - mod menu pour Minecraft PS Vita (plugin taiHEN .suprx)
// Ouvrir/fermer : R + GAUCHE
// Liste des categories : Haut/Bas choisir, Croix entrer, Rond fermer
// Dans une categorie   : Haut/Bas choisir, Croix activer, Gauche/Droite changer une valeur,
//                        L1/R1 categorie precedente/suivante, Rond (ou Gauche) retour
// CustomName : Croix ouvre le clavier integre du menu (le clavier PS Vita n'est plus utilise)
// SD codes   : categorie dont les cheats sont lus dans ux0:data/saity_codes.txt (Reload codes = recharger)
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/display.h>
#include <psp2/ctrl.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/dirent.h>
#include <taihen.h>
#include <stdint.h>
#include <string.h>
#include <psp2/kernel/clib.h>

/* cheats.h reste inchange : on renomme son tableau cats en cats_orig,
 * puis on cree un vrai tableau "cats" avec la categorie SD codes en plus (voir cats_init). */
#define cats cats_orig
#include "cheats.h"
#undef cats
#undef OPS_MAX
#define OPS_MAX 2400
#define K_RELOAD 100
#define K_WRITE 101
#define SD_FIX 2          /* items fixes : Reload codes + Write a cheat */
#define SD_MAX 48
static item_t sd_items[SD_MAX] = { { "Reload codes", K_RELOAD, 0, 0, 0, 0,0,0, 0 }, { "Write a cheat", K_WRITE, 0, 0, 0, 0,0,0, 0 } };
static cat_t cats[COUNT(cats_orig) + 1];

#define MENU_NAME "Saity V1.5 Admin"
#define OPEN_COMBO (SCE_CTRL_R1 | SCE_CTRL_LEFT)
#define NAME_ADDR  0x8234628Du   /* pseudo du joueur (chaine ASCII, 16 car. max) */
#define PTR(a) ((void *)(uintptr_t)(a))

#define RGBA(r,g,b) (0xFF000000u | ((uint32_t)(b) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(r))
#define C_BLACK RGBA(0,0,0)
static uint32_t col(int i) { return RGBA(th[i][0], th[i][1], th[i][2]); }

// melange : t/256 de la couleur b dans la couleur a
static uint32_t mixc(uint32_t a, uint32_t b, int t) {
    uint32_t r = 0xFF000000u;
    for (int s = 0; s < 24; s += 8) {
        int ca = (int)((a >> s) & 255u), cb = (int)((b >> s) & 255u);
        r |= (uint32_t)((ca * (256 - t) + cb * t) >> 8) << s;
    }
    return r;
}

#define L(en, fr) (lang ? (fr) : (en))
static const char *T(const char *s) {            // traduction EN -> FR (si langue = francais)
    if (!lang) return s;
    if (!strcmp(s, "SD codes")) return "Codes SD";
    if (!strcmp(s, "Reload codes")) return "Recharger codes";
    if (!strcmp(s, "Write a cheat")) return "Écrire un cheat";
    for (int i = 0; i < COUNT(TR); i++) if (!strcmp(TR[i][0], s)) return TR[i][1];
    return s;
}

static tai_hook_ref_t ref_disp, ref_peek, ref_read, ref_peek2, ref_read2;

static volatile uint32_t raw = 0;
static uint32_t old = 0;
static int menu_open = 0, view = 0, cur_cat = 0, cur_item = 0, cat_sel[16];   // view 0 = liste des categories, 1 = contenu
static char custom_name[17] = "";
static int input_block = 0;
static int kb_active = 0, kb_x = 0, kb_y = 0, kb_shift = 0;
static int kb_mode = 0;                       /* 0 = pseudo perso, 1 = nom du nouveau cheat, 2 = lignes du cheat */
#define KBC_MAX 1400
#define KBC_COLS 9
#define KBC_ROWS 2
static char kbc_text[KBC_MAX + 2]; static int kbc_n = 0;
static char wc_name[17] = "";
static char kb_text[17] = "";
static item_t *kb_item = 0;
static int name_ok(int ch) { return (ch >= 32 && ch < 127) || ch == 0xA7; }   // ASCII imprimable + §
static int toast_t = 0; static const char *toast_msg = ""; static char toast_buf[40];

// ---------------- police 5x7 (majuscules, chiffres, ponctuation) + minuscules 5x9 ----------------
static const char FONT_CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789></.-:()+_%!?,'";
static const uint8_t FONT[][7] = {
    {0,0,0,0,0,0,0},
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, {0x07,0x02,0x02,0x02,0x02,0x12,0x0C},
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
    {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}, {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    {0x11,0x11,0x11,0x0A,0x04,0x04,0x04}, {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
    {0x10,0x08,0x04,0x02,0x04,0x08,0x10}, {0x01,0x02,0x04,0x08,0x04,0x02,0x01},
    {0x01,0x01,0x02,0x04,0x08,0x10,0x10}, {0,0,0,0,0,0x0C,0x0C},
    {0,0,0,0x1F,0,0,0},                   {0,0x0C,0x0C,0,0x0C,0x0C,0},
    {0x02,0x04,0x08,0x08,0x08,0x04,0x02}, {0x08,0x04,0x02,0x02,0x02,0x04,0x08},
    {0,0x04,0x04,0x1F,0x04,0x04,0},       {0,0,0,0,0,0,0x1F},
    {0x19,0x19,0x02,0x04,0x08,0x13,0x13}, {0x04,0x04,0x04,0x04,0x04,0,0x04},
    {0x0E,0x11,0x01,0x02,0x04,0,0x04},    {0,0,0,0,0x0C,0x04,0x08},
    {0x04,0x04,0x08,0,0,0,0},                   /* ' */
};

// minuscules : 9 lignes (x-height 5, jambages sur les 2 dernieres lignes), bit 0x10 = pixel de gauche
static const uint8_t FONT_LC[26][9] = {
    {0,0,0x0E,0x01,0x0F,0x11,0x0F,0,0},          /* a */
    {0x10,0x10,0x16,0x19,0x11,0x11,0x1E,0,0},    /* b */
    {0,0,0x0E,0x10,0x10,0x11,0x0E,0,0},          /* c */
    {0x01,0x01,0x0D,0x13,0x11,0x11,0x0F,0,0},    /* d */
    {0,0,0x0E,0x11,0x1F,0x10,0x0E,0,0},          /* e */
    {0x06,0x09,0x08,0x1C,0x08,0x08,0x08,0,0},    /* f */
    {0,0,0x0F,0x11,0x11,0x0F,0x01,0x11,0x0E},    /* g */
    {0x10,0x10,0x16,0x19,0x11,0x11,0x11,0,0},    /* h */
    {0x04,0,0x0C,0x04,0x04,0x04,0x0E,0,0},       /* i */
    {0x02,0,0x06,0x02,0x02,0x02,0x02,0x12,0x0C}, /* j */
    {0x10,0x10,0x12,0x14,0x18,0x14,0x12,0,0},    /* k */
    {0x0C,0x04,0x04,0x04,0x04,0x04,0x0E,0,0},    /* l */
    {0,0,0x1A,0x15,0x15,0x11,0x11,0,0},          /* m */
    {0,0,0x16,0x19,0x11,0x11,0x11,0,0},          /* n */
    {0,0,0x0E,0x11,0x11,0x11,0x0E,0,0},          /* o */
    {0,0,0x1E,0x11,0x11,0x11,0x1E,0x10,0x10},    /* p */
    {0,0,0x0F,0x11,0x11,0x11,0x0F,0x01,0x01},    /* q */
    {0,0,0x16,0x19,0x10,0x10,0x10,0,0},          /* r */
    {0,0,0x0F,0x10,0x0E,0x01,0x1E,0,0},          /* s */
    {0x08,0x08,0x1C,0x08,0x08,0x09,0x06,0,0},    /* t */
    {0,0,0x11,0x11,0x11,0x13,0x0D,0,0},          /* u */
    {0,0,0x11,0x11,0x11,0x0A,0x04,0,0},          /* v */
    {0,0,0x11,0x11,0x15,0x15,0x0A,0,0},          /* w */
    {0,0,0x11,0x0A,0x04,0x0A,0x11,0,0},          /* x */
    {0,0,0x11,0x11,0x11,0x0F,0x01,0x11,0x0E},    /* y */
    {0,0,0x1F,0x02,0x04,0x08,0x1F,0,0},          /* z */
};

// accents francais (UTF-8 : 0xC3 xx). Majuscules accentuees -> lettre sans accent.
// acc : 0 aucun, 1 aigu, 2 grave, 3 circonflexe, 4 trema, 5 cedille
typedef struct { uint8_t cp; char base; uint8_t acc; } acc_t;
static const acc_t ACC[] = {
    {0xA0,'a',2},{0xA2,'a',3},{0xA4,'a',4},{0xA7,'c',5},{0xA8,'e',2},{0xA9,'e',1},{0xAA,'e',3},{0xAB,'e',4},
    {0xAE,'i',3},{0xAF,'i',4},{0xB4,'o',3},{0xB6,'o',4},{0xB9,'u',2},{0xBB,'u',3},{0xBC,'u',4},
    {0x80,'A',0},{0x82,'A',0},{0x87,'C',0},{0x88,'E',0},{0x89,'E',0},{0x8A,'E',0},{0x8E,'I',0},{0x94,'O',0},{0x99,'U',0},{0x9B,'U',0},
};

// ---------------- dessin ----------------
static uint32_t *fbp; static int fbw, fbh, fbpitch;

static void rect(int x, int y, int w, int h, uint32_t c) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > fbw) w = fbw - x;
    if (y + h > fbh) h = fbh - y;
    if (w <= 0 || h <= 0) return;
    for (int j = 0; j < h; j++) {
        uint32_t *p = fbp + (y + j) * fbpitch + x;
        for (int i = 0; i < w; i++) p[i] = c;
    }
}

static int isq(int n) { int r = 0; while ((r + 1) * (r + 1) <= n) r++; return r; }

static void rrect(int x, int y, int w, int h, int r, uint32_t c) {      // rectangle a coins arrondis
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    for (int j = 0; j < h; j++) {
        int dy = j < r ? r - j : (j >= h - r ? j - (h - r) + 1 : 0), inset = 0;
        if (dy > 0) inset = r - isq(r * r - dy * dy);
        rect(x + inset, y + j, w - 2 * inset, 1, c);
    }
}

// lit le caractere suivant (UTF-8) ; renvoie la lettre de base et l'accent, 0 en fin de chaine
static int next_char(const char **ps, int *acc) {
    const unsigned char *s = (const unsigned char *)*ps;
    int c = *s;
    *acc = 0;
    if (!c) return 0;
    if (c == 0xC3 && (s[1] & 0xC0) == 0x80) {
        *ps += 2;
        for (int i = 0; i < COUNT(ACC); i++)
            if (ACC[i].cp == s[1]) { *acc = ACC[i].acc; return ACC[i].base; }
        return '?';
    }
    if (c == 0xA7) { *ps += 1; return 0xA7; }     // signe § (octet unique, utilise dans les pseudos)
    if (c >= 0x80) {                              // autre sequence UTF-8 : ignoree
        int k = 1;
        while ((s[k] & 0xC0) == 0x80) k++;
        *ps += k;
        return '?';
    }
    *ps += 1;
    return c;
}

static int text_w(const char *s, int sc) {
    int n = 0, a;
    while (next_char(&s, &a)) n++;
    return n * 6 * sc;
}

static void glyph(int x, int y, int ch, int acc, int s, uint32_t c) {
    if (ch >= 'a' && ch <= 'z') {
        const uint8_t *g = FONT_LC[ch - 'a'];
        for (int r = 0; r < 9; r++) {
            if (acc && ch == 'i' && r == 0) continue;          // l'accent remplace le point du i
            for (int b = 0; b < 5; b++)
                if (g[r] & (0x10 >> b)) rect(x + b * s, y + r * s, s, s, c);
        }
        if (acc) {
            static const uint8_t A[5][2] = { {0,0}, {0x02,0x04}, {0x08,0x04}, {0x04,0x0A}, {0x00,0x0A} };
            if (acc < 5) {
                for (int r = 0; r < 2; r++) for (int b = 0; b < 5; b++)
                    if (A[acc][r] & (0x10 >> b)) rect(x + b * s, y + r * s, s, s, c);
            } else {                                          // cedille
                rect(x + 2 * s, y + 7 * s, s, s, c);
                rect(x + 1 * s, y + 8 * s, s, s, c);
            }
        }
        return;
    }
    if (ch == 0xA7) {                                         // §
        static const uint8_t SEC[7] = {0x0E,0x10,0x0E,0x11,0x0E,0x01,0x0E};
        for (int r = 0; r < 7; r++) for (int b = 0; b < 5; b++)
            if (SEC[r] & (0x10 >> b)) rect(x + b * s, y + r * s, s, s, c);
        return;
    }
    const char *p = (ch > 0) ? strchr(FONT_CHARS, ch) : 0;
    int idx = p ? (int)(p - FONT_CHARS) : 0;
    for (int r = 0; r < 7; r++) for (int b = 0; b < 5; b++)
        if (FONT[idx][r] & (0x10 >> b)) rect(x + b * s, y + r * s, s, s, c);
}

static void text(int x, int y, const char *s, uint32_t c, int sc) {
    int a, ch;
    while ((ch = next_char(&s, &a))) { glyph(x, y, ch, a, sc, c); x += 6 * sc; }
}

static void text_shadow(int x, int y, const char *s, uint32_t c, int sc) {
    text(x + 1, y + 1, s, C_BLACK, sc);
    text(x, y, s, c, sc);
}

// ---------------- acces memoire du jeu ----------------
typedef struct { uint32_t lo, hi, perms; } seg_t;
static seg_t segs[4]; static uintptr_t seg_base[4]; static int seg_ok = 0;

static int load_segs(void) {
    if (seg_ok) return 1;
    tai_module_info_t ti; ti.size = sizeof(ti);
    if (taiGetModuleInfo(TAI_MAIN_MODULE, &ti) < 0) return 0;
    SceKernelModuleInfo mi; memset(&mi, 0, sizeof(mi)); mi.size = sizeof(mi);
    if (sceKernelGetModuleInfo(ti.modid, &mi) < 0) return 0;
    for (int i = 0; i < 4; i++) {
        seg_base[i] = (uintptr_t)mi.segments[i].vaddr;
        segs[i].lo = (uint32_t)seg_base[i];
        segs[i].hi = segs[i].lo + (uint32_t)mi.segments[i].memsz;
        segs[i].perms = (uint32_t)mi.segments[i].perms;
    }
    seg_ok = 1; return 1;
}

static int seg_of(uint32_t a, uint32_t n) {
    for (int i = 0; i < 4; i++)
        if (segs[i].hi > segs[i].lo && a >= segs[i].lo && a + n <= segs[i].hi) return i;
    return -1;
}

// ---- zone de travail du plugin ----
// Certains codes VitaCheat (KillAura, Fishing Rod Jump, Boat Fly, baton) utilisent des adresses
// "libres" (0x82000800, 0x8A000600, 0x8B000000, 0x8B500000) comme memoire temporaire.
// On ne sait pas si elles existent / sont ecrivables dans le jeu : on les redirige vers un
// tampon du plugin. Les adresses ET les valeurs qui tombent dans ces fenetres sont converties,
// donc les pointeurs que le code range dans cette zone restent valides.
typedef struct { uint32_t base, size, off; } win_t;
static const win_t wins[] = {
    { 0x82000800u, 0x040, 0x000 },   /* Fishing Rod Jump */
    { 0x8A000600u, 0x200, 0x040 },   /* OneShot Stick    */
    { 0x8B000000u, 0x020, 0x240 },   /* Boat Fly         */
    { 0x8B500000u, 0x020, 0x260 },   /* KillAura         */
};
static uint8_t scratch[0x280] __attribute__((aligned(16)));

static uint32_t reloc(uint32_t a) {
    for (int i = 0; i < COUNT(wins); i++)
        if (a >= wins[i].base && a - wins[i].base < wins[i].size)
            return (uint32_t)(uintptr_t)scratch + wins[i].off + (a - wins[i].base);
    return a;
}

static int in_scratch(uint32_t a, uint32_t n) {
    uint32_t lo = (uint32_t)(uintptr_t)scratch;
    return a >= lo && a + n <= lo + (uint32_t)sizeof(scratch);
}

// les blocs memoire sont alignes sur des pages de 4 Ko : une page verifiee l'est pour toute la frame
static uint32_t pg_ok[32]; static int pg_n = 0, pg_i = 0;

static int page_ok(uint32_t a) {
    uint32_t pg = a >> 12;
    for (int i = 0; i < pg_n; i++) if (pg_ok[i] == pg) return 1;
    SceKernelMemBlockInfo bi;
    memset(&bi, 0, sizeof(bi)); bi.size = sizeof(bi);
    if (sceKernelGetMemBlockInfoByAddr(PTR(a), &bi) < 0) return 0;
    pg_ok[pg_i] = pg; pg_i = (pg_i + 1) % 32; if (pg_n < 32) pg_n++;
    return 1;
}

// adresse lisible (dans le module du jeu, ou bloc memoire connu du systeme)
static int mapped(uint32_t a, uint32_t n) {
    if (a < 0x1000) return 0;
    if (in_scratch(a, n)) return 1;
    if (seg_of(a, n) >= 0) return 1;
    return page_ok(a) && page_ok(a + n - 1);
}

static int rd32(uint32_t a, uint32_t *out) {
    if (!mapped(a, 4)) return 0;
    memcpy(out, PTR(a), 4);
    return *out != 0;                              // pointeur nul = chaine interrompue
}

static int rd32v(uint32_t a, uint32_t *out) {      // lecture d'une valeur (0 est valide)
    if (!mapped(a, 4)) return 0;
    memcpy(out, PTR(a), 4);
    return 1;
}

// ---------------- interpreteur de codes VitaCheat (.psv) ----------------
// Supporte : $0200/$A200 (ecriture 32 bits), $B200 (base de segment),
//            $32xx + $3200 + $3300 (chaine de pointeurs), $C2xx (condition bouton),
//            $5200 (copie 32 bits), $82xx/$88xx|$83xx/$86xx/$89xx (copie via pointeurs),
//            $D0..$DB (condition sur une valeur en memoire)
enum { OP_W32, OP_BASE, OP_P0, OP_PL, OP_PW, OP_COND,
       OP_DCOND, OP_MOV, OP_D0, OP_DL, OP_S0, OP_SL, OP_MV };
typedef struct { uint8_t k, saved, ok, w; uint32_t a, b, c, orig, addr; int uid; } op_t;
static op_t ops[OPS_MAX]; static int nops = 0, compiled = 0;

static const char *hex(const char *s, uint32_t *out) {
    uint32_t v = 0;
    while (*s == ' ') s++;
    for (;; s++) {
        int c = *s, d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else break;
        v = (v << 4) | (uint32_t)d;
    }
    *out = v; return s;
}

static int parse_line(const char *s, uint32_t *t, uint32_t *a, uint32_t *b) {
    if (*s != '$') return 0;
    s = hex(s + 1, t); s = hex(s, a); hex(s, b);
    return 1;
}

static int emit(int k, uint32_t a, uint32_t b) {
    if (nops >= OPS_MAX) return -1;
    memset(&ops[nops], 0, sizeof(op_t));
    ops[nops].k = (uint8_t)k; ops[nops].a = a; ops[nops].b = b;
    return nops++;
}

// compile une instruction ; renvoie 0 si le code n'est pas supporte
static int comp_stmt(const char *const *L, int n, int *i) {
    uint32_t t, a, b;
    if (*i >= n || !parse_line(L[*i], &t, &a, &b)) return 0;
    (*i)++;
    uint32_t hi = t >> 8, lo = t & 0xFF;
    if ((hi == 0x02 || hi == 0xA2) && lo == 0) return emit(OP_W32, a, b) >= 0;
    if (t == 0xB200) return emit(OP_BASE, a, b) >= 0;
    if (hi == 0x32 && lo >= 1) {
        if (emit(OP_P0, a, b) < 0) return 0;
        for (uint32_t k = 1; k < lo; k++) {
            uint32_t t2, a2, b2;
            if (*i >= n || !parse_line(L[*i], &t2, &a2, &b2) || t2 != 0x3200) return 0;
            (*i)++;
            if (emit(OP_PL, 0, b2) < 0) return 0;
        }
        uint32_t t3, a3, b3;
        if (*i >= n || !parse_line(L[*i], &t3, &a3, &b3) || (t3 >> 8) != 0x33) return 0;
        (*i)++;
        return emit(OP_PW, 0, b3) >= 0;
    }
    if (hi == 0xC2 && lo >= 1) {
        int idx = emit(OP_COND, b, 0);          // a = masque de boutons
        if (idx < 0) return 0;
        for (uint32_t k = 0; k < lo; k++) if (!comp_stmt(L, n, i)) return 0;
        ops[idx].b = (uint32_t)(nops - idx - 1); // nb d'ops a sauter si condition fausse
        return 1;
    }
    if (hi == 0x52 && lo == 0)                   // $5200 dest src : copie 32 bits
        return emit(OP_MOV, a, b) >= 0;
    if ((hi & 0xF0) == 0xD0 && (hi & 0x0F) <= 0x0B && lo >= 1) {
        // $DX.. adresse valeur : X = operateur (=, <>, >, <) x (8, 16, 32 bits)
        int idx = emit(OP_DCOND, a, 0);
        if (idx < 0) return 0;
        ops[idx].c = b; ops[idx].w = (uint8_t)(hi & 0x0F);
        for (uint32_t k = 0; k < lo; k++) if (!comp_stmt(L, n, i)) return 0;
        ops[idx].b = (uint32_t)(nops - idx - 1);
        return 1;
    }
    if (hi == 0x82 && lo >= 1) {
        // copie par pointeurs : $82xx (destination) ... $88xx/$83xx ... $86xx (source) ... $89xx
        if (emit(OP_D0, a, b) < 0) return 0;
        for (uint32_t k = 1; k < lo; k++) {
            uint32_t t2, a2, b2;
            if (*i >= n || !parse_line(L[*i], &t2, &a2, &b2) || t2 != 0x8200) return 0;
            (*i)++;
            if (emit(OP_DL, 0, b2) < 0) return 0;
        }
        uint32_t t3, a3, b3;
        if (*i >= n || !parse_line(L[*i], &t3, &a3, &b3) || ((t3 >> 8) != 0x88 && (t3 >> 8) != 0x83)) return 0;
        (*i)++;
        uint32_t t4, a4, b4;
        if (*i >= n || !parse_line(L[*i], &t4, &a4, &b4) || (t4 >> 8) != 0x86 || (t4 & 0xFF) < 1) return 0;
        (*i)++;
        if (emit(OP_S0, a4, b4) < 0) return 0;
        for (uint32_t k = 1; k < (t4 & 0xFF); k++) {
            uint32_t t5, a5, b5;
            if (*i >= n || !parse_line(L[*i], &t5, &a5, &b5) || t5 != 0x8600) return 0;
            (*i)++;
            if (emit(OP_SL, 0, b5) < 0) return 0;
        }
        uint32_t t6, a6, b6;
        if (*i >= n || !parse_line(L[*i], &t6, &a6, &b6) || (t6 >> 8) != 0x89) return 0;
        (*i)++;
        return emit(OP_MV, 0, 0) >= 0;
    }
    return 0;
}

static void compile_all(void) {
    if (compiled) return;
    compiled = 1;
    load_segs();
    for (int c = 0; c < COUNT(cats); c++) for (int j = 0; j < cats[c].nc; j++) {
        item_t *it = &cats[c].c[j];
        if (it->kind == K_CUSTOM) {              // 5 mots = 16 car. + terminateur
            int start = nops, ok = 1;
            for (int k = 0; k < 5; k++) if (emit(OP_W32, NAME_ADDR + 4u * k, 0) < 0) { ok = 0; break; }
            if (ok) { it->pc = start; it->pn = 5; it->st = 1; } else { nops = start; it->st = 2; }
            continue;
        }
        if (it->kind != K_CODE) continue;
        if (!it->lines) { it->st = 2; continue; }
        int n = 0; while (it->lines[n]) n++;
        int start = nops, i = 0, ok = 1;
        while (i < n) if (!comp_stmt(it->lines, n, &i)) { ok = 0; break; }
        if (!ok) { nops = start; it->st = 2; }
        else { it->pc = start; it->pn = nops - start; it->st = 1; }
    }
}

static void do_write(op_t *o, uint32_t addr, uint32_t val, int restore, int dyn) {
    if (restore) {
        if (!o->saved) return;
        if (o->uid > 0) { taiInjectRelease(o->uid); o->uid = 0; }
        else if (o->addr == addr && mapped(addr, 4)) memcpy(PTR(addr), &o->orig, 4);
        o->saved = 0;
        return;
    }
    if (!dyn) {                                   // adresse fixe : on verifie une seule fois
        if (o->ok == 0) o->ok = mapped(addr, 4) ? 1 : 2;
        if (o->ok == 2) return;
    } else if (!mapped(addr, 4)) return;          // adresse issue d'un pointeur : toujours verifier

    int s = seg_of(addr, 4);
    if (s >= 0 && !(segs[s].perms & 2)) {         // zone en lecture seule (code/rodata) -> taiHEN
        if (o->uid <= 0) {
            o->uid = taiInjectAbs(PTR(addr), &val, 4);
            if (o->uid > 0) { o->saved = 1; o->addr = addr; }
        }
        return;
    }
    if (!o->saved || o->addr != addr) { memcpy(&o->orig, PTR(addr), 4); o->saved = 1; o->addr = addr; }
    if (memcmp(PTR(addr), &val, 4)) memcpy(PTR(addr), &val, 4);
}

// adresse d'un code : relative au segment ($B200) si < 0x81000000, puis redirigee si zone de travail
static uint32_t abs_addr(uint32_t a, uint32_t base) {
    if (a < 0x81000000u) a += base;
    return reloc(a);
}

// condition $DX.. : compare la valeur en memoire (8/16/32 bits) a la valeur du code
static int dcond(const op_t *o, uint32_t base) {
    uint32_t a = abs_addr(o->a, base), v = 0, c = o->c;
    uint32_t sz = (o->w % 3 == 0) ? 1u : (o->w % 3 == 1) ? 2u : 4u;
    if (!mapped(a, sz)) return 0;
    memcpy(&v, PTR(a), sz);                        // little endian : v est deja a zero au-dessus
    if (sz == 1) c &= 0xFFu; else if (sz == 2) c &= 0xFFFFu;
    switch (o->w / 3) {
    case 0:  return v == c;
    case 1:  return v != c;
    case 2:  return v > c;
    default: return v < c;
    }
}

static void run_item(item_t *it, int restore) {
    uint32_t base = 0, cur = 0, dcur = 0, scur = 0; int cok = 0, dok = 0, sok = 0;
    for (int i = 0; i < it->pn; i++) {
        op_t *o = &ops[it->pc + i];
        switch (o->k) {
        case OP_BASE: base = (uint32_t)seg_base[o->a & 3] + o->b; break;
        case OP_COND: if (!restore && (raw & o->a) != o->a) i += (int)o->b; break;
        case OP_DCOND: if (!restore && !dcond(o, base)) i += (int)o->b; break;
        case OP_W32: {
            uint32_t addr = abs_addr(o->a, base);
            do_write(o, addr, reloc(o->b), restore, 0);
            break;
        }
        case OP_MOV: {                               // $5200 : [dest] = [src]
            uint32_t d = abs_addr(o->a, base), v;
            if (restore) { do_write(o, d, 0, 1, 0); break; }
            if (rd32v(abs_addr(o->b, base), &v)) do_write(o, d, v, 0, 0);
            break;
        }
        case OP_P0: cok = rd32(abs_addr(o->a, base), &cur); if (cok) cur += o->b; break;
        case OP_PL: if (cok) { cok = rd32(cur, &cur); if (cok) cur += o->b; } break;
        case OP_PW: if (cok) do_write(o, cur, o->b, restore, 1); break;
        // copie par pointeurs : chaine destination (D0/DL), chaine source (S0/SL), puis MV
        case OP_D0: dok = rd32(abs_addr(o->a, base), &dcur); if (dok) dcur += o->b; break;
        case OP_DL: if (dok) { dok = rd32(dcur, &dcur); if (dok) dcur += o->b; } break;
        case OP_S0: sok = rd32(abs_addr(o->a, base), &scur); if (sok) scur += o->b; break;
        case OP_SL: if (sok) { sok = rd32(scur, &scur); if (sok) scur += o->b; } break;
        case OP_MV: {
            uint32_t v;                              // pas de restauration : l'adresse change a chaque frame
            if (!restore && dok && sok && rd32v(scur, &v)) do_write(o, dcur, v, 0, 1);
            break;
        }
        }
    }
}

static void sd_reload(void);
static int sd_done = 0;

static void tick(void) {
    pg_n = 0; pg_i = 0;                            // les pages verifiees ne valent que pour une frame
    compile_all();
    if (!sd_done) { sd_done = 1; uc_load(); sd_quiet = 1; sd_reload(); sd_quiet = 0; }    // charge les codes du fichier SD au premier tour
    if (!seg_ok) load_segs();                      // reessaie tant que les segments du jeu ne sont pas lus
    for (int c = 0; c < COUNT(cats); c++) for (int j = 0; j < cats[c].nc; j++) {
        item_t *it = &cats[c].c[j];
        if ((it->kind == K_CODE || it->kind == K_CUSTOM) && it->on && it->st == 1) run_item(it, 0);
    }
}

// ---------------- reglages ----------------
static void set_theme(int idx) {
    theme_idx = idx;
    for (int i = 0; i < 4; i++) for (int j = 0; j < 3; j++) th[i][j] = themes[idx].c[i][j];
}

static int is_adjust(item_t *it) { return it->kind == K_THEME || it->kind == K_COLOR || it->kind == K_LANG; }

static int pal_find(int slot) {                  // index palette de la couleur actuelle (-1 = melange)
    for (int i = 0; i < (int)NPAL; i++)
        if (th[slot][0] == palette[i].c[0] && th[slot][1] == palette[i].c[1] && th[slot][2] == palette[i].c[2]) return i;
    return -1;
}

static void adjust(item_t *it, int dir) {
    if (it->kind == K_COLOR) {
        int sl = it->pc, i = pal_find(sl);
        i = (i < 0) ? (dir > 0 ? 0 : (int)NPAL - 1) : (i + dir + (int)NPAL) % (int)NPAL;
        for (int j = 0; j < 3; j++) th[sl][j] = palette[i].c[j];
        theme_idx = -1;                            // theme perso
    } else if (it->kind == K_THEME) {
        int i = theme_idx < 0 ? (dir > 0 ? 0 : (int)NTHEMES - 1) : (theme_idx + dir + (int)NTHEMES) % (int)NTHEMES;
        set_theme(i);
    } else if (it->kind == K_LANG) {
        lang = !lang;
    }
}

// ---------------- sauvegarde des reglages du menu ----------------
#define CFG_MAGIC 0x31544153u                        /* "SAT1" */
typedef struct {
    uint32_t magic;
    int32_t lang, wm, fps, act, kbd, theme;
    uint8_t th[4][3];
    char name[20];
} cfg_t;
static const char *const CFG_PATHS[] = {
    "ux0:data/saity_v1.cfg", "ur0:data/saity_v1.cfg", "ux0:tai/saity_v1.cfg", "ur0:tai/saity_v1.cfg",
    "ux0:user/00/savedata/PCSE00491/saity_v1.cfg", "uma0:data/saity_v1.cfg", "ux0:temp/saity_v1.cfg",
};
static int cfg_err = 0;                              /* code d'erreur du premier emplacement (affiche si la sauvegarde echoue) */

static item_t *find_custom(void) {
    for (int c = 0; c < COUNT(cats); c++) for (int j = 0; j < cats[c].nc; j++)
        if (cats[c].c[j].kind == K_CUSTOM) return &cats[c].c[j];
    return 0;
}

static int cfg_save(void) {
    cfg_t c; memset(&c, 0, sizeof(c)); cfg_err = 0;
    c.magic = CFG_MAGIC; c.lang = lang; c.wm = show_wm; c.fps = show_fps;
    c.act = show_active; c.kbd = use_kb; c.theme = theme_idx;
    for (int i = 0; i < 4; i++) for (int j = 0; j < 3; j++) c.th[i][j] = (uint8_t)th[i][j];
    memcpy(c.name, custom_name, sizeof(custom_name));
    for (int i = 0; i < COUNT(CFG_PATHS); i++) {
        SceUID fd = sceIoOpen(CFG_PATHS[i], SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
        if (fd < 0) { if (i == 0) cfg_err = fd; continue; }
        int w = sceIoWrite(fd, &c, sizeof(c));
        sceIoClose(fd);
        if (w == (int)sizeof(c)) return 1;
    }
    return 0;
}

static void apply_custom(item_t *it);

static void cfg_load(void) {
    cfg_t c;
    for (int i = 0; i < COUNT(CFG_PATHS); i++) {
        SceUID fd = sceIoOpen(CFG_PATHS[i], SCE_O_RDONLY, 0);
        if (fd < 0) continue;
        memset(&c, 0, sizeof(c));
        int r = sceIoRead(fd, &c, sizeof(c));
        sceIoClose(fd);
        if (r != (int)sizeof(c) || c.magic != CFG_MAGIC) continue;
        lang = c.lang ? 1 : 0; show_wm = c.wm ? 1 : 0; show_fps = c.fps ? 1 : 0;
        show_active = c.act ? 1 : 0; use_kb = c.kbd ? 1 : 0;
        theme_idx = (c.theme >= 0 && c.theme < (int)NTHEMES) ? c.theme : -1;
        for (int a = 0; a < 4; a++) for (int b = 0; b < 3; b++) th[a][b] = c.th[a][b];
        int n = 0;
        for (; n < 16 && name_ok((unsigned char)c.name[n]); n++) custom_name[n] = c.name[n];
        custom_name[n] = 0;
        item_t *it = find_custom();
        if (it && custom_name[0]) apply_custom(it);   // le pseudo est pret, il suffit de l'activer
        return;
    }
}

// ---------------- pseudo libre : clavier integre au menu (plus de clavier PS Vita) ----------------
static void apply_custom(item_t *it) {           // injecte custom_name dans les 5 ops du pseudo
    if (it->st != 1) return;
    uint8_t buf[20]; memset(buf, 0, sizeof(buf));
    memcpy(buf, custom_name, strlen(custom_name));
    for (int k = 0; k < 5; k++) { uint32_t w; memcpy(&w, buf + 4 * k, 4); ops[it->pc + k].b = w; }
}

static void group_off(item_t *keep) {            // un seul actif a la fois dans le groupe
    cat_t *cat = &cats[cur_cat];
    for (int j = 0; j < cat->nc; j++) {
        item_t *o = &cat->c[j];
        if (o != keep && o->grp == keep->grp && o->on) { o->on = 0; run_item(o, 1); }
    }
}

// valide un pseudo (ASCII imprimable, 16 car. max) : il remplace le pseudo du joueur et s'active
static void custom_commit(item_t *it, const char *name) {
    char tmp[17]; int n = 0;
    if (it->st != 1) return;
    for (int i = 0; name[i] && n < 16; i++)
        if (name_ok((unsigned char)name[i])) tmp[n++] = name[i];
    tmp[n] = 0;
    if (n == 0) return;
    memcpy(custom_name, tmp, sizeof(tmp));
    if (it->on) run_item(it, 1);                 // retire l'ancien pseudo avant d'ecrire le nouveau
    apply_custom(it);
    it->on = 1; group_off(it);
}

// clavier integre : 10 colonnes x 4 lignes, Triangle = majuscules
#define KB_COLS 10
#define KB_ROWS 5
static const char KB_CH[] = "abcdefghijklmnopqrstuvwxyz0123456789_-. " "\xA7" "!?()+%,':";   /* 50 touches : espace en fin de ligne 4, § en debut de ligne 5 */

static int kb_char(int i) {
    int c = (unsigned char)KB_CH[i];
    if (kb_shift && c >= 'a' && c <= 'z') c -= 32;
    return c;
}

static void kb_start(item_t *it) {
    if (it->st != 1) return;
    kb_active = 1; kb_item = it; kb_mode = 0;
    memcpy(kb_text, custom_name, sizeof(kb_text));
    kb_x = kb_y = 0; kb_shift = 0;
}

static void kb_close(void) { kb_active = 0; kb_item = 0; input_block = 15; }

static void toast(const char *m);
static void wr_commit(void);
static const char KBC_CH[] = "0123456789ABCDEF$ ";   /* 18 touches */

static void wr_start(void) {                     // etape 1 : nom du cheat
    kb_active = 1; kb_item = 0; kb_mode = 1;
    kb_text[0] = 0; kb_x = kb_y = 0; kb_shift = 0;
}

static void kbc_add(const char *t) {
    int l = (int)strlen(t);
    if (kbc_n + l < KBC_MAX) { memcpy(kbc_text + kbc_n, t, (size_t)l); kbc_n += l; kbc_text[kbc_n] = 0; }
}

static void wc_next(void) {                      // nom valide -> etape 2 : lignes
    if (!kb_text[0]) { toast(L("Enter a name", "Entre un nom")); return; }
    memcpy(wc_name, kb_text, sizeof(wc_name));
    kb_mode = 2; kbc_n = 0; kbc_text[0] = 0; kb_x = kb_y = 0;
}

static void kbc_input(uint32_t p) {
    if (p & SCE_CTRL_RIGHT) kb_x = (kb_x + 1) % KBC_COLS;
    if (p & SCE_CTRL_LEFT)  kb_x = (kb_x + KBC_COLS - 1) % KBC_COLS;
    if (p & SCE_CTRL_DOWN)  kb_y = (kb_y + 1) % KBC_ROWS;
    if (p & SCE_CTRL_UP)    kb_y = (kb_y + KBC_ROWS - 1) % KBC_ROWS;
    if (p & SCE_CTRL_CROSS) { char t[2] = { KBC_CH[kb_y * KBC_COLS + kb_x], 0 }; kbc_add(t); }
    if ((p & SCE_CTRL_SQUARE) && kbc_n > 0) kbc_text[--kbc_n] = 0;
    if (p & SCE_CTRL_TRIANGLE) kbc_add("\n");
    if (p & SCE_CTRL_L1) kbc_add("$0200 ");
    if (p & SCE_CTRL_R1) kbc_add("$B200 ");
    if (p & SCE_CTRL_START) wr_commit();
    else if (p & SCE_CTRL_CIRCLE) kb_close();
}

static void kb_input(uint32_t p) {
    if (kb_mode == 2) { kbc_input(p); return; }
    int n = (int)strlen(kb_text);
    if (p & SCE_CTRL_RIGHT) kb_x = (kb_x + 1) % KB_COLS;
    if (p & SCE_CTRL_LEFT)  kb_x = (kb_x + KB_COLS - 1) % KB_COLS;
    if (p & SCE_CTRL_DOWN)  kb_y = (kb_y + 1) % KB_ROWS;
    if (p & SCE_CTRL_UP)    kb_y = (kb_y + KB_ROWS - 1) % KB_ROWS;
    if ((p & SCE_CTRL_CROSS) && n < 16) { kb_text[n] = (char)kb_char(kb_y * KB_COLS + kb_x); kb_text[n + 1] = 0; }
    if ((p & SCE_CTRL_SQUARE) && n > 0) kb_text[n - 1] = 0;
    if (p & SCE_CTRL_TRIANGLE) kb_shift = !kb_shift;
    if (p & SCE_CTRL_START) {
        if (kb_mode == 1) { wc_next(); return; }
        if (kb_item) custom_commit(kb_item, kb_text);
        kb_close();
    }
    else if (p & SCE_CTRL_CIRCLE) kb_close();
}


static void toast(const char *m) { toast_msg = m; toast_t = 120; }

// ======================================================================
// CODES SD : charge des cheats depuis un fichier texte, sans recompiler.
// Fichier : ux0:data/saity_codes.txt (ou ux0:tai, ur0:tai, ur0:data, uma0:data)
//   # commentaire
//   [Nom du cheat]
//   $0200 8234628D 61657244
//   $0200 82346291 0000006D
// ======================================================================
#define SD_BUF   24576
#define SD_LINES 1200

static char sd_buf[SD_BUF];
static char sd_names[SD_MAX][32];
static const char *sd_lines[SD_LINES + SD_MAX + 2];
static int sd_ops_base = -1;

static char sd_status[24] = "";              // affiche a droite de "Reload codes"
static int sd_quiet = 0;

// ---- cheats crees dans le menu : stockes dans le plugin (uc_blob), sauvegardes en fichier si possible ----
#define UC_MAGIC 0x31435541u
static char uc_blob[SD_BUF];                 // format texte : [Nom]\n$xxxx xxxxxxxx xxxxxxxx\n...
static const char *const UC_PATHS[] = {
    "ux0:data/saity_cheats.dat", "ur0:data/saity_cheats.dat", "ux0:tai/saity_cheats.dat", "ur0:tai/saity_cheats.dat",
    "ux0:user/00/savedata/PCSE00491/saity_cheats.dat", "uma0:data/saity_cheats.dat", "ux0:temp/saity_cheats.dat",
};

static int uc_save(void) {
    uint32_t hdr[2]; hdr[0] = UC_MAGIC; hdr[1] = (uint32_t)strlen(uc_blob);
    for (int i = 0; i < COUNT(UC_PATHS); i++) {
        SceUID fd = sceIoOpen(UC_PATHS[i], SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
        if (fd < 0) continue;
        int w1 = sceIoWrite(fd, hdr, 8);
        int w2 = hdr[1] ? sceIoWrite(fd, uc_blob, hdr[1]) : 0;
        sceIoClose(fd);
        if (w1 == 8 && w2 == (int)hdr[1]) return 1;
    }
    return 0;
}

static void uc_load(void) {
    for (int i = 0; i < COUNT(UC_PATHS); i++) {
        SceUID fd = sceIoOpen(UC_PATHS[i], SCE_O_RDONLY, 0);
        if (fd < 0) continue;
        uint32_t hdr[2] = { 0, 0 };
        int r = sceIoRead(fd, hdr, 8), ok = 0;
        if (r == 8 && hdr[0] == UC_MAGIC && hdr[1] < SD_BUF) {
            int n = hdr[1] ? sceIoRead(fd, uc_blob, hdr[1]) : 0;
            if (n == (int)hdr[1]) { uc_blob[n] = 0; ok = 1; }
        }
        sceIoClose(fd);
        if (ok) return;
    }
}

static cat_t *sd_cat(void) {
    for (int c = 0; c < COUNT(cats); c++) if (cats[c].c == sd_items) return &cats[c];
    return 0;
}

// compile les lignes d'un item (meme logique que compile_all)
static void sd_comp(item_t *it) {
    if (!it->lines || !it->lines[0]) { it->st = 2; return; }
    int n = 0; while (it->lines[n]) n++;
    int start = nops, i = 0, ok = 1;
    while (i < n) if (!comp_stmt(it->lines, n, &i)) { ok = 0; break; }
    if (!ok) { nops = start; it->st = 2; }
    else { it->pc = start; it->pn = nops - start; it->st = 1; }
}

static void sd_reload(void) {
    cat_t *cat = sd_cat();
    if (!cat) return;
    compile_all();
    if (sd_ops_base < 0) sd_ops_base = nops;

    // 1. coupe les anciens cheats (remet les valeurs d'origine), puis libere leurs ops
    for (int j = SD_FIX; j < cat->nc; j++)
        if (sd_items[j].on) { run_item(&sd_items[j], 1); sd_items[j].on = 0; }
    nops = sd_ops_base;
    cat->nc = SD_FIX;

    // 2. copie et decoupe les cheats du plugin
    size_t bl = strlen(uc_blob);
    memcpy(sd_buf, uc_blob, bl + 1);

    int ni = SD_FIX, nl = 0;
    item_t *cur = 0;
    char *p = sd_buf;
    while (*p) {
        char *e = p;
        while (*e && *e != '\n' && *e != '\r') e++;
        char *nx = e;
        if (*e) nx = (e[0] == '\r' && e[1] == '\n') ? e + 2 : e + 1;
        *e = 0;
        while (*p == ' ' || *p == '\t') p++;

        if (*p == '[') {
            char *q = strchr(p, ']');
            if (cur) sd_lines[nl++] = 0;          // ferme l'item precedent
            cur = 0;
            if (q && ni < SD_MAX) {
                *q = 0;
                strncpy(sd_names[ni], p + 1, sizeof(sd_names[ni]) - 1);
                sd_names[ni][sizeof(sd_names[ni]) - 1] = 0;
                cur = &sd_items[ni];
                memset(cur, 0, sizeof(*cur));
                cur->name = sd_names[ni];
                cur->kind = K_CODE;
                cur->lines = &sd_lines[nl];
                ni++;
            }
        } else if (*p == '$' && cur && nl < SD_LINES) {
            sd_lines[nl++] = p;
        }
        p = nx;
    }
    if (cur) sd_lines[nl++] = 0;

    // 3. compile
    for (int j = SD_FIX; j < ni; j++) sd_comp(&sd_items[j]);
    cat->nc = ni;

    if (cur_cat < COUNT(cats) && cur_item >= cats[cur_cat].nc) cur_item = 0;
    for (int c = 0; c < COUNT(cats) && c < 16; c++) if (cat_sel[c] >= cats[c].nc) cat_sel[c] = 0;

    sceClibSnprintf(sd_status, sizeof(sd_status), "%d OK", ni - SD_FIX);
    if (!sd_quiet) {
        sceClibSnprintf(toast_buf, sizeof(toast_buf), "%d %s", ni - SD_FIX, L("codes loaded", "codes charges"));
        toast(toast_buf);
    }
}

// valide les lignes tapees : renvoie 0 si OK (out = lignes formatees), sinon le n° de ligne fautive, -1 si trop long
static int wr_fmt(char *out, int cap, int *nlines) {
    int o = 0, ln = 0, cnt = 0;
    const char *p = kbc_text;
    *nlines = 0;
    while (*p) {
        const char *e = p;
        while (*e && *e != '\n') e++;
        ln++;
        char hx[24]; int h = 0, ok = 1, seen = 0;
        for (const char *q = p; q < e; q++) {
            int c = (unsigned char)*q;
            if (c == ' ' || c == '\r' || c == '\t') continue;
            if (!seen) { seen = 1; if (c != '$') { ok = 0; break; } continue; }
            if (c >= 'a' && c <= 'f') c -= 32;
            if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) { if (h < 20) hx[h] = (char)c; h++; }
            else { ok = 0; break; }
        }
        if (seen) {
            if (!ok || h != 20) return ln;
            if (o + 26 > cap) return -1;
            out[o++] = '$'; memcpy(out + o, hx, 4); o += 4; out[o++] = ' ';
            memcpy(out + o, hx + 4, 8); o += 8; out[o++] = ' ';
            memcpy(out + o, hx + 12, 8); o += 8; out[o++] = '\n';
            cnt++;
        }
        p = *e ? e + 1 : e;
    }
    out[o] = 0; *nlines = cnt;
    return 0;
}

static void wr_commit(void) {
    char out[2048]; int nl = 0;
    int r = wr_fmt(out, sizeof(out), &nl);
    if (r > 0) { sceClibSnprintf(toast_buf, sizeof(toast_buf), "%s %d", L("Invalid line", "Ligne invalide"), r); toast(toast_buf); return; }
    if (r < 0) { toast(L("Too long", "Trop long")); return; }
    if (nl == 0) { toast(L("No lines", "Aucune ligne")); return; }
    cat_t *cat = sd_cat();
    size_t bl = strlen(uc_blob);
    if (!cat || bl + strlen(wc_name) + strlen(out) + 4 >= SD_BUF || cat->nc >= SD_MAX) { toast(L("Memory full", "Memoire pleine")); return; }
    sceClibSnprintf(uc_blob + bl, SD_BUF - bl, "[%s]\n%s", wc_name, out);
    kb_close();
    sd_quiet = 1; sd_reload(); sd_quiet = 0;
    int saved = uc_save();
    item_t *it = &sd_items[cat->nc - 1];
    cur_item = cat->nc - 1;
    if (it->st == 2) sceClibSnprintf(toast_buf, sizeof(toast_buf), "%s", L("Added: unsupported (WIP)", "Ajoute : non supporte (WIP)"));
    else if (saved) sceClibSnprintf(toast_buf, sizeof(toast_buf), "%s", L("Cheat added + saved", "Cheat ajoute + sauve"));
    else sceClibSnprintf(toast_buf, sizeof(toast_buf), "%s", L("Added (not saved to file)", "Ajoute (pas sauve)"));
    toast(toast_buf);
}

static void change_cat(int d) {
    int n = COUNT(cats);
    cat_sel[cur_cat] = cur_item;
    cur_cat = (cur_cat + d + n) % n;
    cur_item = cat_sel[cur_cat];
    if (cur_item >= cats[cur_cat].nc) cur_item = 0;
}

static void activate(item_t *it) {
    switch (it->kind) {
    case K_CODE:
        if (it->st != 1) return;                   // WIP
        it->on = !it->on;
        if (!it->on) run_item(it, 1);              // remet les valeurs d'origine
        else if (it->grp) group_off(it);
        break;
    case K_CUSTOM: kb_start(it); break;
    case K_FLAG: *it->var = !*it->var; break;
    case K_ACT: set_theme(0); break;
    case K_RELOAD: sd_reload(); break;
    case K_WRITE: wr_start(); break;
    case K_SAVE:
        if (cfg_save()) toast("Saved");
        else {
            sceClibSnprintf(toast_buf, sizeof(toast_buf), "%s %08X", L("Save failed", "Echec sauvegarde"), (unsigned)cfg_err);
            toast(toast_buf);
        }
        break;
    default: break;
    }
}

// ---------------- entrees ----------------
static void update_input(void) {
    uint32_t b = raw, p = b & ~old; old = b;
    if (input_block > 0) { input_block--; return; }   // evite qu'une touche encore appuyee declenche une action

    if ((b & OPEN_COMBO) == OPEN_COMBO && (p & OPEN_COMBO)) { if (!kb_active) menu_open = !menu_open; return; }
    if (!menu_open) return;
    if (kb_active) { kb_input(p); return; }

    int nc = COUNT(cats);
    if (view == 0) {                               // liste des categories
        if (p & SCE_CTRL_DOWN) cur_cat = (cur_cat + 1) % nc;
        if (p & SCE_CTRL_UP)   cur_cat = (cur_cat + nc - 1) % nc;
        if (p & (SCE_CTRL_CROSS | SCE_CTRL_RIGHT)) {
            view = 1;
            cur_item = cat_sel[cur_cat];
            if (cur_item >= cats[cur_cat].nc) cur_item = 0;
        }
        if (p & SCE_CTRL_CIRCLE) menu_open = 0;
        return;
    }

    cat_t *cat = &cats[cur_cat];                   // contenu d'une categorie
    int n = cat->nc;
    if (n > 0) {
        if (p & SCE_CTRL_DOWN) cur_item = (cur_item + 1) % n;
        if (p & SCE_CTRL_UP)   cur_item = (cur_item + n - 1) % n;
    }
    item_t *it = n > 0 ? &cat->c[cur_item] : 0;
    int adj = it && is_adjust(it);

    if (p & SCE_CTRL_L1) change_cat(-1);
    if (p & SCE_CTRL_R1) change_cat(+1);
    if (p & SCE_CTRL_RIGHT) { if (adj) adjust(it, +1); }
    if (p & SCE_CTRL_LEFT)  { if (adj) adjust(it, -1); else { cat_sel[cur_cat] = cur_item; view = 0; } }
    if (p & SCE_CTRL_CROSS) { if (it) { if (adj) adjust(it, +1); else activate(it); } }
    if (p & SCE_CTRL_SQUARE) {
        if (it && adj) adjust(it, -1);
        else if (it && it->kind == K_CUSTOM && it->on) { it->on = 0; run_item(it, 1); }   // coupe le pseudo perso
    }
    if (p & SCE_CTRL_CIRCLE) { cat_sel[cur_cat] = cur_item; view = 0; }
}

static void filter(SceCtrlData *d, int n) {
    raw = d[n - 1].buttons;
    if (!menu_open) return;
    for (int i = 0; i < n; i++) { d[i].buttons = 0; d[i].lx = d[i].ly = d[i].rx = d[i].ry = 127; }
}

static int peek_h(int port, SceCtrlData *d, int n)  { int r = TAI_CONTINUE(int, ref_peek, port, d, n);  if (r > 0) filter(d, r); return r; }
static int read_h(int port, SceCtrlData *d, int n)  { int r = TAI_CONTINUE(int, ref_read, port, d, n);  if (r > 0) filter(d, r); return r; }
static int peek2_h(int port, SceCtrlData *d, int n) { int r = TAI_CONTINUE(int, ref_peek2, port, d, n); if (r > 0) filter(d, r); return r; }
static int read2_h(int port, SceCtrlData *d, int n) { int r = TAI_CONTINUE(int, ref_read2, port, d, n); if (r > 0) filter(d, r); return r; }

// ---------------- affichage ----------------
static uint64_t fps_t0 = 0; static int fps_frames = 0, fps_val = 0;

static void fps_update(void) {
    uint64_t now = sceKernelGetProcessTimeWide();
    if (!fps_t0) fps_t0 = now;
    fps_frames++;
    if (now - fps_t0 >= 500000) {
        fps_val = (int)((uint64_t)fps_frames * 1000000ULL / (now - fps_t0));
        fps_frames = 0; fps_t0 = now;
    }
}

static int is_on(const item_t *c) { return (c->kind == K_CODE && c->st == 1 && c->on) || (c->kind == K_CUSTOM && c->on); }

// couleur c si elle ressort sur le fond bg, sinon alt (ex. theme "Pink solid" : accent = fond)
static uint32_t vis(uint32_t c, uint32_t bg, uint32_t alt) {
    int d = 0;
    for (int s = 0; s < 24; s += 8) {
        int x = (int)((c >> s) & 255u) - (int)((bg >> s) & 255u);
        d += x < 0 ? -x : x;
    }
    return d < 90 ? alt : c;
}

// HUD en haut a gauche : toujours affiche (menu ferme ou non), couleur d'accent du theme
static void draw_overlay(void) {
    int y = 10; char buf[40]; uint32_t a = col(0);
    if (show_wm) {
        text_shadow(10, y, MENU_NAME, a, 2);
        text_shadow(10 + text_w(MENU_NAME, 2) + 12, y, L("(R+LEFT)", "(R+GAUCHE)"), a, 2);
        y += 22;
    }
    if (show_fps) { sceClibSnprintf(buf, sizeof(buf), "FPS: %d", fps_val); text_shadow(10, y, buf, a, 2); y += 22; }
    if (show_active) {
        const int MAXL = 8;
        int cnt = 0, shown = 0;
        for (int c = 0; c < COUNT(cats); c++) for (int j = 0; j < cats[c].nc; j++) if (is_on(&cats[c].c[j])) cnt++;
        if (!cnt) return;
        sceClibSnprintf(buf, sizeof(buf), "%s (%d)", T("Active cheats"), cnt);
        text_shadow(10, y, buf, a, 2); y += 22;
        for (int c = 0; c < COUNT(cats) && shown < MAXL; c++) for (int j = 0; j < cats[c].nc && shown < MAXL; j++) {
            item_t *it = &cats[c].c[j];
            if (!is_on(it)) continue;
            text_shadow(10, y, (it->kind == K_CUSTOM && custom_name[0]) ? custom_name : T(it->name), a, 2);
            y += 20; shown++;
        }
        if (cnt > shown) {
            sceClibSnprintf(buf, sizeof(buf), "+%d %s", cnt - shown, L("more", "de plus"));
            text_shadow(10, y, buf, a, 2);
        }
    }
}

// ---- menu : panneau compact, liste de categories puis contenu ----
#define MN_W      350      /* largeur du panneau */
#define MN_PAD    16
#define MN_ROW    26       /* hauteur d'une ligne */
#define MN_HEAD   46
#define MN_FOOT   36
#define MN_MARG   20       /* marge avec le bord de l'ecran */
#define MN_MAXVIS 9        /* lignes visibles max */

static void sw(int x, int y, int on) {            // interrupteur 34x16
    rrect(x, y, 34, 16, 8, on ? RGBA(70,200,110) : RGBA(74,74,88));
    rrect(on ? x + 21 : x + 3, y + 3, 10, 10, 5, RGBA(245,245,250));
}

// texte tronque avec ".." s'il depasse maxw
static void text_fit(int x, int y, const char *s, int maxw, uint32_t c, int sc) {
    if (text_w(s, sc) <= maxw) { text(x, y, s, c, sc); return; }
    char buf[64]; int bl = 0, k = 0, a, maxn = maxw / (6 * sc) - 2;
    const char *q = s;
    while (k < maxn && *q && bl < 58) {
        const char *pv = q; next_char(&q, &a);
        memcpy(buf + bl, pv, (size_t)(q - pv)); bl += (int)(q - pv); k++;
    }
    buf[bl++] = '.'; buf[bl++] = '.'; buf[bl] = 0;
    text(x, y, buf, c, sc);
}

static int active_in(const cat_t *c) {
    int k = 0;
    for (int j = 0; j < c->nc; j++) if (is_on(&c->c[j])) k++;
    return k;
}

static void panel(int *px, int *py, int ph, uint32_t bg) {
    *px = fbw - MN_W - MN_MARG; if (*px < 0) *px = 0;
    *py = MN_MARG;
    if (*py + ph > fbh) *py = fbh - ph;
    if (*py < 0) *py = 0;
    rrect(*px, *py, MN_W, ph, 12, bg);             // fond plein : ecriture seule, pas de lecture de la memoire video
}

static void draw_toast(int px, int py, int ph) {
    if (toast_t <= 0) return;
    uint32_t bg = col(1), acc = vis(col(0), bg, col(3));
    const char *m = T(toast_msg);
    int w = text_w(m, 2) + 28, ty = py + ph + 8;
    if (ty + 28 > fbh) ty = fbh - 28;
    rrect(px + MN_W - w, ty, w, 28, 14, acc);
    text(px + MN_W - w + 14, ty + 7, m, bg, 2);
}

static void draw_kbc(void) {
    uint32_t bg = col(1), sel = col(2), txt = col(3), dim = mixc(txt, bg, 130), acc = vis(col(0), bg, txt);
    const int gw = KBC_COLS * 32, pad = (MN_W - gw) / 2, ph = 304;
    int px, py;
    panel(&px, &py, ph, bg);
    rrect(px + pad, py + 15, 4, 14, 2, acc);
    text(px + pad + 12, py + 15, wc_name, txt, 2);
    int fy = py + 40;
    rrect(px + 12, fy, MN_W - 24, 5 * 18 + 12, 8, sel);
    int starts[128], nl = 0;
    starts[nl++] = 0;
    for (int i = 0; i < kbc_n; i++) if (kbc_text[i] == '\n' && nl < 128) starts[nl++] = i + 1;
    int first = nl > 5 ? nl - 5 : 0;
    for (int l = first; l < nl; l++) {
        int st = starts[l], en = (l + 1 < nl) ? starts[l + 1] - 1 : kbc_n, len = en - st;
        char b[40];
        if (len < 0) len = 0;
        if (len > 25) len = 25;
        memcpy(b, kbc_text + st, (size_t)len); b[len] = 0;
        int ly = fy + 8 + (l - first) * 18;
        text(px + 22, ly, b, txt, 2);
        if (l == nl - 1) rect(px + 22 + text_w(b, 2), ly - 1, 2, 16, acc);
    }
    int gy = fy + 5 * 18 + 12 + 8;
    for (int r = 0; r < KBC_ROWS; r++) for (int c = 0; c < KBC_COLS; c++) {
        int ch = (unsigned char)KBC_CH[r * KBC_COLS + c], x = px + pad + c * 32, y = gy + r * 32;
        int cur = (r == kb_y && c == kb_x);
        if (cur) rrect(x + 1, y + 1, 30, 30, 8, acc);
        if (ch == ' ') text(x + 7, y + 12, "spc", cur ? bg : txt, 1);
        else { char s[2] = { (char)ch, 0 }; text(x + 11, y + 7, s, cur ? bg : txt, 2); }
    }
    int hy = gy + KBC_ROWS * 32 + 8;
    rect(px + 12, hy, MN_W - 24, 1, mixc(txt, bg, 200));
    text(px + 12, hy + 8,  L("X key   Square delete", "X touche  Carré effacer"), dim, 2);
    text(px + 12, hy + 26, L("Triangle new line", "Triangle ligne"), dim, 2);
    text(px + 12, hy + 44, "L1 $0200   R1 $B200", dim, 2);
    text(px + 12, hy + 62, L("Start save  O cancel", "Start valider  O annuler"), dim, 2);
    draw_toast(px, py, ph);
}

static void draw_kb(void) {
    uint32_t bg = col(1), sel = col(2), txt = col(3), dim = mixc(txt, bg, 130), acc = vis(col(0), bg, txt);
    const int gw = KB_COLS * 32, pad = (MN_W - gw) / 2, ph = 324;
    int px, py;
    panel(&px, &py, ph, bg);
    rrect(px + pad, py + 15, 4, 14, 2, acc);
    text(px + pad + 12, py + 15, kb_mode == 1 ? L("Cheat name", "Nom du cheat") : L("Custom name", "Pseudo perso"), txt, 2);
    int fy = py + 40;
    rrect(px + pad, fy, gw, 32, 8, sel);
    text(px + pad + 10, fy + 9, kb_text, txt, 2);
    rect(px + pad + 10 + text_w(kb_text, 2), fy + 7, 2, 18, acc);                 // curseur
    const char *sh = kb_shift ? "ABC" : "abc";
    text(px + pad + gw - 10 - text_w(sh, 2), fy + 9, sh, dim, 2);
    int gy = fy + 32 + 8;
    for (int r = 0; r < KB_ROWS; r++) for (int c = 0; c < KB_COLS; c++) {
        int ch = kb_char(r * KB_COLS + c), x = px + pad + c * 32, y = gy + r * 32;
        int cur = (r == kb_y && c == kb_x);
        if (cur) rrect(x + 1, y + 1, 30, 30, 8, acc);
        if (ch == ' ') text(x + 7, y + 12, "spc", cur ? bg : txt, 1);
        else { char s[2] = { (char)ch, 0 }; text(x + 11, y + 7, s, cur ? bg : txt, 2); }
    }
    int hy = gy + KB_ROWS * 32 + 8;
    rect(px + pad, hy, gw, 1, mixc(txt, bg, 200));
    text(px + pad, hy + 10, L("X type   Square delete", "X taper   Carré effacer"), dim, 2);
    text(px + pad, hy + 30, L("Triangle shift   Start ok", "Triangle maj   Start ok"), dim, 2);
    text(px + pad, hy + 50, L("O cancel", "O annuler"), dim, 2);
    draw_toast(px, py, ph);
}

static void draw_menu(void) {
    if (kb_active) { if (kb_mode == 2) draw_kbc(); else draw_kb(); return; }

    uint32_t bg = col(1), sel = col(2), txt = col(3), dim = mixc(txt, bg, 110), acc = vis(col(0), bg, txt);
    int ncat = COUNT(cats);
    cat_t *cat = &cats[cur_cat];
    int n = view == 0 ? ncat : cat->nc;
    int cur = view == 0 ? cur_cat : cur_item;

    int maxvis = (fbh - 2 * MN_MARG - MN_HEAD - MN_FOOT) / MN_ROW;      // s'adapte a la hauteur de l'ecran
    if (maxvis > MN_MAXVIS) maxvis = MN_MAXVIS;
    if (maxvis < 3) maxvis = 3;
    int rows = n < maxvis ? n : maxvis;
    int start = cur >= rows ? cur - rows + 1 : 0;
    int ph = MN_HEAD + rows * MN_ROW + MN_FOOT;
    int px, py;
    char buf[24];
    panel(&px, &py, ph, bg);

    // ---- en-tete ----
    if (view == 0) {
        rrect(px + MN_PAD, py + 16, 4, 14, 2, acc);
        text(px + MN_PAD + 12, py + 16, MENU_NAME, txt, 2);
    } else {
        text(px + MN_PAD, py + 16, "<", dim, 2);
        text(px + MN_PAD + 20, py + 16, T(cat->name), txt, 2);
        sceClibSnprintf(buf, sizeof(buf), "%d/%d", n > 0 ? cur_item + 1 : 0, n);
        text(px + MN_W - MN_PAD - text_w(buf, 2), py + 16, buf, dim, 2);
    }
    rect(px + MN_PAD, py + MN_HEAD - 8, MN_W - 2 * MN_PAD, 1, mixc(txt, bg, 200));

    // ---- lignes ----
    int rx = px + MN_W - MN_PAD - (n > rows ? 8 : 0);               // bord droit des valeurs
    for (int i = 0; i < rows; i++) {
        int idx = start + i, y = py + MN_HEAD + i * MN_ROW;
        int is_cur = (idx == cur);
        int nx = px + MN_PAD + 4, rw = 0;
        const char *name; uint32_t ncol;

        if (is_cur) { rrect(px + 8, y, MN_W - 16, MN_ROW - 2, 7, sel); rrect(px + 8, y + 5, 3, MN_ROW - 12, 1, acc); }

        if (view == 0) {                                          // categorie : nom + cheats actifs + >
            int k = active_in(&cats[idx]);
            name = T(cats[idx].name); ncol = is_cur ? txt : dim;
            text(rx - 12, y + 5, ">", is_cur ? txt : dim, 2); rw = 12;
            if (k > 0) {
                sceClibSnprintf(buf, sizeof(buf), "%d", k);
                int bw = text_w(buf, 2) + 12;
                rrect(rx - 12 - 8 - bw, y + 3, bw, 18, 9, acc);
                text(rx - 12 - 8 - bw + 6, y + 5, buf, bg, 2);
                rw += 8 + bw;
            }
        } else {                                                   // element de la categorie
            item_t *c = &cat->c[idx];
            int on = is_on(c) || (c->kind == K_FLAG && *c->var);
            const char *s = 0; uint32_t sc2 = dim;
            name = T(c->name); ncol = (is_cur || on) ? txt : dim;
            switch (c->kind) {
            case K_CODE:
                if (c->st == 2) s = "WIP"; else { sw(rx - 34, y + 4, c->on); rw = 34; }
                break;
            case K_FLAG: sw(rx - 34, y + 4, *c->var); rw = 34; break;
            case K_CUSTOM:
                if (c->on && custom_name[0]) { s = custom_name; sc2 = acc; } else s = ">";
                break;
            case K_THEME: s = theme_idx >= 0 ? T(themes[theme_idx].name) : T("Custom"); sc2 = is_cur ? acc : dim; break;
            case K_LANG:  s = lang ? "Français" : "English"; sc2 = is_cur ? acc : dim; break;
            case K_COLOR: {
                int pi = pal_find(c->pc);
                s = pi >= 0 ? T(palette[pi].name) : T(theme_idx >= 0 ? "Theme" : "Mix");
                rrect(rx - text_w(s, 2) - 24, y + 4, 16, 16, 4, col(c->pc));
                rw = 24;
                sc2 = is_cur ? acc : dim;
                break;
            }
            case K_RELOAD: s = sd_status[0] ? sd_status : ">"; sc2 = acc; break;
            default: s = ">"; break;                               // K_ACT, K_SAVE
            }
            if (s) { text(rx - text_w(s, 2), y + 5, s, sc2, 2); rw += text_w(s, 2); }
        }
        text_fit(nx, y + 5, name, rx - rw - 10 - nx, ncol, 2);
    }

    // barre de defilement
    if (n > rows) {
        int h = rows * MN_ROW - 2, x = px + MN_W - 11, y0 = py + MN_HEAD;
        int th = h * rows / n; if (th < 12) th = 12;
        int ty = y0 + (h - th) * start / (n - rows);
        rrect(x, y0, 3, h, 1, mixc(txt, bg, 215));
        rrect(x, ty, 3, th, 1, acc);
    }

    // ---- aide ----
    const char *h1;
    if (view == 0) h1 = L("X open   O close", "X ouvrir   O fermer");
    else {
        item_t *c = n > 0 ? &cat->c[cur_item] : 0;
        h1 = L("O back   X select", "O retour   X choisir");
        if (c && is_adjust(c)) h1 = L("< > change   O back", "< > changer   O retour");
        if (c && c->kind == K_CUSTOM) h1 = L("X edit   Square off", "X éditer   Carré off");
    }
    int hy = py + MN_HEAD + rows * MN_ROW + 4;
    rect(px + MN_PAD, hy, MN_W - 2 * MN_PAD, 1, mixc(txt, bg, 200));
    text(px + MN_PAD, hy + 11, h1, dim, 2);

    draw_toast(px, py, ph);
}

static int cfg_done = 0;

static int disp_h(const SceDisplayFrameBuf *fb, int sync) {
    update_input();
    tick();                                        // compile les codes au premier appel
    if (!cfg_done) { cfg_done = 1; cfg_load(); }   // reglages sauvegardes
    if (show_fps) fps_update();
    if (toast_t > 0) toast_t--;
    if (fb && fb->base && fb->pixelformat == SCE_DISPLAY_PIXELFORMAT_A8B8G8R8) {
        fbp = (uint32_t *)fb->base; fbw = fb->width; fbh = fb->height; fbpitch = fb->pitch;
        draw_overlay();
        if (menu_open) draw_menu();
    }
    return TAI_CONTINUE(int, ref_disp, fb, sync);
}

// construit le tableau cats : les categories de cheats.h + "SD codes" juste avant Settings (derniere)
static void cats_init(void) {
    int n = COUNT(cats_orig);
    for (int i = 0; i < n - 1; i++) cats[i] = cats_orig[i];
    cats[n - 1].name = "SD codes"; cats[n - 1].nc = SD_FIX; cats[n - 1].c = sd_items;
    cats[n] = cats_orig[n - 1];
}

// ---------------- entree du module ----------------
// pas de newlib initialise dans un plugin : on fournit ce symbole que libc.a reclame
void _free_vita_newlib(void) {}

void _start() __attribute__((weak, alias("module_start")));

int module_start(SceSize argc, const void *args) {
    cats_init();
    set_theme(0);
    for (int c = 0; c < COUNT(cats); c++) for (int j = 0; j < cats[c].nc; j++) cats[c].c[j].on = 0;   // aucun cheat actif au demarrage
    taiHookFunctionImport(&ref_disp,  TAI_MAIN_MODULE, TAI_ANY_LIBRARY, 0x7A410B64, disp_h);  // sceDisplaySetFrameBuf
    taiHookFunctionImport(&ref_peek,  TAI_MAIN_MODULE, TAI_ANY_LIBRARY, 0xA9C3CED6, peek_h);  // sceCtrlPeekBufferPositive
    taiHookFunctionImport(&ref_read,  TAI_MAIN_MODULE, TAI_ANY_LIBRARY, 0x67E7AB83, read_h);  // sceCtrlReadBufferPositive
    taiHookFunctionImport(&ref_peek2, TAI_MAIN_MODULE, TAI_ANY_LIBRARY, 0x15F81E8C, peek2_h); // sceCtrlPeekBufferPositive2
    taiHookFunctionImport(&ref_read2, TAI_MAIN_MODULE, TAI_ANY_LIBRARY, 0xC4226A3E, read2_h); // sceCtrlReadBufferPositive2
    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) { return SCE_KERNEL_STOP_SUCCESS; }
