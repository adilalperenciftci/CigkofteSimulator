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
  addition, and the untracked tooling scripts and docs pending review.
- **C, duplicate** — the `CigkofteSim-wt` `.uproject` change.
- **D, obsolete and superseded** — the four pre-rewrite branches, and the AI
  service subsystem in particular.
- **E, generated** — every `Logs/` directory.
- **F, machine-local** — nothing identified beyond generated output.
- **G, licensed local-only** — `Plugins/Sentry` (MIT, redistributable, but
  unreviewed), and the 31 absent optional asset packs that the cook policy
  already handles by fallback.
- **H, unknown** — none. Every unique item above is accounted for.

## Removal status

No worktree may be removed yet.

| Path | Status |
|---|---|
| canonical | **unsafe to remove** — it is the intended source of truth and holds all uncommitted material |
| `CigkofteSim-wt` | safe to remove once the `.uproject` decision is recorded |
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

Still to do, in order:

1. Commit the untracked tooling — six `Scripts/*.ps1`, three `docs/*.md`,
   `Start-Blender-Asset-Mode.ps1` (which belongs under `Scripts/`) and the
   `AssetWork/` directory contract. They are secret-free but their content has
   not been reviewed for correctness.
2. Decide `Plugins/Sentry`: track, ignore or remove. It is MIT and
   redistributable, but it is 831 files, no `THIRD_PARTY_NOTICES.md` entry exists
   for it, and nothing references it. Do not enable it in the `.uproject` before
   that entry and a `CREDITS.md` line exist.
3. Declare canonical authoritative and stop working in the D: trees.
