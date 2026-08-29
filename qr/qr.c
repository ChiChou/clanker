/*
 * qr - render input as a QR code bitmap in the terminal.
 *
 * Usage:
 *   qr "text"        encode argv[1]
 *   echo text | qr   encode stdin when no argv given
 *
 * Supports iTerm2 inline images and the kitty graphics protocol
 * (kitty, WezTerm, Windows Terminal, ...), auto-detected from the
 * environment; override with QR_PROTOCOL=iterm2|kitty.
 * Builds with CMake on macOS, Linux and Windows (MSVC, clang-cl, MinGW).
 *
 * Self-contained: links only against libc. QR encoding by qrcodegen
 * (Nayuki, MIT). PNG output uses zlib stored (uncompressed) deflate blocks.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define isatty _isatty
#define STDIN_FILENO 0
#else
#include <unistd.h>
#endif

#include "qrcodegen.h"

/* ---------- CRC32 (PNG) ---------- */
static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len) {
    static uint32_t table[256];
    static int init = 0;
    if (!init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = 1;
    }
    crc ^= 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

static uint32_t adler32(const uint8_t *data, size_t len) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < len; i++) {
        a = (a + data[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

/* ---------- growable buffer ---------- */
struct buf {
    uint8_t *data;
    size_t len, cap;
};

static void buf_put(struct buf *b, const void *p, size_t n) {
    if (b->len + n > b->cap) {
        b->cap = (b->len + n) * 2;
        b->data = realloc(b->data, b->cap);
        if (!b->data) { perror("realloc"); exit(1); }
    }
    memcpy(b->data + b->len, p, n);
    b->len += n;
}

static void buf_u32be(struct buf *b, uint32_t v) {
    uint8_t p[4] = { (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v };
    buf_put(b, p, 4);
}

static void png_chunk(struct buf *png, const char type[4], const uint8_t *data, size_t len) {
    buf_u32be(png, (uint32_t)len);
    size_t start = png->len;
    buf_put(png, type, 4);
    if (len) buf_put(png, data, len);
    buf_u32be(png, crc32_update(0, png->data + start, png->len - start));
}

/* Encode 8-bit grayscale pixels (w*h bytes) as a PNG in memory. */
static struct buf write_png(const uint8_t *pix, uint32_t w, uint32_t h) {
    /* raw scanlines: filter byte 0 per row */
    size_t raw_len = (size_t)h * (1 + w);
    uint8_t *raw = malloc(raw_len);
    if (!raw) { perror("malloc"); exit(1); }
    for (uint32_t y = 0; y < h; y++) {
        raw[y * (1 + w)] = 0;
        memcpy(raw + y * (1 + w) + 1, pix + (size_t)y * w, w);
    }

    /* zlib stream: stored deflate blocks (max 65535 each) */
    struct buf z = {0};
    uint8_t zhdr[2] = { 0x78, 0x01 };
    buf_put(&z, zhdr, 2);
    size_t off = 0;
    while (off < raw_len) {
        uint32_t n = (uint32_t)(raw_len - off);
        if (n > 65535) n = 65535;
        int final = (off + n == raw_len);
        uint8_t hdr[5] = { (uint8_t)final, (uint8_t)n, (uint8_t)(n >> 8),
                           (uint8_t)~n, (uint8_t)(~n >> 8) };
        buf_put(&z, hdr, 5);
        buf_put(&z, raw + off, n);
        off += n;
    }
    buf_u32be(&z, adler32(raw, raw_len));
    free(raw);

    struct buf png = {0};
    static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    buf_put(&png, sig, 8);

    uint8_t ihdr[13];
    ihdr[0] = (uint8_t)(w >> 24); ihdr[1] = (uint8_t)(w >> 16);
    ihdr[2] = (uint8_t)(w >> 8);  ihdr[3] = (uint8_t)w;
    ihdr[4] = (uint8_t)(h >> 24); ihdr[5] = (uint8_t)(h >> 16);
    ihdr[6] = (uint8_t)(h >> 8);  ihdr[7] = (uint8_t)h;
    ihdr[8] = 8;   /* bit depth */
    ihdr[9] = 0;   /* color type: grayscale */
    ihdr[10] = 0;  /* compression */
    ihdr[11] = 0;  /* filter */
    ihdr[12] = 0;  /* interlace */
    png_chunk(&png, "IHDR", ihdr, 13);
    png_chunk(&png, "IDAT", z.data, z.len);
    png_chunk(&png, "IEND", NULL, 0);
    free(z.data);
    return png;
}

/* ---------- base64 ---------- */
static const char b64tab[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static struct buf base64_encode(const uint8_t *data, size_t len) {
    struct buf b = {0};
    size_t i = 0;
    for (; i + 3 <= len; i += 3) {
        uint32_t v = (data[i] << 16) | (data[i+1] << 8) | data[i+2];
        char o[4] = { b64tab[v >> 18], b64tab[(v >> 12) & 63],
                      b64tab[(v >> 6) & 63], b64tab[v & 63] };
        buf_put(&b, o, 4);
    }
    if (i < len) {
        uint32_t v = data[i] << 16;
        int two = (i + 1 < len);
        if (two) v |= data[i+1] << 8;
        char o[4] = { b64tab[v >> 18], b64tab[(v >> 12) & 63],
                      two ? b64tab[(v >> 6) & 63] : '=', '=' };
        buf_put(&b, o, 4);
    }
    return b;
}

/* ---------- terminal output protocols ---------- */
enum proto { PROTO_ITERM2, PROTO_KITTY };

static enum proto detect_proto(void) {
    const char *env = getenv("QR_PROTOCOL");   /* explicit override */
    if (env) {
        if (strcmp(env, "kitty") == 0) return PROTO_KITTY;
        if (strcmp(env, "iterm2") == 0) return PROTO_ITERM2;
    }
    if (getenv("KITTY_WINDOW_ID")) return PROTO_KITTY;
    if (getenv("WEZTERM_PANE")) return PROTO_KITTY;  /* wezterm speaks kitty graphics */
    const char *term = getenv("TERM");
    if (term && strstr(term, "kitty")) return PROTO_KITTY;
    const char *prog = getenv("TERM_PROGRAM");
    if (prog && strstr(prog, "iTerm")) return PROTO_ITERM2;
    if (getenv("ITERM_SESSION_ID")) return PROTO_ITERM2;
    if (getenv("WT_SESSION")) return PROTO_KITTY;  /* Windows Terminal */
    return PROTO_KITTY;  /* kitty protocol is the more widely implemented one */
}

/* iTerm2 inline image: OSC 1337 with base64 PNG */
static void emit_iterm2(const struct buf *png, uint32_t px) {
    struct buf b64 = base64_encode(png->data, png->len);
    printf("\033]1337;File=inline=1;width=%upx;height=%upx;preserveAspectRatio=1:", px, px);
    fwrite(b64.data, 1, b64.len, stdout);
    printf("\a\n");
    free(b64.data);
}

/* kitty graphics protocol: APC_G chunks of base64 PNG (f=100) */
static void emit_kitty(const struct buf *png, uint32_t px) {
    struct buf b64 = base64_encode(png->data, png->len);
    const size_t chunk = 4096;
    size_t off = 0;
    int first = 1;
    while (off < b64.len || first) {
        size_t n = b64.len - off;
        if (n > chunk) n = chunk;
        int more = (off + n < b64.len);
        if (first)
            printf("\033_Ga=T,f=100,s=%u,v=%u,m=%d;", px, px, more);
        else
            printf("\033_Gm=%d;", more);
        fwrite(b64.data + off, 1, n, stdout);
        printf("\033\\");
        off += n;
        first = 0;
    }
    putchar('\n');
    free(b64.data);
}

/* ---------- input ---------- */
static char *read_stdin(size_t *out_len) {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    struct buf b = {0};
    char tmp[4096];
    size_t n;
    while ((n = fread(tmp, 1, sizeof tmp, stdin)) > 0)
        buf_put(&b, tmp, n);
    /* trim a single trailing newline, like `echo foo | qr` produces */
    while (b.len > 0 && (b.data[b.len-1] == '\n' || b.data[b.len-1] == '\r'))
        b.len--;
    *out_len = b.len;
    return (char *)b.data;
}

int main(int argc, char **argv) {
#ifdef _WIN32
    /* keep CRLF translation from mangling piped input and escape output */
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    const char *text;
    size_t len;
    if (argc > 1) {
        text = argv[1];
        len = strlen(text);
    } else if (!isatty(STDIN_FILENO)) {
        text = read_stdin(&len);
    } else {
        fprintf(stderr, "usage: qr <text>   or   ... | qr\n");
        return 2;
    }
    if (len == 0) {
        fprintf(stderr, "qr: empty input\n");
        return 2;
    }
    if (len > qrcodegen_BUFFER_LEN_MAX) {
        fprintf(stderr, "qr: input too long\n");
        return 2;
    }

    uint8_t qr[qrcodegen_BUFFER_LEN_MAX];
    uint8_t tmp[qrcodegen_BUFFER_LEN_MAX];
    memcpy(tmp, text, len);  /* encodeBinary takes the payload in dataAndTemp */
    if (!qrcodegen_encodeBinary(tmp, len, qr, qrcodegen_Ecc_MEDIUM,
                                qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
                                qrcodegen_Mask_AUTO, true)) {
        fprintf(stderr, "qr: failed to encode input\n");
        return 1;
    }

    int size = qrcodegen_getSize(qr);
    const int border = 4, scale = 8;
    int dim = (size + 2 * border);
    uint32_t px = (uint32_t)dim * scale;

    uint8_t *pix = malloc((size_t)px * px);
    if (!pix) { perror("malloc"); return 1; }
    memset(pix, 0xFF, (size_t)px * px); /* white */
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
            if (qrcodegen_getModule(qr, x, y))
                for (int dy = 0; dy < scale; dy++)
                    memset(pix + ((size_t)(y + border) * scale + dy) * px
                               + (size_t)(x + border) * scale, 0x00, scale);

    struct buf png = write_png(pix, px, px);
    free(pix);

    if (detect_proto() == PROTO_KITTY)
        emit_kitty(&png, px);
    else
        emit_iterm2(&png, px);
    free(png.data);
    return 0;
}
