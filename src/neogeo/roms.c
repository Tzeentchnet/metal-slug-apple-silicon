/* Loading of the user's copy ("bring your own ROM").
 *
 * `path` can be a folder (searched up to a few levels deep) or a zip. A MAME set (mslug +
 * neogeo) is accepted as zips or loose files. Each file is recognized by its CRC-32 and
 * confirmed by SHA-1, whatever its name; graphics are normalized to the runtime's unpacked
 * layout.
 *
 * The repo only contains hashes, never data. */
#include "neogeo.h"
#include "hashes.h"
#include "sha256.h"
#include "unzip.h"

#include <ctype.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MB (1u << 20)

/* ------------------------------------------------------------------ catalog */
enum { R_P1, R_S1, R_M1, R_V1, R_V2, R_C1, R_C2, R_C3, R_C4, R_SFIX, R_LO, R_BIOS };
#define N_ROLES (R_BIOS + 1)

typedef struct {
    int role;
    const char *name;        /* MAME's name (for messages; files are matched by hash) */
    uint32_t size;
    uint32_t crc;
    const char *sha1;
    const char *label;       /* P1: the version; BIOS: MAME's -bios name */
} MameFile;

/* From `mame -listxml mslug`. The verified US MVS BIOS is preferred. The 512 KB MV1C BIOSes are
   not supported. */
