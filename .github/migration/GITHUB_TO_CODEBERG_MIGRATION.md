# Migrating from GitHub to Codeberg

A guide for moving a project (e.g. the Hive engine) from GitHub to Codeberg, with
particular attention to CI setup and Git LFS budget — since LFS download volume was
the specific bottleneck that motivated this move.

## 1. What's different about Codeberg

Codeberg is a non-profit (Codeberg e.V.), donation-funded Git hosting service running
[Forgejo](https://forgejo.org/) (a Gitea fork), not GitHub's stack. Two practical
consequences follow directly from "non-profit, donation-funded":

- **Resources are shared and finite by design**, with explicit quotas rather than the
  effectively-unlimited-until-you-hit-a-wall model GitHub uses for free accounts.
- **CI is opt-in and manually reviewed**, not automatically available the moment you
  push a repo.

Neither is a downgrade in capability — Woodpecker CI is a fully capable pipeline
system, and self-hosting your own CI agent is explicitly supported and easy — but the
operating model requires deliberate setup rather than "it just works like GitHub did."

## 2. Storage & Git LFS quotas — the part that bit you on GitHub

As of the May 2025 rollout, Codeberg enforces storage quotas **per user/organization
account** (aggregated across your repositories, not a single blanket per-repo number):

| Category | Quota |
|---|---|
| Git repository storage | 750 MiB |
| Git LFS + Packages + Releases + Attachments (combined) | additional 1.5 GiB |
| Private / "non-promoted" repos (dotfiles, personal sites) | 100 MiB total |

Source: [Codeberg blog, "New storage limits on Codeberg"](https://blog.codeberg.org/new-storage-limits-on-codeberg-what-you-need-to-know.html)
(May 2025).

**Important nuances:**
- These are *storage* quotas (how much you can store), checked in
  `https://codeberg.org/user/settings/storage_overview` (and the equivalent org
  settings page). They are not the same thing as a *bandwidth/download* quota — but on
  a donation-funded, fair-use platform, egress is also finite in practice, and repeated
  large downloads (e.g. CI checking out LFS objects on every run) are exactly the kind
  of usage pattern Codeberg's fair-use enforcement is watching for.
- If you expect to exceed the default (e.g. migrating a repo with a large existing LFS
  history), **request an exception before migrating**, via
  [Codeberg-e.V./requests](https://codeberg.org/Codeberg-e.V./requests). Reviews are
  manual, so do this early — not after you've already pushed and hit a wall.
- Exceptions are more readily granted to established free/libre software projects with
  a clear, described use case — write a short, honest justification when you file the
  request (what the project is, why it needs the space, expected growth).

### Specifically addressing your GitHub LFS bottleneck

Note: the C++ migration itself already reduces this pressure substantially — compiled
engine binaries and a quantized NNUE weights file are far smaller than whatever was
driving LFS usage under Julia (e.g. precompiled sysimages, package artifacts, or larger
uncompressed data blobs). So treat the points below as low-effort good hygiene rather
than an urgent fire to put out — worth doing, but not a blocker for the migration.

The problem you had on GitHub — burning through LFS bandwidth from repeated
downloads — is a *usage pattern* problem more than a storage-size problem, and it
carries over to any Git host unless you change the pattern. Concrete mitigations:

1. **Audit what's actually in LFS.** Run `git lfs ls-files` and confirm only genuinely
   large binaries (NNUE network files, opening books, training datasets, etc.) are
   tracked — not anything a plain diff-able text format could serve just as well.
2. **Don't let CI fetch full LFS history on every run.** Default `git lfs pull` fetches
   every tracked object. Instead:
   - Use `GIT_LFS_SKIP_SMUDGE=1` on clone, then `git lfs pull --include="<pattern>"` for
     only the files a given CI job actually needs (e.g. the eval net for a bench job,
     not the full training corpus).
   - Use shallow clones (`git clone --depth 1`) in CI where full history isn't needed.
3. **Cache LFS objects between CI runs** rather than re-downloading every time. On
   Woodpecker this means either a cache plugin/volume mounted across runs, or — more
   robustly — a self-hosted agent (below), where the objects can simply persist on
   local disk between jobs indefinitely.
4. **Reconsider whether large, rarely-changing assets belong in LFS at all.** For
   things like a released NNUE weights file, an external download step (e.g. fetching
   a fixed asset from a Release, or a separate artifact store) inside the CI job can
   be cheaper and simpler than versioning it through LFS, especially if it doesn't
   change every commit.
5. **Prune regularly.** `git lfs prune` locally to drop old objects you don't need in
   your working copy; on the server side, remove genuinely obsolete large files from
   history (`git lfs migrate`) before the initial push to Codeberg, so you're not
   paying storage/bandwidth for dead weight from day one.

## 3. CI: Woodpecker CI on Codeberg

Codeberg's CI runs on [Woodpecker CI](https://woodpecker-ci.org/), not GitHub Actions —
config lives in `.woodpecker.yml` at the repo root, conceptually similar to GitHub
Actions workflows but with a different syntax (`steps`/`when`/`image` rather than
`jobs`/`steps`/`uses`).

**Key operational differences from GitHub Actions:**

- **Onboarding is manual, not automatic.** CI is not enabled the moment you create a
  repo. You need to request access at [ci.codeberg.org](https://ci.codeberg.org) (the
  onboarding form), and it's reviewed by Codeberg moderators. **Do this as one of your
  first steps in the migration** — approval isn't instant, and you don't want it
  blocking your first pushes.
- **No published hard "minutes" quota** — unlike GitHub Actions' explicit minute
  allowance, Codeberg's shared Woodpecker runners operate on a fair-use basis. There's
  no meter to watch, but also no guaranteed capacity; heavy, constant, or wasteful CI
  usage can get a project asked to scale back or move to self-hosting.
- **Only `linux/amd64` is supported** on the shared runners currently — relevant if
  you need other platforms/architectures.
- **Self-hosted agents are fully supported and straightforward**: you run a
  `woodpecker-agent` Docker container (or Kubernetes/Helm deployment) that connects to
  `ci.codeberg.org` with a secret token generated in your account/org CI settings, and
  your repo's jobs can then run on your own hardware instead of (or alongside) the
  shared pool.

**Recommendation for a project tracking engine performance over time:**
Given you're already planning bench/perft regression tracking (per the C++ migration
doc), a **self-hosted Woodpecker agent** is worth setting up early rather than as a
later escape hatch:
- Removes any fair-use ambiguity for frequent CI runs.
- Gives you consistent, dedicated hardware for benchmark numbers — shared CI runners
  have noisy-neighbor variance that undermines exactly the kind of "track nodes/sec
  over time" regression testing you want for the engine.
- LFS objects can persist on the agent's local disk across runs, which is a nice bonus
  but no longer the primary motivation now that C++ binaries/NNUE files keep LFS usage
  small in the first place (see §2).

Minimal `.woodpecker.yml` example to start from:
```yaml
steps:
  build-and-test:
    image: alpine  # replace with your actual build image, e.g. a C++ toolchain image
    commands:
      - cmake -B build -DCMAKE_BUILD_TYPE=Release
      - cmake --build build
      - ctest --test-dir build --output-on-failure
```

## 4. Step-by-step migration plan

1. **Request CI onboarding early** at ci.codeberg.org — do this in parallel with
   everything else below, since manual review takes time.
2. **Decide on LFS hygiene before migrating**, not after:
   - Run `git lfs ls-files` and `git lfs migrate info` on the existing repo to see
     what's actually large and why.
   - Prune/rewrite history to drop obsolete large objects if it's safe to do so
     (coordinate with any collaborators — this rewrites history).
3. **Check expected storage against quota** (750 MiB repo + 1.5 GiB LFS/releases/etc.
   per account) and file an exception request *before* pushing if you're likely to
   exceed it.
4. **Create the repo on Codeberg** and push:
   ```powershell
   git remote add codeberg https://codeberg.org/<user>/<repo>.git
   git push codeberg --all
   git push codeberg --tags
   git lfs push codeberg --all
   ```
5. **Set up `.woodpecker.yml`** once CI access is approved; port your GitHub Actions
   workflow logic (build, test, bench) into Woodpecker steps.
6. **Decide GitHub's role going forward** — common options: archive/read-only mirror
   on GitHub (for visibility) with Codeberg as the canonical remote, or a one-way
   push mirror from Codeberg to GitHub if you still want GitHub presence without
   maintaining it manually. Codeberg supports repo mirroring natively — check current
   docs at [docs.codeberg.org](https://docs.codeberg.org/) for the up-to-date mirroring
   setup, since this feature has evolved.
7. **Migrate issues/PRs only if genuinely needed** — there's no lossless native
   importer guaranteed to match GitHub's model exactly; for an early-stage personal
   project this is usually not worth the effort compared to starting fresh on
   Codeberg's tracker.
8. **Update local clones and any CI secrets/badges** referencing the old GitHub URLs.

## 5. Ongoing monitoring

- Periodically check your quota usage page (`/user/settings/storage_overview` or the
  org equivalent) — treat it like disk-space monitoring, not a one-time setup step.
- Keep the LFS-hygiene habits from §2 as standing practice (audit what's tracked,
  prune, avoid full pulls in CI) rather than a one-time migration cleanup — quota
  pressure and CI fair-use both scale with ongoing usage patterns, not just initial
  repo size.
- If usage grows legitimately (e.g. more NNUE training data), file exception requests
  proactively rather than waiting to hit a hard block.
