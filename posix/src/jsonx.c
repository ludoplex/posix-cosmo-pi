/* jsonx.c — minimal, dependency-free JSON accessor for the hollowed pi harness.
 *
 *   Build:  cosmocc -O2 -o bin/jsonx.com src/jsonx.c
 *
 * This is the deterministic JSON boundary the harness uses instead of jq (an
 * external dependency) or grep/sed (unsafe: JSON strings may contain }, ", and
 * newlines that break regex parsing). One compiled APE, identical on every OS.
 *
 * Subcommands
 * -----------
 *   jsonx get <path>   Read JSON from stdin; print the value at <path>.
 *                      Path is dot-separated. Object keys and array indices:
 *                        tool            args.command       content.0.text
 *                      Strings print decoded (no surrounding quotes); numbers,
 *                      true/false/null print their literal source; objects and
 *                      arrays print their raw JSON source slice.
 *                      Exit 0 = found, 3 = not found, 2 = parse error.
 *
 *   jsonx str          Read raw bytes from stdin; print a JSON string literal
 *                      (quoted, fully escaped). Build JSON from shell without
 *                      quoting bugs:  printf '%s' "$x" | jsonx str
 *
 *   jsonx validate     Read JSON from stdin; exit 0 if it is exactly one
 *                      well-formed value (trailing whitespace allowed), else 1.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char  *g;   /* input buffer  */
static size_t gn;  /* input length  */

static char *slurp(FILE *f, size_t *outlen) {
    size_t cap = 1 << 16, len = 0;
    char *b = malloc(cap);
    if (!b) return NULL;
    size_t r;
    while ((r = fread(b + len, 1, cap - len, f)) > 0) {
        len += r;
        if (len == cap) {
            cap *= 2;
            char *nb = realloc(b, cap);
            if (!nb) { free(b); return NULL; }
            b = nb;
        }
    }
    char *nb = realloc(b, len + 1);
    if (nb) b = nb;
    b[len] = 0;
    *outlen = len;
    return b;
}

static void skipws(size_t *i) {
    while (*i < gn) {
        char c = g[*i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') (*i)++;
        else break;
    }
}

/* g[*i] == '"'; on success *i points just past the closing quote. */
static int scan_string(size_t *i) {
    size_t k = *i + 1;
    while (k < gn) {
        char c = g[k++];
        if (c == '\\') { if (k < gn) k++; }
        else if (c == '"') { *i = k; return 0; }
    }
    return -1; /* unterminated */
}

/* Advance *i past one complete value (object/array/string/number/literal). */
static int skip_value(size_t *i) {
    skipws(i);
    if (*i >= gn) return -1;
    char c = g[*i];
    if (c == '"') return scan_string(i);
    if (c == '{' || c == '[') {
        char close = (c == '{') ? '}' : ']';
        int is_obj = (c == '{');
        (*i)++;
        for (;;) {
            skipws(i);
            if (*i >= gn) return -1;
            if (g[*i] == close) { (*i)++; return 0; }
            if (is_obj) {
                if (g[*i] != '"') return -1;
                if (scan_string(i)) return -1;
                skipws(i);
                if (*i >= gn || g[*i] != ':') return -1;
                (*i)++;
            }
            if (skip_value(i)) return -1;
            skipws(i);
            if (*i >= gn) return -1;
            if (g[*i] == ',') { (*i)++; continue; }
            if (g[*i] == close) { (*i)++; return 0; }
            return -1;
        }
    }
    /* scalar: number / true / false / null */
    while (*i < gn) {
        char d = g[*i];
        if (d == ',' || d == '}' || d == ']' ||
            d == ' ' || d == '\t' || d == '\n' || d == '\r') break;
        (*i)++;
    }
    return 0;
}

static void emit_utf8(unsigned cp, FILE *out) {
    if (cp < 0x80) fputc(cp, out);
    else if (cp < 0x800) {
        fputc(0xC0 | (cp >> 6), out);
        fputc(0x80 | (cp & 0x3F), out);
    } else if (cp < 0x10000) {
        fputc(0xE0 | (cp >> 12), out);
        fputc(0x80 | ((cp >> 6) & 0x3F), out);
        fputc(0x80 | (cp & 0x3F), out);
    } else {
        fputc(0xF0 | (cp >> 18), out);
        fputc(0x80 | ((cp >> 12) & 0x3F), out);
        fputc(0x80 | ((cp >> 6) & 0x3F), out);
        fputc(0x80 | (cp & 0x3F), out);
    }
}

static unsigned hex4(const char *p) {
    unsigned v = 0;
    for (int k = 0; k < 4; k++) {
        char c = p[k];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= c - '0';
        else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
        else return 0xFFFFFFFFu;
    }
    return v;
}

/* Decode the JSON string that begins at g[s] ('"') and write bytes to out. */
static void decode_string(size_t s, FILE *out) {
    size_t k = s + 1;
    while (k < gn) {
        char c = g[k++];
        if (c == '"') break;
        if (c == '\\' && k < gn) {
            char e = g[k++];
            switch (e) {
                case 'n': fputc('\n', out); break;
                case 't': fputc('\t', out); break;
                case 'r': fputc('\r', out); break;
                case 'b': fputc('\b', out); break;
                case 'f': fputc('\f', out); break;
                case '/': fputc('/', out);  break;
                case '\\': fputc('\\', out); break;
                case '"': fputc('"', out);  break;
                case 'u': {
                    if (k + 4 <= gn) {
                        unsigned cp = hex4(g + k);
                        k += 4;
                        if (cp >= 0xD800 && cp <= 0xDBFF && k + 6 <= gn &&
                            g[k] == '\\' && g[k + 1] == 'u') {
                            unsigned lo = hex4(g + k + 2);
                            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                                k += 6;
                            }
                        }
                        if (cp != 0xFFFFFFFFu) emit_utf8(cp, out);
                    }
                    break;
                }
                default: fputc(e, out);
            }
        } else {
            fputc(c, out);
        }
    }
}