static const MameFile MAME[] = {
    {R_P1, "201-p1.p1", 2097152, 0x08d8daa5, "b888993dbb7e9f0a28a01d7d2e1da00ef9cf6f38", "arcade"},
    {R_S1, "201-s1.s1", 131072, 0x2f55958d, "550b53628daec9f1e1e11a398854092d90f9505a", NULL},
    {R_M1, "201-m1.m1", 131072, 0xc28b3253, "fd75bd15aed30266a8b3775f276f997af57d1c06", NULL},
    {R_V1, "201-v1.v1", 4194304, 0x23d22ed1, "cd076928468ad6bcc5f19f88cb843ecb5e660681", NULL},
    {R_V2, "201-v2.v2", 4194304, 0x472cf9db, "5f79ea9286d22ed208128f9c31ca75552ce08b57", NULL},
    {R_C1, "201-c1.c1", 4194304, 0x72813676, "7b045d1a48980cb1a140699011cb1a3d4acdc4d1", NULL},
    {R_C2, "201-c2.c2", 4194304, 0x96f62574, "cb7254b885989223bba597b8ff0972dfa5957816", NULL},
    {R_C3, "201-c3.c3", 4194304, 0x5121456a, "0a7a27d603d1bb2520b5570ebf5b34a106e255a6", NULL},
    {R_C4, "201-c4.c4", 4194304, 0xf4ad59a3, "4e94fda8ee63abf0f92afe08060a488546e5c280", NULL},
    {R_SFIX, "sfix.sfix", 131072, 0xc2ea0cfd, "fd4a618cdcdbf849374f0a50dd8efe9dbab706c3", NULL},
    {R_LO, "000-lo.lo", 131072, 0x5a86cff2, "5992277debadeb64d1c1c64b0a92d9293eaf7e4a", NULL},
    {R_BIOS, "sp-u2.sp1", 131072, 0xe72943de, "5c6bba07d2ec8ac95776aa3511109f5e1e2e92eb", "us"},
    {R_BIOS, "sp-e.sp1", 131072, 0x2723a5b5, "5dbff7531cf04886cde3ef022fb5ca687573dcb8", "us-e"},
    {R_BIOS, "sp1-u2", 131072, 0x62f021f4, "62d372269e1b3161c64ae21123655a0a22ffd1bb", "us-v2"},
    {R_BIOS, "sp1-u4.bin", 131072, 0x1179a30f, "866817f47aa84d903d0b819d61f6ef356893d16a", "us-u4"},
    {R_BIOS, "sp1-u3.bin", 131072, 0x2025b7a2, "73d774746196f377111cd7aa051cc8bb5dd948b3", "us-u3"},
    {R_BIOS, "sp-s2.sp1", 131072, 0x9036d879, "4f5ed7105b7128794654ce82b51723e16e389543", "euro"},
    {R_BIOS, "sp-s.sp1", 131072, 0xc7f2fa45, "09576ff20b4d6b365e78e6a5698ea450262697cd", "euro-s1"},
    {R_BIOS, "sp-s3.sp1", 131072, 0x91b64be3, "720a3e20d26818632aedf2c2fd16c54f213543e1", "asia-mv1b"},
    {R_BIOS, "vs-bios.rom", 131072, 0xf0e8f27d, "ecf01eda815909f1facec62abf3594eaa8d11075", "japan"},
    {R_BIOS, "sp-j2.sp1", 131072, 0xacede59c, "b6f97acd282fd7e94d9426078a90f059b5e9dd91", "japan-s2"},
    {R_BIOS, "sp1.jipan.1024", 131072, 0x9fb0abe4, "18a987ce2229df79a8cf6a84f968f0e42ce4e59d", "japan-s1"},
    {R_BIOS, "japan-j3.bin", 131072, 0xdff6d41f, "e92910e20092577a4523a6b39d578a71d4de7085", "japan-mv1b"},
    {R_BIOS, "sp1-j3.bin", 131072, 0xfbc6d469, "46b2b409b5b68869e367b40c846373623edb632a", "japan-j3a"},
    {R_BIOS, "sp-1v1_3db8c.bin", 131072, 0x162f0ebe, "fe1c6dd3dfcf97d960065b1bb46c1e11cb7bf271", "japan-hotel"},
    {R_BIOS, "uni-bios_4_0.rom", 131072, 0xa7aab458, "938a0bda7d9a357240718c2cec319878d36b8f72", "unibios40"},
    {R_BIOS, "uni-bios_3_3.rom", 131072, 0x24858466, "0ad92efb0c2338426635e0159d1f60b4473d0785", "unibios33"},
    {R_BIOS, "uni-bios_3_2.rom", 131072, 0xa4e8b9b3, "c92f18c3f1edda543d264ecd0ea915240e7c8258", "unibios32"},
    {R_BIOS, "uni-bios_3_1.rom", 131072, 0x0c58093f, "29329a3448c2505e1ff45ffa75e61e9693165153", "unibios31"},
    {R_BIOS, "uni-bios_3_0.rom", 131072, 0xa97c89a9, "97a5eff3b119062f10e31ad6f04fe4b90d366e7f", "unibios30"},
    {R_BIOS, "uni-bios_2_3.rom", 131072, 0x27664eb5, "5b02900a3ccf3df168bdcfc98458136fd2b92ac0", "unibios23"},
    {R_BIOS, "uni-bios_2_3o.rom", 131072, 0x601720ae, "1b8a72c720cdb5ee3f1d735bbcf447b09204b8d9", "unibios23o"},
    {R_BIOS, "uni-bios_2_2.rom", 131072, 0x2d50996a, "5241a4fb0c63b1a23fd1da8efa9c9a9bd3b4279c", "unibios22"},
    {R_BIOS, "uni-bios_2_1.rom", 131072, 0x8dabf76b, "c23732c4491d966cf0373c65c83c7a4e88f0082c", "unibios21"},
    {R_BIOS, "uni-bios_2_0.rom", 131072, 0x0c12c2ad, "37bcd4d30f3892078b46841d895a6eff16dc921e", "unibios20"},
    {R_BIOS, "uni-bios_1_3.rom", 131072, 0xb24b44a0, "eca8851d30557b97c309a0d9f4a9d20e5b14af4e", "unibios13"},
    {R_BIOS, "uni-bios_1_2.rom", 131072, 0x4fa698e9, "682e13ec1c42beaa2d04473967840c88fd52c75a", "unibios12"},
    {R_BIOS, "uni-bios_1_2o.rom", 131072, 0xe19d3ce9, "af88ef837f44a3af2d7144bb46a37c8512b67770", "unibios12o"},
    {R_BIOS, "uni-bios_1_1.rom", 131072, 0x5dda0d84, "4153d533c02926a2577e49c32657214781ff29b7", "unibios11"},
    {R_BIOS, "uni-bios_1_0.rom", 131072, 0x0ce453a0, "3b4c0cd26c176fc6b26c3a2f95143dd478f6abf9", "unibios10"},
};
#define N_MAME (int)(sizeof MAME / sizeof MAME[0])

