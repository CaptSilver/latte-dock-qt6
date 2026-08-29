# RPM packaging

The package is `latte-dock-qt6`. Fedora ships no `latte-dock` of its own, so nothing collides —
but mind the name: this is the Qt6 / Plasma 6 fork, not the unmaintained Qt5 dock.

Two ways to get it: install from COPR, or build it yourself.

## Install from COPR

Replace `captsilver666` with the COPR account hosting the build.

### Bazzite / Kinoite / Silverblue / any rpm-ostree host

If you have ever layered a hand-built latte-dock RPM **from a file**, remove it first:

```sh
rpm-ostree status                       # look for LocalPackages: latte-dock-…
sudo rpm-ostree uninstall latte-dock
```

It will not upgrade on its own, for two reasons. A package layered from a file is pinned to that
exact file forever — it is never re-resolved against a repo, so it cannot see a COPR build. And the
name changed: nothing upgrades `latte-dock` to `latte-dock-qt6`. Both own `/usr/bin/latte-dock`
besides, so they will not sit side by side.

Then install by name:

```sh
sudo dnf5 copr enable captsilver666/latte-dock-qt6
sudo rpm-ostree install latte-dock-qt6
systemctl reboot
```

`sudo rpm-ostree uninstall latte-dock --install latte-dock-qt6` does the swap in one transaction if
you prefer.

On an image with no dnf at all, write the repo file yourself:

```sh
sudo curl -o /etc/yum.repos.d/_copr_captsilver666-latte-dock-qt6.repo \
  "https://copr.fedorainfracloud.org/coprs/captsilver666/latte-dock-qt6/repo/fedora-$(rpm -E %fedora)/captsilver666-latte-dock-qt6-fedora-$(rpm -E %fedora).repo"
```

Install **by package name**, never from a downloaded `.rpm` file — that is the same trap as above.
Packages layered by name get re-resolved from the repo on every `rpm-ostree upgrade`.

To pick up new builds without doing it by hand:

```sh
sudo sed -i 's/^AutomaticUpdatePolicy=.*/AutomaticUpdatePolicy=stage/' /etc/rpm-ostreed.conf
sudo rpm-ostree reload
sudo systemctl enable --now rpm-ostreed-automatic.timer
```

New builds then download and stage into the next deployment; they go live on reboot. Note that
`uupd` and `bootc upgrade` will **not** refresh a layered RPM — they only fetch the base image.
Only `rpm-ostree upgrade` re-resolves layered packages.

### Stock Fedora

```sh
sudo dnf remove latte-dock          # only if you installed a hand-built RPM under the old name
sudo dnf copr enable captsilver666/latte-dock-qt6
sudo dnf install latte-dock-qt6
```

## Why take the COPR build over one you built yourself

Build the RPM somewhere whose Plasma is newer than the machine you install it on and it links
sonames that machine does not have. libksysguard did this to us: a build container tracking current
Fedora produced `liblattetasksplugin.so` linked against `libprocesscore.so.11` while the target
still shipped `.so.10`. The `latte-dock` binary itself was clean. The QML plugin just refused to
load, so the tasks applet came up with no backend — no window list, no launchers — and the symptom
reads like a D-Bus problem. Nothing points at a missing library.

A COPR build compiles against the exact Fedora release it is published for, and rpm's automatic
dependency generation writes the sonames it actually linked into the package's `Requires`. The same
mismatch then stops you at install time with a dependency error naming the library, instead of
failing silently at runtime.

## Build it yourself

```sh
git clone https://github.com/CaptSilver/latte-dock-qt6.git
cd latte-dock-qt6
sudo dnf builddep ./rpm/latte-dock-qt6.spec

# Packs the source tarball and stamps the version into the spec.
tools/scripts/make-srpm.sh --outdir /tmp/srpm

rpmbuild --define='_topdir /tmp/rpmbuild' --rebuild /tmp/srpm/*.src.rpm
```

COPR runs the same `make-srpm.sh`, so a local build takes the path a real one takes.

tar reads the working tree rather than HEAD, so a build from a checkout with uncommitted changes to
tracked files gets `.dirty` appended to its version — a package should not claim to be a commit it
isn't. COPR always builds a fresh clone and never trips this.

## Setting up the COPR project

One-time, with an API token from <https://copr.fedorainfracloud.org/api/> saved to `~/.config/copr`:

```sh
copr-cli create latte-dock-qt6 \
    --chroot fedora-44-x86_64 \
    --chroot fedora-45-x86_64 \
    --description "Latte Dock ported to Qt6 and KDE Plasma 6."

copr-cli add-package-scm latte-dock-qt6 \
    --name latte-dock-qt6 \
    --clone-url https://github.com/CaptSilver/latte-dock-qt6.git \
    --commit main \
    --spec rpm/latte-dock-qt6.spec \
    --type git \
    --method make_srpm \
    --webhook-rebuild on \
    --chroot-denylist 'opensuse-*,mageia-*,epel-*,centos-*,rhel-*,alma*,amazon*,oracle*'
```

