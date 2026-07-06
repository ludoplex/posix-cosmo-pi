# posix/ — the hollowed POSIX + Cosmopolitan harness

A zero-Node re-implementation of the Pi agent harness: two
[Cosmopolitan](https://github.com/jart/cosmopolitan) APE binaries plus
deterministic POSIX `sh`, built **modularity-first** so security/filter/sanitize/
review logic lives in independent, hot-swappable module units.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the design. This README is the quickstart.

## Build

Needs the `cosmocc` toolchain on `PATH` (https://cosmo.zip/pub/cosmocc/cosmocc.zip).

```sh
cd posix
make            # builds bin/jsonx.com and bin/llm_bridge.com
make test       # builds, then runs the offline module-contract tests
```

The resulting `.com` files run natively on Linux, macOS, and Windows with no
interpreter and no dependencies.

## Run

```sh
export ANTHROPIC_API_KEY=sk-ant-...
export PI_WORKSPACE="$PWD"           # the sandbox root the agent is confined to
sh core/agent_loop.sh "List the files here and summarize the README."
```

Environment knobs:

| Var                 | Default                     | Meaning                                   |
| ------------------- | --------------------------- | ----------------------------------------- |
| `ANTHROPIC_API_KEY` | —                           | provider credential (kept out of `argv`)  |
| `PI_MODEL`          | `claude-opus-4-8`           | model id                                  |
| `PI_MAX_TOKENS`     | `8192`                      | response cap                              |
| `PI_EFFORT`         | `high`                      | adaptive-thinking effort                  |
| `PI_WORKSPACE`      | `$PWD`                      | filesystem root the agent may touch       |
| `PI_MAX_TURNS`      | `25`                        | loop safety bound                          |
| `PI_HTTP`           | `curl`                      | HTTPS client (point at a bundled `curl.com` for zero-dep) |
| `PI_BASE_URL`       | `https://api.anthropic.com` | provider endpoint                          |

## Add a module

The extensibility story in one example — a filter that blocks the model from
reading `.env` files:

```sh
mkdir -p modules/seccheck-dotenv-block
cat > modules/seccheck-dotenv-block/module.conf <<'EOF'
NAME="dotenv-block"
TYPE="seccheck"
HOOKS="pre_tool"
PRIORITY=15
FAIL="closed"
ENABLED=1
DESC="Deny reads of .env / secret files."
EOF
cat > modules/seccheck-dotenv-block/run <<'EOF'
#!/bin/sh
set -eu; : "${PI_ROOT:?}"; . "$PI_ROOT/core/lib.sh"
env="$(cat)"
path="$(printf '%s' "$env" | jget args.path 2>/dev/null || true)"
case "$path" in
  *.env|*.env.*|*/secrets/*) deny "reading secret file blocked: $path" ;;
esac
printf '%s' "$env"; exit 0
EOF
chmod +x modules/seccheck-dotenv-block/run
```

Then add it to the `pre_tool:` line in [`config/pipeline.conf`](config/pipeline.conf):

```
pre_tool:  seccheck-path-guard seccheck-dotenv-block seccheck-bash-allowlist
```

No core code changes. `run` can be an APE binary instead of a script — the
contract is stdin/stdout/exit-code, language-agnostic.

## Layout

```
posix/
├── ARCHITECTURE.md          design: hooks, module contract, envelope schema
├── Makefile                 cosmocc build targets
├── src/
│   ├── jsonx.c              deterministic JSON get/str/validate  → bin/jsonx.com
│   └── llm_bridge.c         Anthropic Messages bridge            → bin/llm_bridge.com
├── core/
│   ├── lib.sh               shared helpers (jget/jstr/log/deny)
│   ├── dispatch.sh          runs a hook's module chain
│   ├── agent_loop.sh        the deterministic loop
│   └── exec_tool.sh         the tool executor (read/write/ls/bash)
├── config/
│   ├── pipeline.conf        hook → ordered module list
│   ├── system.txt           system prompt
│   └── tools.json           Anthropic tool schema
├── modules/<name>/{module.conf,run}
└── test/run-tests.sh        offline module-contract tests (no network)
```
