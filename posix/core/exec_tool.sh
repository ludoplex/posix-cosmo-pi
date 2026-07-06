#!/bin/sh
# posix/core/exec_tool.sh — execute one allowed tool call, output to stdout.
#
#   Usage:  exec_tool.sh <tool> <args-json>
#
# This is the executor. Security is enforced upstream by the pre_tool seccheck
# modules; this stays small and confines file paths to $PI_WORKSPACE as a
# defense-in-depth backstop.
set -eu
: "${PI_ROOT:?PI_ROOT must be set}"
. "$PI_ROOT/core/lib.sh"

tool="${1:?tool required}"
args="${2:-\{\}}"
ws="${PI_WORKSPACE:-$PWD}"

case "$tool" in
    read)
        p="$(printf '%s' "$args" | jget path)"
        cat -- "$ws/$p"
        ;;
    ls)
        p="$(printf '%s' "$args" | jget path 2>/dev/null || true)"
        ls -la -- "$ws/${p:-.}"
        ;;
    write)
        p="$(printf '%s' "$args" | jget path)"
        printf '%s' "$args" | jget content > "$ws/$p"
        printf 'wrote %s\n' "$p"
        ;;
    bash)
        cmd="$(printf '%s' "$args" | jget command)"
        ( cd "$ws" && sh -c "$cmd" )
        ;;
    *)
        printf 'unknown tool: %s\n' "$tool" >&2
        exit 1
        ;;
esac