Match the casing GitHub uses in the clone URL (`CaptSilver`, not `captsilver`). COPR compares the
URL from the webhook payload against this one as a plain string, and a case mismatch means every
push quietly rebuilds nothing.

`--chroot` enables exactly what you name — there is no default set to inherit. The denylist is
insurance for later: chroots can land on a project after the fact, and a Fedora KF6 spec cannot
build on openSUSE or EPEL, so you would just collect red builds and failure mail. Watch out for
`copr-cli modify --chroot`, which **replaces** the chroot list rather than adding to it; pass the
full set every time.

Then wire up the push notification. Take the URL from the project's Settings → Integrations page —
`https://copr.fedorainfracloud.org/webhooks/github/<project-id>/<secret>/latte-dock-qt6/` — and add
it to the GitHub repo as a webhook with content type `application/json` and push events. It is a
push notification, not CI: nothing runs the test suite on the way in, so that is still yours to do
before you push.

The package name on the end of that URL is not decoration. COPR works out which package a tag
refers to by parsing the tag as `PKGNAME-VERSION`, and `v0.10.78` gives it nothing to match on, so
it would rebuild nothing at all. Naming the package in the URL settles the question.

Two more things about tags, before you turn them on. GitHub only sets the field COPR reads for tags
on its *create* event, not on *push*, so a push-only webhook ignores tags outright. And once create
events are on, **any** tag reaching the fork builds — including the upstream Qt5 tags this fork
inherited, which sit hundreds of commits back and would publish an ancient tree under a version that
looks like a release. Cutting a release without any of that: push the tag, then start the build by
hand. It builds `main`, finds the tag on HEAD, and stamps the plain version.

```sh
copr-cli build-package latte-dock-qt6 --name latte-dock-qt6
```

## How COPR builds this

The source method is SCM with `--method make_srpm`: COPR runs `.copr/Makefile` in a mock chroot
with network, which is what lets it install git and rpm-build before packing the tarball. `%prep`
and `%build` are offline, so whatever the build needs has to be inside the tarball by then — for
this tree that is just the tracked files, there are no submodules. The default `rpkg` method would
handle the tarball fine, but it derives the version from tags and rpkg macros, and this fork's
versions come from neither.

### Versioning

A tagged commit builds as the plain version (`v0.10.78` → `0.10.78`). Everything else is a
post-release snapshot: `0.10.77^20260712.133354.g0059ef7` — the base version from `CMakeLists.txt`,
then the commit's date, its time, and the short hash. `^` marks a post-release, so a snapshot sorts
above `0.10.77` and below `0.10.78`.

The time of day is load-bearing. The usual Fedora snapshot form puts a commit count in that slot,
and the count does not survive here: COPR clones `--depth 500` and this tree has over six thousand
commits, so `git rev-list --count HEAD` returns 500 and goes on returning 500 forever. Two commits
on the same day would then differ only in the hash, which rpm compares as text — about half the
time the newer build sorts lower and dnf refuses it as a downgrade. That is the exact failure the
count exists to prevent, so it has to go. HHMMSS is monotonic, needs no tags, and does not care how
shallow the clone was.

**Tag and bump `set(VERSION)` in the same commit.** The two numbers come from different places —
snapshots read `CMakeLists.txt`, releases read the tag — and nothing keeps them in step. Tag
`v0.10.78` while `CMakeLists.txt` still says `0.10.77`, and every snapshot after it sorts *below*
the release: dnf pins everyone to `0.10.78` and never offers a snapshot again. `make-srpm.sh`
refuses to build that rather than let you publish it.

Version and release are written into the spec as literals before rpmbuild sees it, and so is the
changelog — the in-tree entry is a placeholder that exists only so `rpmspec -q` works on a plain
checkout, and the stamping step replaces it outright. The SRPM stores the spec verbatim and mock
re-parses it later, on a builder with no git checkout and a different clock, so anything left as
`%(...)` gets evaluated in the wrong place at the wrong time. `%{?dist}` is the one deliberate
exception: it names the chroot, so only the builder can expand it.

Two rpm habits worth knowing before you touch any of this. It never fails a build over a changelog
it dislikes — a date it cannot parse costs you every entry, an out-of-order one costs you everything
from the offender down, and either way it exits 0. `make-srpm.sh` asserts the entry it just
generated is present in the finished SRPM, because asking merely whether *a* changelog survived
walks straight past the second case. And rebuilding an unchanged commit yields a byte-identical EVR,
which dnf reads as "already have that" — so republishing a commit after a dependency bump in the
chroot needs `--release 2`, or nobody ever receives it.