/* ------------------------------------------------------------------ candidates */
typedef struct {
    char path[1024];         /* loose file or zip */
    char group[1300];        /* folder of the file (zip path + inner folder for members) */
    char base[256];          /* lowercase file name */
    int in_zip;
    ZipEntry entry;
    uint32_t size;
    uint32_t crc;
    int have_crc;
} Candidate;

typedef struct {
    Candidate *c;
    int n, cap;
    int visited;
} Candidates;

#define MAX_DEPTH 4
#define MAX_VISITED 20000

static void lower_base(const char *path, char *out, size_t size)
{
    const char *b = path;
    for (const char *p = path; *p; p++) if (*p == '/' || *p == '\\') b = p + 1;
    size_t i = 0;
    for (; b[i] && i + 1 < size; i++) out[i] = (char)tolower((unsigned char)b[i]);
    out[i] = 0;
}

static int interesting(const char *base, uint32_t size)
{
    if (!strncmp(base, "mslug_", 7)) return 1;
    return size == 131072 || size == MB || size == 2 * MB || size == 4 * MB || size == 8 * MB;
}

static Candidate *add(Candidates *cs)
{
    if (cs->n == cs->cap) {
        int cap = cs->cap ? cs->cap * 2 : 64;
        Candidate *c = realloc(cs->c, (size_t)cap * sizeof *c);
        if (!c) return NULL;
        cs->c = c;
        cs->cap = cap;
    }
    Candidate *c = &cs->c[cs->n++];
    memset(c, 0, sizeof *c);
    return c;
}

static int ends_with(const char *s, const char *suffix)
{
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && !strcmp(s + a - b, suffix);
}

static void add_zip(Candidates *cs, const char *path)
{
    ZipEntry *e;
    int n = zip_list(path, &e);
    for (int i = 0; i < n; i++) {
        char base[256];
        lower_base(e[i].name, base, sizeof base);
        if (!interesting(base, e[i].size)) continue;
        Candidate *c = add(cs);
        if (!c) break;
        snprintf(c->path, sizeof c->path, "%s", path);
        snprintf(c->group, sizeof c->group, "%s|%.*s", path,
                 (int)(strlen(e[i].name) - strlen(base)), e[i].name);
        snprintf(c->base, sizeof c->base, "%s", base);
        c->in_zip = 1;
        c->entry = e[i];
        c->size = e[i].size;
        c->crc = e[i].crc;
        c->have_crc = 1;
    }
    free(e);
}

static void walk(Candidates *cs, const char *path, int depth)
{
    struct stat st;
    if (cs->visited++ > MAX_VISITED || stat(path, &st) != 0) return;
    if (S_ISDIR(st.st_mode)) {
        if (depth > MAX_DEPTH) return;
        DIR *d = opendir(path);
        if (!d) return;
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char sub[1024];
            size_t n = strlen(path);
            int slash = n && (path[n - 1] == '/' || path[n - 1] == '\\');
            snprintf(sub, sizeof sub, "%s%s%s", path, slash ? "" : "/", e->d_name);
            walk(cs, sub, depth + 1);
        }
        closedir(d);
        return;
    }
    char base[256];
    lower_base(path, base, sizeof base);
    if (ends_with(base, ".zip")) { add_zip(cs, path); return; }
    if (st.st_size > 64 * (long long)MB || !interesting(base, (uint32_t)st.st_size)) return;
    Candidate *c = add(cs);
    if (!c) return;
    snprintf(c->path, sizeof c->path, "%s", path);
    snprintf(c->group, sizeof c->group, "%.*s", (int)(strlen(path) - strlen(base)), path);
    snprintf(c->base, sizeof c->base, "%s", base);
    c->size = (uint32_t)st.st_size;
}

static uint8_t *read_candidate(const Candidate *c)
{
    if (c->in_zip) return zip_read(c->path, &c->entry);
    FILE *f = fopen(c->path, "rb");
    if (!f) return NULL;
    uint8_t *d = malloc(c->size ? c->size : 1);
    if (d && fread(d, 1, c->size, f) != c->size) { free(d); d = NULL; }
    fclose(f);
    return d;
}

static void candidate_crc(Candidate *c)
{
    if (c->have_crc) return;
    uint8_t *d = read_candidate(c);
    if (d) { c->crc = crc32_update(0, d, c->size); c->have_crc = 1; free(d); }
}

