# Lead server — FreeBSD package

This directory builds a native FreeBSD/amd64 package, **`lead-server`**, that
turns a bare FreeBSD host into a running Lead server with a single command:

```sh
pkg install lead-server
```

That one command:

- pulls in **MariaDB** and **lzo2** automatically (declared dependencies,
  resolved from the official FreeBSD repo);
- installs the `db` + `game` cores, the shared content tree, the per-core
  configs, the DB-bootstrap tool and an `rc.d` service;
- creates the `lead` user, builds the runtime tree and **symlinks** under
  `/var/db/lead` (replacing every old `install.sh`);
- starts MariaDB and **creates the databases, the SQL user and applies every
  migration** (idempotently);
- enables the `lead` service.

The package is published as a **pkg repository on GitHub Pages**, so new
releases are delivered with `pkg upgrade` — and each upgrade re-applies only the
new database migrations.

---

## Layout once installed

| Path | Contents |
|------|----------|
| `/usr/local/libexec/lead/{game,db}` | the two server binaries |
| `/usr/local/libexec/lead/lead-db-setup` | idempotent DB bootstrap |
| `/usr/local/libexec/lead/lead-layout` | (re)builds the runtime tree + symlinks |
| `/usr/local/share/lead/{data,locale,package,conf}` | static content |
| `/usr/local/share/lead/db-scripts/{base,migrations}` | SQL |
| `/usr/local/etc/lead/<core>/CONFIG`, `/usr/local/etc/lead/db/conf.txt` | configs (`@config`) |
| `/usr/local/etc/rc.d/lead` | service |
| `/var/db/lead/<core>/` | per-core working dirs (logs, cores, marks) + symlinks |

Cores shipped: `db`, `auth`, `channel1/game1`, `channel1/game2`, `channel99`,
`markserver`. Adjust the running set via `lead_cores` in `/etc/rc.conf`.

---

## For the server admin

1. **Add the repo** (once):
   ```sh
   fetch -o /usr/local/etc/pkg/repos/lead.conf \
       https://INDEX-S.github.io/lead-files-community/lead.conf
   pkg update
   ```
2. **Install**:
   ```sh
   pkg install lead-server
   ```
3. **Set the public/bind IP** in the `@config` files before first start:
   `/usr/local/etc/lead/auth/CONFIG`, the `channel*/CONFIG`s and
   `/usr/local/etc/lead/db/conf.txt`.
4. **Start**:
   ```sh
   service lead start
   service lead status
   ```
5. **Upgrade later**:
   ```sh
   pkg update && pkg upgrade lead-server   # new migrations apply automatically
   ```

Re-run the DB bootstrap any time (safe, idempotent):
```sh
/usr/local/libexec/lead/lead-db-setup
```

If your database is **not** local (e.g. a shared MariaDB), point the tool at it:
```sh
LEAD_DB_HOST=10.0.0.5 LEAD_DB_ADMIN_USER=root LEAD_DB_ADMIN_PASSWORD=secret \
    /usr/local/libexec/lead/lead-db-setup
```

---

## For the maintainer (building the package)

Everything must be built **on FreeBSD/amd64** (it produces amd64 ELF binaries).

### Manual build (your QEMU VM)

```sh
# one-time: install build deps (needs root)
sh packaging/freebsd/build.sh --install-deps --no-build

# build the package
sh packaging/freebsd/build.sh --version 1.0.0
# -> packaging/freebsd/dist/lead-server-1.0.0.pkg

# turn it into a publishable repo (optionally signed)
sh packaging/freebsd/make-repo.sh --url https://INDEX-S.github.io/lead-files-community
# -> packaging/freebsd/repo-out/   (publish this tree to gh-pages)
```

Test it on a clean jail/VM:
```sh
pkg add ./packaging/freebsd/dist/lead-server-1.0.0.pkg   # or via the repo
service lead start
```

### Automated build (GitHub Actions)

`.github/workflows/freebsd-pkg.yml` builds in a FreeBSD VM and publishes on every
`vX.Y.Z` tag:

```sh
git tag v1.0.0 && git push origin v1.0.0
```

- Repo is published to the **gh-pages** branch (enable Pages → branch `gh-pages`).
- The `.pkg` is also attached to the GitHub release.
- Optional signing: add an RSA private key as the `LEAD_REPO_SIGNING_KEY` secret;
  the workflow signs the catalog and emits `lead-repo.pub` (clients install it to
  `/usr/local/etc/pkg/keys/` and set `signature_type: "pubkey"`).

---

## How the pieces map to your old workflow

| Old | New |
|-----|-----|
| `Lead-Serverfiles/*/install.sh` (symlinks, dirs, chmod) | `lead-layout` + plist (`@dir`/`@config`) |
| `Lead-Serverfiles/start.sh`, per-core `run.sh` | `service lead start` (`rc.d/lead`) |
| `Lead-Database-Scripts/setup.py` | `lead-db-setup` (idempotent, migration-tracked, latin1-safe) |
| manual dep install | declared `deps` (`pkg` pulls MariaDB + lzo2) |
| copy binaries by hand | `pkg install` / `pkg upgrade` |

### Notes / trade-offs

- The post-install **starts MariaDB and seeds the schema**. This is slightly
  outside official-ports etiquette but intended for a turnkey appliance; every
  step is best-effort so `pkg install` never fails on it, and idempotency makes
  re-runs safe.
- Player data and the databases are **preserved on `pkg delete`** — removal is
  left to the admin (see the deinstall message).
- The server links almost everything statically; the only non-base runtime
  dependency is `lzo2`. `build.sh` prints `ldd` output so the manifest stays
  honest if that ever changes.
