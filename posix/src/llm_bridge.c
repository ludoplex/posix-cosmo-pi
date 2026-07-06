/* llm_bridge.c — zero-dependency LLM API bridge (Cosmopolitan APE).
 *
 *   Build:  cosmocc -O2 -o bin/llm_bridge.com src/llm_bridge.c
 *
 * Reads a JSON *messages array* (Anthropic Messages shape) from a file argument
 * or stdin, wraps it into a full request using environment configuration, and
 * POSTs it to the provider over HTTPS. The raw provider response is written to
 * stdout for the shell loop to parse with jsonx.
 *
 *   llm_bridge.com [messages.json]      # or pipe the array on stdin
 *
 * Environment
 * -----------
 *   ANTHROPIC_API_KEY    provider key (sent as x-api-key)     [required unless
 *   ANTHROPIC_AUTH_TOKEN  OAuth token (sent as Bearer)         one is set]
 *   PI_MODEL             model id            (claude-opus-4-8)
 *   PI_MAX_TOKENS        response cap        (8192)   [override PI_MAX_TOKENS]
 *   PI_EFFORT            adaptive effort     (high)
 *   PI_BASE_URL          endpoint host       (https://api.anthropic.com)
 *   PI_SYSTEM            path to a system-prompt text file (optional)
 *   PI_TOOLS             path to a tools JSON array file (optional)
 *   PI_HTTP              HTTPS client        (curl)   [point at bundled curl.com]
 *
 * TLS is delegated to `curl`, invoked via a config file so the API key never
 * appears in argv or the process listing. Swapping PI_HTTP to a bundled
 * `curl.com` APE preserves the zero-dependency property.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *getenv_def(const char *k, const char *d) {
    char *v = getenv(k);
    return (v && *v) ? v : (char *)d;
}

static char *slurp_path(const char *path, size_t *outlen) {
    FILE *f = path ? fopen(path, "rb") : stdin;
    if (!f) return NULL;
    size_t cap = 1 << 16, len = 0;
    char *b = malloc(cap);
    if (!b) { if (path) fclose(f); return NULL; }
    size_t r;
    while ((r = fread(b + len, 1, cap - len, f)) > 0) {
        len += r;
        if (len == cap) {
            cap *= 2;
            char *nb = realloc(b, cap);
            if (!nb) { free(b); if (path) fclose(f); return NULL; }
            b = nb;
        }
    }
    if (path) fclose(f);
    char *nb = realloc(b, len + 1);
    if (nb) b = nb;
    b[len] = 0;
    if (outlen) *outlen = len;
    return b;
}

/* Write `s` as a JSON string literal (quoted, escaped) to `out`. */
static void json_escape(FILE *out, const char *s, size_t n) {
    fputc('"', out);
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n", out);  break;
            case '\r': fputs("\\r", out);  break;
            case '\t': fputs("\\t", out);  break;
            case '\b': fputs("\\b", out);  break;
            case '\f': fputs("\\f", out);  break;
            default:
                if (c < 0x20) fprintf(out, "\\u%04x", c);
                else fputc(c, out);
        }
    }
    fputc('"', out);
}

static int make_temp(char *tmpl) {
    int fd = mkstemp(tmpl);
    if (fd < 0) return -1;
    fchmod(fd, 0600);
    return fd;
}

