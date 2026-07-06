#!/bin/sh
# posix/core/dispatch.sh — run the module pipeline for one hook.
#
#   Usage:  dispatch.sh <hook>        (envelope JSON on stdin; result on stdout)
#   Exit :  0  allow  (stdout = final, possibly-transformed envelope)
#           10 deny   (stdout = the denying module's {"reason": ...})
#
# Threads the envelope through the ordered module chain from config/pipeline.conf:
# each module's stdout feeds the next module's stdin; a DENY short-circuits; a
# module ERROR is resolved by that module's FAIL policy (closed => deny).
set -eu
: "${PI_ROOT:?PI_ROOT must be set}"
. "$PI_ROOT/core/lib.sh"

hook="${1:?hook required}"
PI_STATE_DIR="${PI_STATE_DIR:-${TMPDIR:-/tmp}}"

env_cur="$(mktemp "$PI_STATE_DIR/env.XXXXXX")"
env_next="$(mktemp "$PI_STATE_DIR/env.XXXXXX")"
cleanup() { rm -f "$env_cur" "$env_next"; }
trap cleanup EXIT INT TERM
cat >"$env_cur"

# Ordered module list for this hook from pipeline.conf ("hook: a b c").
modules="$(awk -v h="$hook" '
    /^[[:space:]]*#/ { next } /^[[:space:]]*$/ { next }
    { line = $0; sub(/#.*/, "", line)
      n = index(line, ":"); if (n == 0) next
      key = substr(line, 1, n - 1); gsub(/[[:space:]]/, "", key)
      if (key == h) print substr(line, n + 1) }
' "$PI_ROOT/config/pipeline.conf" 2>/dev/null || true)"

decision=allow
for m in $modules; do
    mdir="$PI_ROOT/modules/$m"
    conf="$mdir/module.conf"
    [ -f "$conf" ] || { log "dispatch[$hook]: missing module '$m' (skipped)"; continue; }

    ENABLED=1 FAIL=closed
    . "$conf"
    [ "${ENABLED:-1}" = "1" ] || continue

    set +e
    PI_HOOK="$hook" PI_MODULE_DIR="$mdir" PI_STATE_DIR="$PI_STATE_DIR" \
        "$mdir/run" "$hook" <"$env_cur" >"$env_next" 2>>"$PI_LOG"
    rc=$?
    set -e

    case "$rc" in
        0)  # allow — a non-empty stdout replaces the envelope (transform)
            if [ -s "$env_next" ]; then
                mv "$env_next" "$env_cur"
                env_next="$(mktemp "$PI_STATE_DIR/env.XXXXXX")"
            fi
            ;;
        10) # deny — short-circuit the chain
            [ -s "$env_next" ] && mv "$env_next" "$env_cur"
            reason="$(jget reason <"$env_cur" 2>/dev/null || true)"
            log "dispatch[$hook]: DENY by $m${reason:+ - $reason}"
            decision=deny
            break
            ;;
        *)  # module error — honor FAIL policy
            if [ "${FAIL:-closed}" = "closed" ]; then
                log "dispatch[$hook]: module '$m' error rc=$rc, fail=closed => DENY"
                printf '{"decision":"deny","reason":%s}' \
                    "$(printf 'module %s failed (rc=%s)' "$m" "$rc" | jstr)" >"$env_cur"
                decision=deny
                break
            fi
            log "dispatch[$hook]: module '$m' error rc=$rc, fail=open (skipped)"
            ;;
    esac
done

cat "$env_cur"
[ "$decision" = allow ] && exit 0 || exit 10
