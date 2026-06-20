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

## Run the server (FreeBSD)

The package is published as a plain **download** on the GitHub Release — no pkg
repo or other hosted service required. Pick the `.pkg` matching your host's
FreeBSD version + CPU architecture (`lead-server-FreeBSD<major>-<arch>.pkg`):

```sh
# 1. dependencies (a local .pkg does not auto-fetch them)
pkg install -y mariadb118-server devil lzo2

# 2. download the matching package and install it — this creates the databases,
#    applies migrations, builds the runtime tree/symlinks and enables the service
fetch -o lead-server.pkg \
  https://github.com/Index-s/lead-files-community/releases/latest/download/lead-server-FreeBSD15-amd64.pkg
pkg add ./lead-server.pkg

# 3. set your public/bind IP in the configs, then start
#    /usr/local/etc/lead/{auth,channel1/game1,...}/CONFIG  and  db/conf.txt
service lead start
service lead status
```

**Updating** — run the bundled updater. It checks the latest GitHub Release,
and if it is newer than what's installed, downloads the matching FreeBSD/arch
package and upgrades in place (re-applying any new DB migrations):

```sh
/usr/local/libexec/lead/lead-update            # or --check to just compare
```

Full admin guide: [`Lead-Tools/freebsd-pkg/README.md`](Lead-Tools/freebsd-pkg/README.md).

### Architectures

A FreeBSD package is tagged `FreeBSD:<osmajor>:<arch>` (e.g. `FreeBSD:15:amd64`)
and its binaries are architecture-specific, so you build **one package per
target** on a host of that architecture — `build.sh` derives the arch from the
build host automatically and names the asset `lead-server-FreeBSD<major>-<arch>.pkg`,
so amd64/aarch64/etc. packages coexist in one Release and `lead-update` fetches
the one matching each host. Today the server source is **x86-64 (amd64)** only
(it compiles with `-m64`/SSE); other arches such as `aarch64` would need a source
port first, after which the packaging picks them up with no changes.

---

## Build the package

The package contains FreeBSD ELF binaries, so it **must be built on FreeBSD**, on
a host of the target CPU architecture (the package is tagged for whatever host
builds it). Pick the path that fits you:

### A. GitHub Actions — recommended, no local FreeBSD needed (works from Windows)

**Publish a GitHub Release** with a `vX.Y.Z` tag (the same flow the upstream repo
uses). The workflow triggers on the release, builds in a FreeBSD VM, and
**attaches the `.pkg` to that release** for download:

```sh
gh release create v1.0.0 --title "v1.0.0" --notes "..."
# or create the release from the GitHub web UI
```

- The `.pkg` is attached to the release; servers download + install it (see
  "Run the server" above) or pull updates with `lead-update`.
- `workflow_dispatch` is available for manual test builds without a release.
- **Optional** pkg repository: set repo variable `PUBLISH_PKG_REPO=true` to also
  publish a gh-pages repo for `pkg install`/`pkg upgrade`. Off by default — by
  default GitHub only hosts the downloadable package.

### B. Local build on Windows/macOS/Linux via VirtualBox (Vagrant)

Requirements: **VirtualBox**, **Vagrant**, and `rsync` on PATH.

> Note: `vagrant up` downloads a base VM image ("box") from **Vagrant Cloud** —
> HashiCorp's public registry of VM images (think Docker Hub for VMs). That's a
> one-time network download. If you'd rather **not use any registry**, use the
> QEMU recipe in option C instead (it pulls the official image straight from the
> FreeBSD project's own download mirror, then runs entirely locally).

```sh
cd Lead-Tools/freebsd-pkg
vagrant up                      # creates a FreeBSD VM and builds the .pkg
vagrant plugin install vagrant-scp
vagrant scp default:/repo/Lead-Tools/freebsd-pkg/dist/lead-server-FreeBSD15-amd64.pkg .
vagrant destroy -f
```

### C. On a FreeBSD host/VM directly (no registry / no cloud)

Requirements: a FreeBSD host of the target architecture. The build deps (incl.
the latest available MariaDB) are installed for you with `--install-deps` (root):

```sh
sh Lead-Tools/freebsd-pkg/build.sh --install-deps        # version defaults to 1.0.0
# -> dist/lead-server-1.0.0.pkg  AND  dist/lead-server-FreeBSD<major>-<arch>.pkg
```

No FreeBSD box on hand? Run one locally with **QEMU**, pulling the image directly
from the FreeBSD project's own mirror (no Vagrant Cloud / registry involved):

```sh
# official VM image (uncompress the .xz), then boot headless with serial console:
#   https://download.freebsd.org/releases/VM-IMAGES/15.1-RELEASE/amd64/Latest/
#       FreeBSD-15.1-RELEASE-amd64-ufs.qcow2.xz
qemu-system-x86_64 -machine q35 -accel kvm -m 4096 -smp 4 \
  -drive file=FreeBSD-15.1-RELEASE-amd64-ufs.qcow2,if=virtio \
  -vga none -nographic \
  -netdev user,id=n0,hostfwd=tcp:127.0.0.1:2222-:22 -device virtio-net-pci,netdev=n0
# then copy the repo in (scp -P 2222) and run build.sh as above.
```
(`-vga none` makes FreeBSD use the serial console; on non-Linux hosts replace
`-accel kvm` with your accelerator, e.g. `-accel whpx` on Windows or `hvf` on macOS.)

> **Native Windows build is not possible.** The server binaries are FreeBSD ELF
> (must be compiled on FreeBSD) and `pkg`/`pkg create` are FreeBSD-only tools.
> Use option **A** (CI) or **B** (a FreeBSD VM) instead — both are driveable from
> a Windows host.

---

## Build the client / Windows server

Open the Visual Studio solutions in `Lead-Client-Source/` and `Lead-Server-Source/`
(x64). See the per-directory notes for details.
