# Worktree inventory

Every checkout of this repository on this machine, what is unique to it, and
what has to happen to it. Read-only forensics: nothing here was deleted, reset
or stashed to produce it.

Recorded 2026-08-02, against `origin/master` at `6e71e8b` (PR #9 merge), and
updated the same day at `bd306a0` (PR #11 merge) with the consolidation record at
the end.

## The checkouts

`git worktree list` reports six, not the four this work started from. The two
extra ones are real and are included below.

| Path | Branch | HEAD | Unique commits vs `origin/master` |
|---|---|---|---|
| `C:\Users\adila\Documents\Unreal Projects\CigkofteSimulator` | `docs/optimised-build-unmeasurable` | `7da8b4b` | none |
| `C:\Users\adila\Documents\Unreal Projects\CigkofteSim-wt` | `feat/commercial-demo-completion` | `f2e3ce3` | none |
| `C:\Users\adila\Documents\Unreal Projects\CigkofteSim-stage3-wt` | `feat/stage3-placement-authority` | `939d5c0` | none |
| `D:\CodexWorktrees\CigkofteSim-stage3-2` | `feat/stage3-placement-categories` | `c7eb90d` | none |
| `D:\CodexWorktrees\CigkofteSim-stage3-3` | `feat/stage3-layout-consequences` | `ca8cee2` | none — merged as `6e71e8b` |
| `D:\CodexWorktrees\CigkofteSim-stage3-validation` | detached | `a96d012` | none |

`C:\Users\adila\Documents\Unreal Projects\CigkofteSimulator-YEDEK-20260725-1824`
is the protected historical backup. It is not a worktree, it was not read for
this inventory beyond confirming it exists, and nothing in this plan touches it.

Every branch checked out anywhere is fully contained in `origin/master`. No
committed work is stranded in a worktree.

## Branches not contained in `origin/master`

Four local branches are ahead of master and none of them shares a commit with
it — `git merge-base` returns nothing for all four. They are the pre-rewrite
lineage, kept as local-only refs with no remote.

| Branch | Ahead | Files present there and absent from master |
|---|---|---|
| `feat/ai-diyalog-testler-config` | 16 | the five `Source/CigkofteSimulator/AI/` files |
| `archive/history-rewrite-2026-07` | 2 | the same five, plus `.claude/settings.json` and `.github/workflows/remove-claude-contributor.yml` |
| `archive/pre-cleanup-2026-07` | 3 | the same five, plus `.claude/settings.json` |
| `backup-coauthor-20260723` | 8 | none |

Comparing the old lineage tip with master over `Source/` gives 149 files changed,
19,981 insertions and 3,584 deletions in master's favour. Master is a superset
except for those five files.

### The five files are not recoverable work

`CigAIProvider.h`, `CigAIResponseCache.{h,cpp}` and `CigAIServiceSubsystem.{h,cpp}`
implement a runtime dialogue provider that issues an asynchronous HTTP request to
`https://api.anthropic.com/v1/messages`, authenticated with an `ANTHROPIC_API_KEY`
read from the environment.

That is the thing this project has since decided it must not be. It breaks
offline capability, it makes the shipped game depend on a paid third-party
service, and `Tools/check_sources.py` now fails the build on exactly this — the
`runtime ag` rule scans 31,426 lines for an external service call and finds none.
Dialogue is generated ahead of time instead: `Tools/generate_dialogue.py` and a
checked table of 39 lines across 21 buckets.

They were removed deliberately. They must stay removed. This entry exists so the
next audit does not rediscover them and mistake deletion for loss.

The other two absent files are `.claude/settings.json`, which is agent tooling
configuration rather than project source, and a contributor-cleanup workflow
that commit `9137a03` removed on purpose.

## Uncommitted work, by checkout

This is where the only genuinely unrecovered material is.

### Canonical — `CigkofteSimulator`

Modified: `CigkofteSimulator.uproject`. It adds two plugins:

- `ModelContextProtocol`, constrained with `"TargetAllowList": ["Editor"]`
- `AllToolsets`

The allow-list matters and should be preserved. `STATE.md` already records that
a locally enabled editor plugin whose modules load by default gets compiled into
the packaged game, and that packaging size regressions should be diagnosed by
diffing the plugin list first.

The same file has also been reindented from tabs to spaces throughout, which is
why the diff reads as a whole-file rewrite. That part is noise and must not be
committed; `.gitattributes` expects LF and the repository uses tabs here.

Untracked, 859 paths:

| What | Count | Disposition |
|---|---|---|
| `Plugins/Sentry` | 831 | Sentry UE plugin v1.17.0, MIT. Vendored but **not** referenced by the `.uproject`, so currently inert. Needs a tracked/ignored decision and a `THIRD_PARTY_NOTICES` entry before it is enabled. |
| `AssetWork/` | 18 | Scaffolding for the asset pipeline: `.gitkeep` markers, a licence README and a ComfyUI workflow template. Directory contract, not content. |
| `Scripts/` | 6 | `Get-GitDiff`, `Get-GitStatus`, `List-ProjectProcesses`, `New-GeneratedAssetMetadata`, `SafeCheckpoint`, `Start-ComfyUI`. |
| `docs/` | 3 | `CREATIVE_PIPELINES.md`, `LOW_TOKEN_TASK_TEMPLATE.md`, `WEB_AUTOMATION_POLICY.md`. |
| `Start-Blender-Asset-Mode.ps1` | 1 | Repository root. Belongs under `Scripts/` if it is kept. |
| `Logs/` | — | Generated. Ignorable. |

None of it has been reviewed for secrets, licence or provenance yet. That review
is a precondition for committing any of it, not a follow-up to it.

**Reviewed since.** The credential scan came back clean — the only matches for
the secret pattern are two lines of policy prose telling the reader not to write
tokens into project files. Reading the files then split them in two, and the
first version of this document was wrong to describe the whole set as "tooling to
commit".

Six of them are agent scaffolding rather than project tooling: a task template
that budgets skills and subagents, a browser-automation policy about which client
to prefer, thin wrappers around `git status` and `git diff` that exist so a tool
can be handed a narrow permission instead of a shell, a process lister that
emits JSON, a checkpoint guard, and a launcher that sets agent concurrency
environment variables. Those belong in the global ignore file with the rest of
the agent tooling, and that is where they went.

The rest is real production tooling and is committed: the `AssetWork` skeleton
the `.gitignore` was already describing, the external-asset licence record, the
ComfyUI workflow template, the launcher that binds that runtime to `127.0.0.1`
only, and the metadata writer that stamps prompt, model, model licence, seed and
usage plan onto every generated file. `CREATIVE_PIPELINES.md` went in after its
tool-name references were replaced with the method they describe; its version
claims — ComfyUI 0.28.0, FFmpeg 8.1.2, Blender 4.5.10 LTS — were checked against
the installed runtimes rather than copied.

`Plugins/Sentry` is ignored rather than vendored. `CRASH_PRIVACY.md` already
says an endpoint is enabled only after explicit consent, a privacy policy, a
stated retention period and a configured DSN. None of those exist, nothing
references the plugin and the `.uproject` does not enable it, so committing 831
files of it would have been deciding that by accident.

### `CigkofteSim-wt`

Modified: `CigkofteSimulator.uproject`. Adds `ModelContextProtocol` with **no**
target allow-list, and drops the trailing newline. Strictly worse than the
canonical version of the same change. Superseded; nothing to recover.

### `CigkofteSim-stage3-wt`, `CigkofteSim-stage3-2`, `CigkofteSim-stage3-validation`

Clean except for untracked `Logs/`. `CigkofteSim-stage3-2` has nothing untracked
at all. Generated output only; nothing to recover.

## Classification

Using the mission's categories:

- **A, valid completed development** — all of it is already in `origin/master`.
- **B, valid unfinished development** — the canonical `.uproject` plugin
  addition and the asset-pipeline scaffolding. Both are now on master.
- **C, duplicate** — the `CigkofteSim-wt` `.uproject` change and the stash.
- **D, obsolete and superseded** — the four pre-rewrite branches, and the AI
  service subsystem in particular.
- **E, generated** — every `Logs/` directory.
- **F, machine-local** — the six agent scaffolding files, now in the global
  ignore file rather than the repository.
- **G, licensed local-only** — `Plugins/Sentry` (MIT, redistributable, ignored
  rather than vendored), and the 31 absent optional asset packs that the cook
  policy already handles by fallback.
- **H, unknown** — none. Every unique item above is accounted for.

## Removal status

No worktree may be removed yet.

| Path | Status |
|---|---|
| canonical | **unsafe to remove** — it is the source of truth |
| `CigkofteSim-wt` | safe to remove; its `.uproject` variant is superseded |
| `CigkofteSim-stage3-wt` | safe to remove |
| `CigkofteSim-stage3-2` | safe to remove |
| `CigkofteSim-stage3-3` | safe to remove after canonical development resumes |
| `CigkofteSim-stage3-validation` | safe to remove |

"Safe to remove" is a finding, not an instruction. Nothing is deleted here.

## The stash

`git stash list` has one entry, and the first version of this document missed it.

`stash@{0}` is `WIP on feat/commercial-demo-overhaul: 4af66e0`. Its base commit
is an ancestor of master. Its content is a **third** hand-made variant of the
same `.uproject` plugin change: `ModelContextProtocol` and `AllToolsets` enabled
with no target allow-list, and the trailing newline dropped.

Nothing in it is unique and the version now on master is strictly better. It is
left in place rather than dropped — it costs nothing and this document is a
better record of it than its absence would be.

## Consolidation record

Done:

1. Canonical is on `master`. The switch needed no editor shutdown: the diff from
   its previous head to master touches no `Content/` path, only text files that a
   running editor does not lock.
2. The `.uproject` plugin entries are merged as PR #11 and the whole-file
   reindentation was dropped. Three checkouts had made this change three
   different ways and only the canonical one constrained `ModelContextProtocol`
   to Editor targets; the other two would have compiled an MCP server into the
   packaged game. Verified in both directions — see the PR.
3. The untracked canonical scripts and docs were scanned for credentials. Clean:
   the only matches for the secret pattern are two lines of policy prose telling
   the reader *not* to write tokens into project files.

4. The untracked files were read and split: agent scaffolding to the global
   ignore file, production tooling committed. See the canonical section above.
5. `Plugins/Sentry` is ignored rather than vendored, for the reason
   `CRASH_PRIVACY.md` already gives.

The canonical working tree is now clean — no modified files and nothing
untracked. Consolidation is complete and canonical is the source of truth.

## What still has to happen to the other checkouts

Nothing, until someone decides to remove them. They are historical and
read-only; no branch, fix, export, package or document should originate in one
again. The removal status table above says which are safe when that day comes.

If `Plugins/Sentry` is ever wanted, the order is: privacy policy, retention
period, consent flow, `THIRD_PARTY_NOTICES.md` and `CREDITS.md` entries, DSN
configuration — and only then an `.uproject` entry, constrained the same way the
editor plugins are.
