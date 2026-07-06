# posix/core/lib.sh — shared helpers. Source this; do not execute it.
# Strict mode is asserted here so every consumer inherits it; all callers in this
# tree already run `set -eu`, so sourcing is a no-op for them.
set -eu
: "${PI_ROOT:?PI_ROOT must be set}"

PI_JSONX="${PI_JSONX:-$PI_ROOT/bin/jsonx.com}"
PI_BRIDGE="${PI_BRIDGE:-$PI_ROOT/bin/llm_bridge.com}"
PI_LOG="${PI_LOG:-/dev/stderr}"
export PI_ROOT PI_JSONX PI_BRIDGE PI_LOG

# log <msg...>  — timestamped line to the log sink (stderr by default)
log() {
    printf '[pi] %s\n' "$*" >>"$PI_LOG" 2>/dev/null || printf '[pi] %s\n' "$*" >&2
}

# die <msg...>  — log and abort
die() { log "FATAL: $*"; exit 1; }

# jget <path>   — read JSON on stdin, print the value at <path> (empty if absent)
jget() { "$PI_JSONX" get "$1"; }

# jstr          — read raw bytes on stdin, print a JSON string literal
jstr() { "$PI_JSONX" str; }

# deny <reason> — emit a minimal deny envelope and exit 10 (module short-circuit)
deny() {
    printf '{"decision":"deny","reason":%s}' "$(printf '%s' "$1" | "$PI_JSONX" str)"
    exit 10
}