/* ------------------------------------------------------------------ helpers */
static void set_error(char *error, int size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(error, (size_t)size, fmt, ap);
    va_end(ap);
}

static void swap_bytes(uint8_t *d, uint32_t n)
{
    for (uint32_t i = 0; i < n; i += 2) { uint8_t t = d[i]; d[i] = d[i + 1]; d[i + 1] = t; }
}

/* Neo Geo S format (per 32-byte tile: 4 column groups of 8 bytes, one byte per row; groups in
   pixel order 4-5, 6-7, 0-1, 2-3) to the runtime's 4 bpp row layout. */
static void fix_to_runtime(const uint8_t *s, uint8_t *out, uint32_t size)
{
    static const int GROUP_OF_PAIR[4] = {2, 3, 0, 1};
    for (uint32_t t = 0; t < size; t += 32)
        for (int row = 0; row < 8; row++)
            for (int k = 0; k < 4; k++) out[t + row * 4 + k] = s[t + GROUP_OF_PAIR[k] * 8 + row];
}

/* A pair of C ROMs (odd: planes 0-1, even: planes 2-3, interleaved byte by byte) to runtime tiles
   (16x16, 4 bpp rows, 8 bytes per row, low nibble left). Per 128-byte Neo Geo tile: right half
   (x 8-15) at 0x00, left half at 0x40; 4 bytes per row: plane 0, plane 2, plane 1, plane 3;
   pixel x of the half is bit 7 - x. */
static void sprites_to_runtime(const uint8_t *odd, const uint8_t *even, uint32_t size, uint8_t *out)
{
    static const int PLANE_OFFSET[4] = {0, 2, 1, 3};          /* plane -> byte in the row */
    for (uint32_t t = 0; t < size * 2 / 128; t++) {
        uint8_t region[128];
        for (int i = 0; i < 64; i++) {
            region[2 * i] = odd[t * 64 + i];
            region[2 * i + 1] = even[t * 64 + i];
        }
        uint8_t *o = out + (size_t)t * 128;
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                const uint8_t *r = region + (x < 8 ? 0x40 : 0x00) + y * 4;
                int bit = 7 - (x & 7), px = 0;
                for (int p = 0; p < 4; p++) px |= ((r[PLANE_OFFSET[p]] >> bit) & 1) << p;
                if (x & 1) o[y * 8 + x / 2] |= (uint8_t)(px << 4);
                else o[y * 8 + x / 2] = (uint8_t)px;
            }
        }
    }
}

static void describe(NgRoms *r, int verified)
{
    NgRomInfo *i = &r->info;
    i->format = NG_FORMAT_MAME;
    i->verified = verified;
    sha256(r->p, MB, i->p1_sha256);
    sha256(r->bios, 131072, i->bios_sha256);
    snprintf(i->description, sizeof i->description, "MAME set, %s, BIOS %s%s",
             i->version == NG_VERSION_ARCADE ? "arcade program" : "unknown program",
             i->bios, verified ? "" : " (unverified)");
}

/* ------------------------------------------------------------------ MAME */
static uint8_t *read_mame(Candidates *cs, const MameFile *m)
{
    for (int i = 0; i < cs->n; i++) {
        Candidate *c = &cs->c[i];
        if (c->size != m->size) continue;
        candidate_crc(c);
        if (!c->have_crc || c->crc != m->crc) continue;
        uint8_t *d = read_candidate(c);
        if (!d) continue;
        uint8_t h[20];
        char hex[41];
        sha1(d, m->size, h);
        hex_string(h, 20, hex);
        if (!strcmp(hex, m->sha1)) return d;
        free(d);
    }
    return NULL;
}

