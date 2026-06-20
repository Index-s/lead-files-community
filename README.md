# Lead Files Community

A Metin2-class game (client + server), ported to native **x64**. The server runs
on **FreeBSD/amd64** and ships as a single FreeBSD package, `lead-server`, that
installs and configures everything (MariaDB, schema + migrations, symlinks,
service) in one command.

## Repository layout

| Directory | Contents |
|-----------|----------|
| `Lead-Client/` | client runtime (DLLs, content) |
| `Lead-Client-Source/` | client source (Visual Studio, x64) |
| `Lead-Server-Source/` | server source (FreeBSD, `gmake` + clang) |
| `Lead-Serverfiles/` | server runtime tree, configs, content |
| `Lead-Database-Scripts/` | base SQL + ordered migrations |
| `Lead-Shared-Source/` | packet/struct headers shared client↔server |
| `Lead-Extern/` | third-party libs/headers |
| `Lead-Tools/` | tooling, incl. **`freebsd-pkg/`** (the package) |

---

## Run the server (FreeBSD/amd64)

Once a package is published to the repo, a bare FreeBSD host needs **one command**:

```sh
# 1. add the package repository (once)
fetch -o /usr/local/etc/pkg/repos/lead.conf \
    https://INDEX-S.github.io/lead-files-community/lead.conf
pkg update

# 2. install — this also installs MariaDB, creates the databases, applies
#    migrations, builds the runtime tree/symlinks and enables the service
pkg install lead-server

# 3. set your public/bind IP in the configs, then start
#    /usr/local/etc/lead/{auth,channel1/game1,...}/CONFIG  and  db/conf.txt
service lead start
service lead status
```

Upgrade later with `pkg update && pkg upgrade lead-server` — new database
migrations are applied automatically. Full admin guide:
[`Lead-Tools/freebsd-pkg/README.md`](Lead-Tools/freebsd-pkg/README.md).

---

## Build the package

The package contains FreeBSD ELF binaries, so it **must be built on FreeBSD/amd64**.
Pick the path that fits you:

### A. GitHub Actions — recommended, no local FreeBSD needed (works from Windows)

**Publish a GitHub Release** with a `vX.Y.Z` tag (the same flow the upstream repo
uses). The workflow triggers on the release, builds in a FreeBSD 15.1 VM and
publishes the result:

```sh
gh release create v0.1.0 --title "v0.1.0" --notes "..."
# or create the release from the GitHub web UI
```

- The repository is published to the **gh-pages** branch (enable Pages →
  branch `gh-pages`).
- The `.pkg` is attached to that release — download and `pkg add` it.
- (`workflow_dispatch` is available for manual test builds without a release.)

### B. Local build on Windows/macOS/Linux via VirtualBox (Vagrant)

Requirements: **VirtualBox**, **Vagrant**, and `rsync` on PATH.

```sh
cd Lead-Tools/freebsd-pkg
vagrant up                      # creates a FreeBSD VM and builds the .pkg
vagrant plugin install vagrant-scp
vagrant scp default:/repo/Lead-Tools/freebsd-pkg/dist/lead-server-0.1.0.pkg .
vagrant destroy -f
```

### C. On a FreeBSD host/VM directly

Requirements: FreeBSD 15.x amd64. The build deps (incl. the latest available
MariaDB) are installed for you with
`--install-deps` (needs root):

```sh
sh Lead-Tools/freebsd-pkg/build.sh --install-deps --version 0.1.0
# -> Lead-Tools/freebsd-pkg/dist/lead-server-0.1.0.pkg

# optional: turn it into a publishable repository
sh Lead-Tools/freebsd-pkg/make-repo.sh --url https://INDEX-S.github.io/lead-files-community
```

> **Native Windows build is not possible.** The server binaries are FreeBSD ELF
> (must be compiled on FreeBSD) and `pkg`/`pkg create` are FreeBSD-only tools.
> Use option **A** (CI) or **B** (a FreeBSD VM) instead — both are driveable from
> a Windows host.

---

## Build the client / Windows server

Open the Visual Studio solutions in `Lead-Client-Source/` and `Lead-Server-Source/`
(x64). See the per-directory notes for details.
