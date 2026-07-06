---
name: ci-workflow-update
description: Workflow command scaffold for ci-workflow-update in posix-cosmo-pi.
allowed_tools: ["Bash", "Read", "Write", "Grep", "Glob"]
---

# /ci-workflow-update

Use this workflow when working on **ci-workflow-update** in `posix-cosmo-pi`.

## Goal

Updates to the CI workflow for the posix-cosmo-pi project, including build/test steps and shellcheck configuration.

## Common Files

- `.github/workflows/posix-cosmo.yml`

## Suggested Sequence

1. Understand the current state and failure mode before editing.
2. Make the smallest coherent change that satisfies the workflow goal.
3. Run the most relevant verification for touched files.
4. Summarize what changed and what still needs review.

## Typical Commit Signals

- Edit .github/workflows/posix-cosmo.yml to update build/test/shellcheck steps.
- Commit changes with a message describing the CI adjustment.

## Notes

- Treat this as a scaffold, not a hard-coded script.
- Update the command if the workflow evolves materially.