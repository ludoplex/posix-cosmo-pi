# Hollowed POSIX/Cosmo harness — modular architecture

This tree re-implements the Pi agent harness as a **zero-Node, zero-npm** system:
a couple of [Cosmopolitan](https://github.com/jart/cosmopolitan) APE binaries plus
deterministic POSIX `sh`. It is designed **modularity-first** so that filtering,
sanitizing, deterministic security checks, and LLM reviews are each independent,
hot-swappable **module units** rather than logic baked into one loop.

```
legacy layer (TypeScript/Node)          hollowed layer (Cosmo APE / POSIX sh)
──────────────────────────────         ──────────────────────────────────────
@earendil-works/pi-ai  (API client)  →  src/llm_bridge.c   → bin/llm_bridge.com
tool-calling / JSON state            →  src/jsonx.c        → bin/jsonx.com
@earendil-works/pi-agent-core (loop) →  core/agent_loop.sh + core/dispatch.sh
@earendil-works/pi-tui  (interface)  →  ANSI via printf in sh
```

Only two things are compiled. Everything else is text-processing shell that a
reviewer can read top to bottom.

- **`jsonx.com`** — the deterministic JSON boundary. `get` a path, `str`-escape a
  value, `validate` a document. This is the "minimal compiled C binary" the
  blueprint calls for — it replaces both `jq` (an external dependency) **and**
  grep/sed-based JSON scraping (which is unsafe: JSON strings can contain `}`,
  `"`, and newlines that break regex parsing).
- **`llm_bridge.com`** — builds the provider request, performs the HTTPS call, and
  prints the raw response. First provider is the Anthropic Messages API
  (`POST /v1/messages`, `anthropic-version: 2023-06-01`, default model
  `claude-opus-4-8`, adaptive thinking). TLS is delegated to `curl` (config-file
  based, so the API key never appears in `argv`/process listings); a bundled
  `curl.com` APE keeps the zero-dependency property.

---

## The module system (the point of this redesign)

### Hook points

The loop emits well-defined boundaries. Modules registered for a hook run, in
order, at that boundary:

| Hook           | Fires when…                                   | Typical module types            |
| -------------- | --------------------------------------------- | ------------------------------- |
| `pre_request`  | outbound context is about to go to the LLM    | filter, sanitize (redact secrets)|
| `post_response`| a raw model response was just received        | sanitize (strip injection markers)|
| `pre_tool`     | the model requested a tool, before execution  | **seccheck**, **LLM review**    |
| `post_tool`    | a tool produced output, before it re-enters context | sanitize, filter          |
| `on_exit`      | the loop is tearing down                       | cleanup                         |

Wiring lives in [`config/pipeline.conf`](config/pipeline.conf): one line per hook,
listing the enabled modules in run order. Reorder a line, and you reorder the
pipeline. Delete a name, and that check is gone. Add a directory + one line, and
a new check is live — no core code changes.

### A module unit

A module is a self-contained directory `modules/<name>/`:

```
modules/seccheck-path-guard/
├── module.conf     # metadata (KEY=VALUE, sourced by the dispatcher)
└── run             # executable: sh script OR a compiled .com APE
```

`module.conf` fields:

| Key        | Meaning                                                        |
| ---------- | ------------------------------------------------------------- |
| `NAME`     | display name                                                  |
| `TYPE`     | `filter` \| `sanitize` \| `seccheck` \| `review`              |
| `HOOKS`    | space-separated hooks this module is designed for (documentation) |
| `PRIORITY` | intended order hint (the authoritative order is `pipeline.conf`) |
| `FAIL`     | `closed` (a module error ⇒ deny) or `open` (a module error ⇒ skip) |
| `ENABLED`  | `1`/`0` — a global off switch independent of `pipeline.conf`  |
| `DESC`     | one line                                                      |

### The contract (`run`)

```
invocation :  run <hook>                 # hook name in $1 and $PI_HOOK
input      :  the envelope JSON on STDIN
output     :  the (possibly transformed) envelope JSON on STDOUT
environment:  PI_ROOT PI_HOOK PI_MODULE_DIR PI_STATE_DIR PI_WORKSPACE
              PI_JSONX PI_BRIDGE PI_LOG
exit code  :  0   ALLOW    — continue; STDOUT (if non-empty) replaces the envelope
              10  DENY     — block the action; STDOUT carries {"reason": "..."}
              other ERROR  — dispatcher applies this module's FAIL policy
```

The dispatcher ([`core/dispatch.sh`](core/dispatch.sh)) threads the envelope through
the chain: each module's stdout becomes the next module's stdin, a `DENY`
short-circuits the rest, and an `ERROR` is resolved by `FAIL`. This makes modules
**composable** — a sanitizer can transform text that a downstream filter then
redacts, with no coupling between them.

### Envelope shapes

Two shapes, by hook family, both plain JSON that `jsonx get` navigates:

- **Tool hooks** (`pre_tool` / `post_tool`) use a tagged envelope:
  ```json
  { "hook": "pre_tool", "kind": "tool_call", "turn": 7,
    "tool": "bash", "args": { "command": "ls -la" }, "reason": "" }
  ```
- **Content hooks** (`pre_request` / `post_response`) pass the underlying JSON
  directly (the messages array, or the raw provider response). Content-oriented
  modules (e.g. secret redaction) operate on the bytes, so they work uniformly on
  either shape — a plain-text replacement leaves the JSON well-formed.

A `DENY` may emit a minimal `{"decision":"deny","reason":"…"}` — the loop only
reads `.reason` from a denied result and feeds it back to the model as a blocked
tool result, so the model can course-correct.

### The four categories, each shipped as a real unit

| Module                     | Type     | Hook(s)               | FAIL   | What it does |
| -------------------------- | -------- | --------------------- | ------ | ------------ |
| `seccheck-path-guard`      | seccheck | pre_tool              | closed | denies read/write/ls/edit whose path is absolute or escapes the workspace (`..`) |
| `seccheck-bash-allowlist`  | seccheck | pre_tool              | closed | denies clearly destructive/exfiltrating shell (`rm -rf /`, `curl … \| sh`, `mkfs`, fork bombs) |
| `sanitize-control-strip`   | sanitize | post_tool             | open   | strips ANSI escapes and control bytes so tool output can't inject terminal sequences into context |
| `filter-secret-redact`     | filter   | pre_request, post_tool| closed | redacts API keys, tokens, JWTs, PEM keys before they reach the model or re-enter context |
| `review-llm-tool-gate`     | review   | pre_tool              | closed | asks `llm_bridge.com` for an allow/deny verdict on the proposed tool call (**disabled by default** — costs a call) |

---

## Why this shape is the "hollowed privileges" model

1. **Deterministic static analysis.** A tool call is matched against explicit
   `case` rules in a `seccheck` module *before* anything executes. The rule set is
   a handful of readable lines, not an opaque V8 heap.
2. **Minimal semantic attack surface.** An indirect prompt injection inside a
   file cannot target a node_module CVE or a V8 JIT bug — there are none in the
   loop. If the injection convinces the model to request a bad tool call, the
   `seccheck` module flags it, or the `review` module escalates it to a second
   model, before the executor ever runs.
3. **True ephemerality.** The whole core is a few `sh` scripts and two APE
   binaries — a fresh sandbox starts in microseconds, with no `npm install` and
   no interpreter bootstrap.

## Not done yet (tracked for later steps)

- Native TLS in `llm_bridge.c` (mbedTLS from a full Cosmopolitan checkout) to drop
  the `curl` dependency entirely; today the bridge shells out to `curl`/`curl.com`.
- More providers behind the same bridge contract (OpenAI, Google).
- A `chroot`/`--userspec` jailed executor for the `bash` tool (blueprint Step 4).
- Porting the remaining coding-agent tools (grep/find/edit-diff) to modules.
