#!/bin/sh
# posix/core/agent_loop.sh — deterministic, modular POSIX agent loop.
#
#   Usage:  agent_loop.sh "task text"      (or pipe the task on stdin)
#
# Each turn: run the pre_request pipeline over the outbound context, call the
# compiled bridge, run post_response over the reply, then for every tool_use the
# model requests, run the pre_tool pipeline (deterministic seccheck + optional
# LLM review); execute allowed calls; run post_tool over their output; feed the
# results back. All security/filter/sanitize/review logic lives in the modules —
# this loop only wires the hooks.
set -eu
PI_ROOT="${PI_ROOT:-$(unset CDPATH; cd -- "$(dirname -- "$0")/.." && pwd)}"
export PI_ROOT
. "$PI_ROOT/core/lib.sh"

PI_WORKSPACE="${PI_WORKSPACE:-$PWD}"
PI_STATE_DIR="${PI_STATE_DIR:-$(mktemp -d "${TMPDIR:-/tmp}/pi.XXXXXX")}"
export PI_WORKSPACE PI_STATE_DIR
MAX_TURNS="${PI_MAX_TURNS:-25}"
export PI_SYSTEM="${PI_SYSTEM:-$PI_ROOT/config/system.txt}"
export PI_TOOLS="${PI_TOOLS:-$PI_ROOT/config/tools.json}"

MSGS="$PI_STATE_DIR/messages.json"    # JSON array of Anthropic messages
resp="$PI_STATE_DIR/response.json"

# Seed the conversation with the user's task (argv or stdin).
task="${*:-}"
[ -n "$task" ] || task="$(cat)"
printf '[{"role":"user","content":%s}]' "$(printf '%s' "$task" | jstr)" >"$MSGS"

turn=0
while [ "$turn" -lt "$MAX_TURNS" ]; do
    turn=$((turn + 1))
    log "turn $turn"

    # --- pre_request: filter/sanitize the outbound context ---
    if "$PI_ROOT/core/dispatch.sh" pre_request <"$MSGS" >"$PI_STATE_DIR/msgs.checked"; then
        mv "$PI_STATE_DIR/msgs.checked" "$MSGS"
    else
        die "pre_request pipeline denied the outbound context"
    fi

    # --- model call via the compiled APE bridge ---
    "$PI_BRIDGE" "$MSGS" >"$resp" 2>>"$PI_LOG" || die "bridge call failed (see $PI_LOG)"

    # --- post_response: sanitize raw model output ---
    "$PI_ROOT/core/dispatch.sh" post_response <"$resp" >"$PI_STATE_DIR/resp.checked" || true
    [ -s "$PI_STATE_DIR/resp.checked" ] && mv "$PI_STATE_DIR/resp.checked" "$resp"

    # Surface a provider error early instead of looping on it.
    if [ "$(jget type <"$resp" 2>/dev/null || true)" = error ]; then
        printf 'provider error: %s\n' "$(jget error.message <"$resp" 2>/dev/null || true)" >&2
        exit 1
    fi

    asst="$(jget content <"$resp" || true)"   # assistant content array (raw JSON)
    stop="$(jget stop_reason <"$resp" || true)"

    # --- walk content blocks: print text, gate + execute tool_use ---
    i=0
    had_tool=0
    results=""
    while :; do
        btype="$(jget "content.$i.type" <"$resp" 2>/dev/null || true)"
        [ -n "$btype" ] || break
        case "$btype" in
            text)
                jget "content.$i.text" <"$resp"; printf '\n'
                ;;
            tool_use)
                had_tool=1
                tname="$(jget "content.$i.name" <"$resp")"
                tid="$(jget "content.$i.id" <"$resp")"
                targs="$(jget "content.$i.input" <"$resp")"   # raw JSON object

                envelope="$(printf '{"hook":"pre_tool","kind":"tool_call","turn":%s,"tool":%s,"args":%s,"reason":""}' \
                    "$turn" "$(printf '%s' "$tname" | jstr)" "$targs")"

                if printf '%s' "$envelope" | "$PI_ROOT/core/dispatch.sh" pre_tool \
                        >"$PI_STATE_DIR/env.checked"; then
                    out="$PI_STATE_DIR/tool.out"
                    "$PI_ROOT/core/exec_tool.sh" "$tname" "$targs" >"$out" 2>&1 || true
                    post="$(printf '{"hook":"post_tool","kind":"tool_result","tool":%s,"text":%s}' \
                        "$(printf '%s' "$tname" | jstr)" "$(jstr <"$out")")"
                    printf '%s' "$post" | "$PI_ROOT/core/dispatch.sh" post_tool \
                        >"$PI_STATE_DIR/post.checked" || true
                    content_txt="$(jget text <"$PI_STATE_DIR/post.checked" 2>/dev/null || cat "$out")"
                else
                    reason="$(jget reason <"$PI_STATE_DIR/env.checked" 2>/dev/null || echo 'blocked by security policy')"
                    content_txt="TOOL BLOCKED: $reason"
                    log "tool '$tname' blocked: $reason"
                fi

                block="$(printf '{"type":"tool_result","tool_use_id":%s,"content":%s}' \
                    "$(printf '%s' "$tid" | jstr)" "$(printf '%s' "$content_txt" | jstr)")"
                results="${results:+$results,}$block"
                ;;
        esac
        i=$((i + 1))
    done

    # --- append assistant turn (+ tool results) to the conversation ---
    base="$(cat "$MSGS")"; base="${base%]}"
    newmsgs="$base,{\"role\":\"assistant\",\"content\":$asst}"
    [ -n "$results" ] && newmsgs="$newmsgs,{\"role\":\"user\",\"content\":[$results]}"
    printf '%s]' "$newmsgs" >"$MSGS"

    # Done when the model stops requesting tools.
    if [ "$had_tool" -eq 0 ] || [ "$stop" != tool_use ]; then
        log "loop complete (stop_reason=$stop) after $turn turn(s)"
        break
    fi
done

# --- on_exit hook (cleanup modules) ---
printf '{"hook":"on_exit"}' | "$PI_ROOT/core/dispatch.sh" on_exit >/dev/null 2>&1 || true