static int load_mame(NgRoms *r, Candidates *cs, char *error, int size)
{
    uint8_t *d[N_ROLES] = {0};
    const MameFile *chosen[N_ROLES] = {0};
    int verified = 1;
    for (int k = 0; k < N_MAME; k++) {
        int role = MAME[k].role;
        if (!d[role] && (d[role] = read_mame(cs, &MAME[k])) != NULL) chosen[role] = &MAME[k];
    }
    if (!d[R_P1]) {                     /* another program version: recognized by its name */
        for (int i = 0; i < cs->n && !d[R_P1]; i++)
            if (!strcmp(cs->c[i].base, "201-p1.p1") && cs->c[i].size == 2 * MB)
                d[R_P1] = read_candidate(&cs->c[i]);
        if (d[R_P1]) verified = 0;
    }
    char missing[600] = "";
    int found = 0;
    for (int role = 0; role < N_ROLES; role++) {
        if (d[role]) { found++; continue; }
        const char *name = role == R_BIOS ? "an MVS BIOS (neogeo.zip)" : NULL;
        for (int k = 0; k < N_MAME && !name; k++) if (MAME[k].role == role) name = MAME[k].name;
        size_t n = strlen(missing);
        snprintf(missing + n, sizeof missing - n, "%s%s", n ? ", " : "", name);
    }
    if (missing[0]) {
        if (found > 2) set_error(error, size, "Incomplete MAME set: missing %s", missing);
        for (int role = 0; role < N_ROLES; role++) free(d[role]);
        return -1;
    }
    r->p_size = 2 * MB;
    r->p = malloc(r->p_size);
    r->fix_game = malloc(131072);
    r->fix_bios = malloc(131072);
    r->n_tiles = 16 * MB / 128;
    r->tiles = malloc(16 * MB);
    r->adpcm_size = 8 * MB;
    r->adpcm = malloc(r->adpcm_size);
    r->bios = d[R_BIOS];
    r->zoom = d[R_LO];
    r->z80 = d[R_M1];
    if (!r->p || !r->fix_game || !r->fix_bios || !r->tiles || !r->adpcm) {
        set_error(error, size, "Out of memory");
        for (int role = 0; role < N_ROLES; role++)
            if (role != R_BIOS && role != R_LO && role != R_M1) free(d[role]);
        ng_roms_free(r);
        return -1;
    }
    /* MAME loads the second file half at CPU address 0 and the first half at 0x100000. */
    memcpy(r->p, d[R_P1] + MB, MB);
    memcpy(r->p + MB, d[R_P1], MB);
    fix_to_runtime(d[R_S1], r->fix_game, 131072);
    fix_to_runtime(d[R_SFIX], r->fix_bios, 131072);
    for (int k = 0; k < 2; k++)
        sprites_to_runtime(d[R_C1 + 2 * k], d[R_C2 + 2 * k], 4 * MB,
                           r->tiles + (size_t)k * 8 * MB);
    memcpy(r->adpcm, d[R_V1], 4 * MB);
    memcpy(r->adpcm + 4 * MB, d[R_V2], 4 * MB);
    for (int role = 0; role < N_ROLES; role++)
        if (role != R_BIOS && role != R_LO && role != R_M1) free(d[role]);
    if (chosen[R_P1] && !strcmp(chosen[R_P1]->label, "arcade")) r->info.version = NG_VERSION_ARCADE;
    snprintf(r->info.bios, sizeof r->info.bios, "%s", chosen[R_BIOS]->label);
    describe(r, verified);
    return 0;
}

/* ------------------------------------------------------------------ entry point */
int ng_roms_load(NgRoms *r, const char *path, char *error, int error_size)
{
    memset(r, 0, sizeof *r);
    Candidates cs = {0};
    walk(&cs, path, 0);
    char base[256];
    lower_base(path, base, sizeof base);
    if (ends_with(base, ".zip") && strcmp(base, "neogeo.zip") != 0) {
        /* Just the game's zip (from a MAME roms folder): the BIOS is in neogeo.zip next to it. */
        char bios[1100];
        snprintf(bios, sizeof bios, "%.*sneogeo.zip", (int)(strlen(path) - strlen(base)), path);
        add_zip(&cs, bios);
    }
    int result = -1;
    set_error(error, error_size, "No Metal Slug files found in %s", path);
    result = load_mame(r, &cs, error, error_size);
    free(cs.c);
    if (result != 0) return -1;
    /* P and BIOS: the files keep MAME's .p byte order; the 68000 view is the swapped one. */
    swap_bytes(r->p, r->p_size);
    swap_bytes(r->bios, 131072);
    snprintf(r->info.source, sizeof r->info.source, "%s", path);
    return 0;
}

void ng_roms_free(NgRoms *r)
{
    free(r->p); free(r->bios); free(r->fix_game); free(r->fix_bios); free(r->tiles); free(r->zoom);
    free(r->z80); free(r->adpcm);
    memset(r, 0, sizeof *r);
}
