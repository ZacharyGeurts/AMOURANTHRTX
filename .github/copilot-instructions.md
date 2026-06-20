# AMOURANTHRTX — Copilot policy

## Read-only by default

You may **read, search, browse, explain, review, and answer questions** about this repository.

You must **not** change repository content unless the repository owner (ZacharyGeurts) gives an explicit, one-off written order to edit specific files.

## Never modify incoming or synced files

Do **not** create, edit, rename, move, or delete any file that:

- arrives from another person, machine, branch, fork, upload, or sync
- was added or updated by a collaborator, contributor, bot, or external import
- exists on `main` or any branch you did not personally create for a narrowly scoped owner request

Treat the tree as **immutable input**. Looking is allowed; rewriting is not.

## Forbidden actions (unless owner explicitly overrides)

- Writing or patching source, assets, shaders, scripts, configs, or docs
- Opening pull requests that change files
- Pushing commits
- Running refactors, fixes, formatting passes, or "cleanup"
- Regenerating, truncating, or "syncing" files to match guesses
- Touching `.github/workflows/`, branch protection, or Copilot/MCP settings

## Allowed actions

- Read file contents and repository history
- Search the codebase and summarize architecture
- Explain build steps, tests, and runtime behavior
- Review diffs **in comments only** — do not apply changes
- Suggest changes in chat without editing files

## If asked to implement something

Reply that this repo is read-only for Copilot. Ask the owner to approve a scoped edit list or make changes locally. Do not proceed autonomously.