/* Compare the decoded JSON key at g[s] ('"') to the ASCII segment `seg`. */
static int keymatch(size_t s, const char *seg) {
    size_t k = s + 1;
    const char *p = seg;
    while (k < gn) {
        char c = g[k++];
        if (c == '"') return *p == 0;
        unsigned char out;
        if (c == '\\' && k < gn) {
            char e = g[k++];
            switch (e) {
                case 'n': out = '\n'; break;
                case 't': out = '\t'; break;
                case 'r': out = '\r'; break;
                case 'b': out = '\b'; break;
                case 'f': out = '\f'; break;
                case '/': out = '/';  break;
                case '\\': out = '\\'; break;
                case '"': out = '"';  break;
                case 'u': {
                    if (k + 4 <= gn) {
                        unsigned cp = hex4(g + k);
                        k += 4;
                        if (cp < 0x80) out = (unsigned char)cp;
                        else return 0; /* non-ASCII key: treat as non-match */
                    } else return 0;
                    break;
                }
                default: out = (unsigned char)e;
            }
        } else {
            out = (unsigned char)c;
        }
        if (*p == 0 || (unsigned char)*p != out) return 0;
        p++;
    }
    return 0;
}

static void emit_value(size_t i) {
    skipws(&i);
    if (i >= gn) return;
    if (g[i] == '"') { decode_string(i, stdout); return; }
    size_t start = i, j = i;
    skip_value(&j);
    fwrite(g + start, 1, j - start, stdout);
}

/* Descend into the value at *i following segs[depth..nseg). */
static int seek(size_t *i, char **segs, int nseg, int depth) {
    skipws(i);
    if (depth == nseg) { emit_value(*i); return 0; }
    if (*i >= gn) return 3;
    char c = g[*i];
    if (c == '{') {
        (*i)++;
        for (;;) {
            skipws(i);
            if (*i >= gn) return 3;
            if (g[*i] == '}') return 3;
            if (g[*i] != '"') return 2;
            size_t ks = *i;
            if (scan_string(i)) return 2;
            skipws(i);
            if (*i >= gn || g[*i] != ':') return 2;
            (*i)++;
            if (keymatch(ks, segs[depth])) return seek(i, segs, nseg, depth + 1);
            if (skip_value(i)) return 2;
            skipws(i);
            if (*i < gn && g[*i] == ',') { (*i)++; continue; }
            return 3;
        }
    }
    if (c == '[') {
        const char *s = segs[depth];
        if (!*s) return 3;
        long idx = 0;
        for (const char *p = s; *p; p++) {
            if (*p < '0' || *p > '9') return 3;
            idx = idx * 10 + (*p - '0');
        }
        (*i)++;
        long k = 0;
        for (;;) {
            skipws(i);
            if (*i >= gn) return 3;
            if (g[*i] == ']') return 3;
            if (k == idx) return seek(i, segs, nseg, depth + 1);
            if (skip_value(i)) return 2;
            skipws(i);
            if (*i < gn && g[*i] == ',') { (*i)++; k++; continue; }
            return 3;
        }
    }
    return 3; /* path continues into a scalar */
}

static int cmd_str(void) {
    size_t n;
    char *b = slurp(stdin, &n);
    if (!b) return 2;
    putchar('"');
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)b[i];
        switch (c) {
            case '"':  fputs("\\\"", stdout); break;
            case '\\': fputs("\\\\", stdout); break;
            case '\n': fputs("\\n", stdout);  break;
            case '\r': fputs("\\r", stdout);  break;
            case '\t': fputs("\\t", stdout);  break;
            case '\b': fputs("\\b", stdout);  break;
            case '\f': fputs("\\f", stdout);  break;
            default:
                if (c < 0x20) printf("\\u%04x", c);
                else putchar(c);
        }
    }
    putchar('"');
    free(b);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: jsonx get <path> | str | validate\n");
        return 2;
    }
    if (!strcmp(argv[1], "str")) return cmd_str();

    size_t n;
    char *b = slurp(stdin, &n);
    if (!b) return 2;
    g = b;
    gn = n;

    if (!strcmp(argv[1], "validate")) {
        size_t i = 0;
        if (skip_value(&i)) return 1;
        skipws(&i);
        return (i == gn) ? 0 : 1;
    }
    if (!strcmp(argv[1], "get")) {
        if (argc < 3) {
            fprintf(stderr, "usage: jsonx get <path>\n");
            return 2;
        }
        char *copy = strdup(argv[2]);
        if (!copy) return 2;
        int nseg = 1;
        for (char *p = copy; *p; p++) if (*p == '.') nseg++;
        char **segs = calloc(nseg, sizeof(char *));
        if (!segs) return 2;
        int si = 0;
        segs[si++] = copy;
        for (char *p = copy; *p; p++)
            if (*p == '.') { *p = 0; segs[si++] = p + 1; }
        size_t i = 0;
        int r = seek(&i, segs, nseg, 0);
        free(segs);
        free(copy);
        return (r == 0) ? 0 : (r == 3 ? 3 : 2);
    }
    fprintf(stderr, "jsonx: unknown subcommand: %s\n", argv[1]);
    return 2;
}