int main(int argc, char **argv) {
    const char *msgs_path = (argc > 1) ? argv[1] : NULL;

    size_t mlen;
    char *msgs = slurp_path(msgs_path, &mlen);
    if (!msgs) { fprintf(stderr, "llm_bridge: cannot read messages\n"); return 2; }

    const char *model    = getenv_def("PI_MODEL", "claude-opus-4-8");
    const char *maxtok   = getenv_def("PI_MAX_TOKENS", "8192");
    const char *effort   = getenv_def("PI_EFFORT", "high");
    const char *base     = getenv_def("PI_BASE_URL", "https://api.anthropic.com");
    const char *http     = getenv_def("PI_HTTP", "curl");
    const char *api_key  = getenv("ANTHROPIC_API_KEY");
    const char *oauth    = getenv("ANTHROPIC_AUTH_TOKEN");
    const char *sys_path = getenv("PI_SYSTEM");
    const char *tools_path = getenv("PI_TOOLS");

    if ((!api_key || !*api_key) && (!oauth || !*oauth)) {
        fprintf(stderr, "llm_bridge: set ANTHROPIC_API_KEY or ANTHROPIC_AUTH_TOKEN\n");
        return 2;
    }

    /* ---- build request body to a temp file ---- */
    char body_tmpl[] = "/tmp/pi_body.XXXXXX";
    char cfg_tmpl[]  = "/tmp/pi_cfg.XXXXXX";
    int bfd = make_temp(body_tmpl);
    if (bfd < 0) { perror("mkstemp body"); return 2; }
    FILE *bf = fdopen(bfd, "w");
    if (!bf) { perror("fdopen"); return 2; }

    fprintf(bf, "{\"model\":");
    json_escape(bf, model, strlen(model));
    fprintf(bf, ",\"max_tokens\":%s", maxtok);
    fprintf(bf, ",\"thinking\":{\"type\":\"adaptive\"}");
    fprintf(bf, ",\"output_config\":{\"effort\":");
    json_escape(bf, effort, strlen(effort));
    fprintf(bf, "}");

    if (sys_path && *sys_path) {
        size_t slen;
        char *sys = slurp_path(sys_path, &slen);
        if (sys && slen) {
            fprintf(bf, ",\"system\":");
            json_escape(bf, sys, slen);
        }
        free(sys);
    }
    if (tools_path && *tools_path) {
        size_t tlen;
        char *tools = slurp_path(tools_path, &tlen);
        if (tools && tlen) fprintf(bf, ",\"tools\":%s", tools);
        free(tools);
    }

    fprintf(bf, ",\"messages\":%s}", msgs);
    fclose(bf);
    free(msgs);

    /* ---- build curl config to a temp file (keeps secrets off argv) ---- */
    int cfd = make_temp(cfg_tmpl);
    if (cfd < 0) { perror("mkstemp cfg"); unlink(body_tmpl); return 2; }
    FILE *cf = fdopen(cfd, "w");
    if (!cf) { perror("fdopen"); unlink(body_tmpl); return 2; }

    fprintf(cf, "url = \"%s/v1/messages\"\n", base);
    fprintf(cf, "request = \"POST\"\n");
    fprintf(cf, "header = \"content-type: application/json\"\n");
    fprintf(cf, "header = \"anthropic-version: 2023-06-01\"\n");
    if (api_key && *api_key) {
        fprintf(cf, "header = \"x-api-key: %s\"\n", api_key);
    } else {
        fprintf(cf, "header = \"authorization: Bearer %s\"\n", oauth);
        fprintf(cf, "header = \"anthropic-beta: oauth-2025-04-20\"\n");
    }
    fprintf(cf, "data = \"@%s\"\n", body_tmpl);
    fprintf(cf, "silent\nshow-error\n");
    fclose(cf);

    /* ---- run the HTTPS client, stream its stdout to ours ---- */
    char cmd[1024];
    snprintf(cmd, sizeof cmd, "%s -K %s", http, cfg_tmpl);
    FILE *p = popen(cmd, "r");
    int rc = 2;
    if (p) {
        char buf[8192];
        size_t r;
        while ((r = fread(buf, 1, sizeof buf, p)) > 0) fwrite(buf, 1, r, stdout);
        int status = pclose(p);
        rc = (status == 0) ? 0 : 1;
    } else {
        fprintf(stderr, "llm_bridge: failed to launch '%s'\n", http);
    }

    unlink(body_tmpl);
    unlink(cfg_tmpl);
    return rc;
}
