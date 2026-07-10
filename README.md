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
FreeBSD version + CPU architecture (`lead-server-FreeBSD<major>-<arch>.pkg`),
then install it with **one command**:

```sh
# download the matching package ...
fetch -o lead-server.pkg \
  https://github.com/Index-s/lead-files-community/releases/latest/download/lead-server-FreeBSD15-amd64.pkg

# ... and install it. Use `pkg install` (NOT `pkg add`): given a file path, pkg
# install resolves and fetches the dependencies (MariaDB, DevIL, lzo2) from the
# FreeBSD repo automatically.  This one command also creates the databases,
# applies migrations, builds the runtime tree/symlinks, autodetects the host IP,
# and enables the service.
pkg install -y ./lead-server.pkg

# start the server (db -> auth -> channels, in order)
lead-ctl start            # or: service lead start
lead-ctl status
```

> `pkg add ./lead-server.pkg` does **not** work on a bare host — `pkg add` never
> fetches dependencies, so it fails with `Missing dependency 'devil'`. Always use
> `pkg install ./...pkg`.

The server is managed entirely through **`lead-ctl`** (on PATH in `/usr/local/sbin`):

```sh
lead-ctl start [ch2]      lead-ctl stop [ch4]       lead-ctl restart
lead-ctl status           lead-ctl backup           lead-ctl migrate
lead-ctl configure        lead-ctl update           # update to a newer release
```

> Each game core loads map/quest data into RAM, so a full 4-channel server
> (12 cores) wants roughly **4 GB+ of RAM**. On a smaller box run fewer channels:
> `sysrc lead_channels="1"` (or `lead-ctl start ch1`).

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
builds it).

### How the package is created

One script does it — [`Lead-Tools/freebsd-pkg/build.sh`](Lead-Tools/freebsd-pkg/build.sh).
Run on a FreeBSD host it:

1. **(optional) `--install-deps`** — `pkg install`s the build deps (gmake, clang/llvm,
   png, jpeg-turbo, cryptopp, devil, boost-libs, the latest mariadb client) and
   auto-detects the newest available MariaDB for the runtime dependency.
2. **Compiles the server** from `Lead-Server-Source` with `gmake` (`--no-build`
   skips this and reuses existing `game`/`db` binaries).
3. **Stages a single self-contained tree** into a temp `STAGEDIR`:
   - `/usr/local/lead/share/bin/{game,db}` — the two server binaries
   - `/usr/local/lead/share/{data,locale,conf}` — game content (from `Lead-Serverfiles/share`)
   - `/usr/local/lead/{db,auth,channel1..4/game1..2,channel99,markserver}/CONFIG`
     — the per-core configs (from `Lead-Serverfiles`, **127.0.0.1** + canonical db names), marked `@config`
   - `/usr/local/share/lead/db-scripts/{base,migrations}` — SQL (from `Lead-Database-Scripts`)
   - `/usr/local/libexec/lead/{lead-db-setup,lead-layout,lead-configure,lead-update}` — helpers
   - `/usr/local/sbin/lead-ctl`, `/usr/local/etc/rc.d/lead` — the admin tool + service
   - prunes any 32-bit ELF (e.g. the quest compiler) so the package stays pure amd64
4. **Generates the manifest** — name/version/maintainer, the runtime deps **pinned to
   exact versions** (`devil`, `lzo2`, `mariadb<NNN>-server`), and embeds the lifecycle
   scripts (`+POST_INSTALL` etc.) + the install message.
5. **`pkg create`** → `dist/lead-server-<version>.pkg` **and** a stable
   `dist/lead-server-FreeBSD<major>-<arch>.pkg` copy (the one published as a release
   asset / used by `lead-update`).

So a build is just:
```sh
sh Lead-Tools/freebsd-pkg/build.sh --install-deps      # version defaults to 0.1.0
# -> Lead-Tools/freebsd-pkg/dist/lead-server-FreeBSD15-amd64.pkg
```
Everything the *installed* package does at install time (deps, DB seed + migrations,
symlinks, IP autodetect, service) lives in the embedded `+POST_INSTALL` script; the
build just packs it. CI runs this exact script in a FreeBSD VM — see below.

### A. GitHub Actions — recommended, no local FreeBSD needed (works from Windows)

**Publish a GitHub Release** with a `vX.Y.Z` tag (the same flow the upstream repo
uses). The workflow triggers on the release, builds in a FreeBSD VM, and
**attaches the `.pkg` to that release** for download:

```sh
gh release create v0.1.0 --title "v0.1.0" --notes "..."
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
sh Lead-Tools/freebsd-pkg/build.sh --install-deps        # version defaults to 0.1.0
# -> dist/lead-server-0.1.0.pkg  AND  dist/lead-server-FreeBSD<major>-<arch>.pkg
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

### Prerequisites

| Requirement | Version | Notes |
|-------------|---------|-------|
| Visual Studio | 2022 (v17) or newer | The projects use the **`v145`** platform toolset and **C++20** (`stdcpp20`). |
| Windows 10/11 SDK | any `10.0.x` | Targeted as `WindowsTargetPlatformVersion = 10.0`, which resolves to the **newest SDK installed**. Minimum is `10.0.10240` (first SDK that ships `d3d12.h`); the newest is recommended. |
| Workload | *Desktop development with C++* | This workload includes the MSVC toolset **and** the Windows SDK. |

The renderer is **DirectX 12**. No DirectX SDK is vendored and none needs to be:
`d3d12.h` / `dxgi1_4.h` / `d3dcompiler.h` all come from the Windows SDK above, and
they are linked via `#pragma comment(lib, ...)` in the sources — there is nothing to
add to the project's library paths. See [Renderer / DirectX 12](#renderer--directx-12).

### Check whether the Windows SDK is already installed

Any one of these confirms a usable SDK is present:

- **Visual Studio Installer** → *Modify* → *Individual components* → search "Windows
  SDK". A checked "Windows 11 SDK (10.0.x)" or "Windows 10 SDK (10.0.x)" entry means
  it is installed.
- **PowerShell** (lists every installed SDK version; if it prints a `10.0.*` folder
  you are set):
  ```powershell
  Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\Include" -Directory |
      Select-Object -ExpandProperty Name
  ```
- **Confirm the DX12 headers specifically** exist under a listed version:
  ```powershell
  Test-Path "${env:ProgramFiles(x86)}\Windows Kits\10\Include\<version>\um\d3d12.h"
  ```

### Install the Windows SDK (if missing)

- **Recommended — via Visual Studio Installer:** run the installer, *Modify* your VS
  2022 install, tick **Desktop development with C++** (bundles the latest Windows SDK),
  and under *Individual components* optionally tick a specific "Windows 11/10 SDK
  (10.0.x)". Install.
- **Standalone (no full VS):** download the *Windows SDK* from
  <https://developer.microsoft.com/windows/downloads/windows-sdk/> and run the
  installer. Building still requires the MSVC toolset (VS Build Tools).
- **Command line (winget):**
  ```powershell
  winget install --id Microsoft.WindowsSDK.10.0.26100
  ```

After installing, reopen the solution; because the projects target `10.0` (newest),
they pick up the new SDK automatically with no `.vcxproj` edit.

> **End users need none of this.** The Windows SDK is a *build-time* requirement only.
> The DirectX 12 **runtime** (`d3d12.dll`, `dxgi.dll`, `d3dcompiler_47.dll`) is part of
> every Windows 10/11 install, so players do not install anything extra to run the
> DirectX 12 client.

### Renderer / DirectX 12

The client renders exclusively through **DirectX 12** (the legacy Direct3D 9Ex path
was removed). Key facts for building and shipping:

- **Feature Level 11_0.** The device is created at `D3D_FEATURE_LEVEL_11_0` — the
  Direct3D *feature level* is the hardware-capability baseline a GPU must meet,
  independent of the API version. `11_0` corresponds to Direct3D 11-class hardware
  (roughly any GPU from ~2010 onward: NVIDIA Fermi/GT 400, AMD TeraScale 2/HD 5000,
  Intel HD Graphics of the Ivy Bridge era and later). DX12 running at FL 11_0 means
  "use the modern DirectX 12 API, but only require GPU features that DX11-era cards
  already have" — the widest possible compatibility for a 2005-era MMO.
- **No Agility SDK.** The backend uses only OS-inbox Direct3D 12 (classic barriers,
  Shader Model 5 via FXC, one direct queue). The DirectX 12 Agility SDK is neither
  required nor bundled; it would only be needed to adopt newer features (enhanced
  barriers, Shader Model 6 via DXC, DRED 1.2+) on older Windows 10 builds.
- **Shaders** are compiled at runtime with FXC (`D3DCompile`, `vs_5_0`/`ps_5_0`,
  `D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY`); no offline shader build step.
