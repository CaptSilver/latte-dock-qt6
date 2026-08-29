#!/usr/bin/env bash
#
# Build a self-contained source RPM.
#
# Two choices here are not the obvious ones, both for reasons that bite:
#
#   * Version, Release and the whole changelog are written into the spec as
#     literals before rpmbuild ever parses it.  The SRPM stores the spec
#     verbatim and mock re-parses it later on a builder with no git checkout and
#     a different clock, so anything left as %(...) gets re-evaluated in the
#     wrong place at the wrong time.  %{?dist} is the deliberate exception --
#     it names the chroot, so only the builder can expand it.
#
#   * A snapshot version carries the commit date AND time, not a commit count.
#     COPR clones --depth 500.  This tree is thousands of commits deep and the
#     build remote carries no tags, so `git rev-list --count HEAD` would pin
#     itself to 500 and never move again -- every build after the first would
#     compare EQUAL to the one before it, and dnf reads equal as "already have
#     it".  The package would freeze at whatever got built first and nobody
#     would see an error.  A timestamp needs no tags and does not care how deep
#     the clone is.
#
# .copr/Makefile calls this, so a local RPM build exercises exactly the path
# COPR takes.

set -euo pipefail

NAME=latte-dock-qt6

outdir=""
spec=""
release=1
allow_skew=0

usage() {
    cat <<'USAGE'
usage: make-srpm.sh [--outdir DIR] [--spec PATH] [--release N] [--allow-skew]

  --outdir DIR   where the .src.rpm and its tarball land
                 (default: <repo>/build/srpm)
  --spec PATH    spec template to stamp (default: <repo>/rpm/latte-dock-qt6.spec)
  --release N    release serial (default 1).  Bump it to republish the SAME
                 commit -- after a soname bump in the build chroot, say.  Leave
                 it alone and the rebuild carries an EVR identical to the build
                 before it, which dnf reads as "already have that" and skips.
  --allow-skew   build even though an ancestor of HEAD carries a later committer
                 date than HEAD.  The escape hatch for a bad clock buried in
                 history; the build it produces may sort below a published one.

COPR's make_srpm method invokes this via .copr/Makefile as:
  make -f .copr/Makefile srpm outdir="<dir>" spec="<path>"
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --outdir) outdir="$2"; shift 2 ;;
        --spec)   spec="$2";   shift 2 ;;
        --release) release="$2"; shift 2 ;;
        --allow-skew) allow_skew=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "make-srpm.sh: unknown argument '$1'" >&2; usage >&2; exit 2 ;;
    esac
done

[[ "$release" =~ ^[1-9][0-9]*$ ]] \
    || { echo "make-srpm.sh: --release must be a positive integer, got '$release'" >&2; exit 2; }

repo_root="$(git rev-parse --show-toplevel)"
cd "$repo_root"

: "${outdir:=$repo_root/build/srpm}"
: "${spec:=$repo_root/rpm/${NAME}.spec}"
mkdir -p "$outdir"
outdir="$(cd "$outdir" && pwd)"

[[ -f "$spec" ]] || { echo "make-srpm.sh: no spec at $spec" >&2; exit 1; }

# COPR clones --depth 500 and can land without tags, which makes `git describe`
# die with "There are no tags in the repo".  The make_srpm step has network, so
# just refill them.  Today the build remote carries none and this fetches
# nothing; it is here so that the first tag pushed builds as a release instead
# of as another snapshot.
git fetch --tags --quiet 2>/dev/null || true

# Everything below is derived from one committer timestamp, so the version
# string, the tarball mtime and the changelog entry cannot drift apart.
# Committer date, not author date: rebase and cherry-pick keep the original
# author date, so a freshly pushed commit could otherwise claim a version older
# than the build before it.
commit_epoch="$(git log -1 --format=%ct)"
[[ "$commit_epoch" =~ ^[0-9]+$ ]] \
    || { echo "make-srpm.sh: git gave no committer timestamp for HEAD" >&2; exit 1; }

# UTC throughout.  git's own --date=format: renders in the timezone recorded in
# the commit, so the same instant committed from two machines prints two
# different clock times and the string stops tracking the epoch it came from.
commit_stamp="$(date -u -d "@${commit_epoch}" +%Y%m%d.%H%M%S)"
commit_human="$(date -u -d "@${commit_epoch}" +'%Y-%m-%d %H:%M:%S UTC')"
short="$(git rev-parse --short=7 HEAD)"

# rpm compares the changelog date, and one it cannot parse makes it discard the
# WHOLE changelog and still exit 0.  Pin the locale.
changelog_date="$(LC_ALL=C date -u -d "@${commit_epoch}" +'%a %b %d %Y')"

if tag="$(git describe --tags --exact-match HEAD 2>/dev/null)"; then
    # A release build: 0.10.78, not 0.10.78^something.
    version="${tag#v}"
    changelog_note="Release ${version}"
else
    # There is no VERSION file; CMakeLists.txt holds the base version and the
    # build reads the very same line.
    base="$(sed -n 's/^[[:space:]]*set([[:space:]]*VERSION[[:space:]]\+"\?\([0-9][0-9.]*\)"\?[[:space:]]*).*/\1/p' CMakeLists.txt | head -1)"
    if [[ -z "$base" ]]; then
        echo "make-srpm.sh: no 'set(VERSION x.y.z)' in CMakeLists.txt" >&2
        echo "make-srpm.sh: refusing to build ${NAME}-^${commit_stamp}.g${short}" >&2
        exit 1
    fi

    # The base version and a release tag are two independent sources for the
    # same number and nothing keeps them in step.  Cut v0.10.78 without moving
    # set(VERSION), and every snapshot after it still says 0.10.77^... -- which
    # sorts BELOW the published 0.10.78, so dnf pins everyone to the release and
    # offers no snapshot ever again.  That is the same permanent, silent stall
    # the timestamp scheme exists to prevent, reached by a different road.  Tag
    # and bump set(VERSION) in the same commit.
    #
    # base EQUAL to the tag is the healthy state, not a fault: the tagged commit
    # built as the plain version, and rpm sorts ${base}^stamp ABOVE ${base}, so
    # the next snapshot lands just above the release exactly as intended.  Only
    # a base BELOW the newest tag is broken.
    last_tag="$(git describe --tags --abbrev=0 2>/dev/null || true)"
    if [[ -n "$last_tag" ]]; then
        tag_v="${last_tag#v}"
        oldest="$(printf '%s\n%s\n' "$base" "$tag_v" | sort -V | head -1)"
        if [[ "$base" != "$tag_v" && "$oldest" == "$base" ]]; then
            echo "make-srpm.sh: set(VERSION ${base}) is behind tag ${last_tag}." >&2
            echo "make-srpm.sh: ${base}^${commit_stamp} would sort below the published ${tag_v}," >&2
            echo "make-srpm.sh: so nobody on ${tag_v} would ever be offered it." >&2
            echo "make-srpm.sh: bump set(VERSION) in CMakeLists.txt to ${tag_v} or later." >&2
            exit 1
        fi
    fi

    # Why this sorts monotonically, checked with rpmdev-vercmp rather than
    # assumed: rpm splits a version on the non-alphanumerics and compares each
    # run of digits as a number, not as text -- it strips leading zeros, then
    # takes the longer run as the larger and only falls back to a character
    # compare when the lengths match.  Stripping zeros off a fixed-width HHMMSS
    # leaves the plain decimal, so that rule is exactly numeric order and 090500
    # lands below 120500 instead of above it.  The date segment decides first
    # and the hash is reached only when two commits share a second, where there
    # is no right answer anyway.  '^' marks a post-release: a snapshot sorts
    # above 0.10.77 and below 0.10.78.
    version="${base}^${commit_stamp}.g${short}"
    changelog_note="Snapshot of ${short}, committed ${commit_human}"
fi

# The whole scheme rests on HEAD being the newest commit on the branch by
# committer date.  A skewed clock or an explicit --date breaks that, and the
# damage is silent and permanent: the published build sorts BELOW one already in
# the repo, so dnf calls the upgrade a downgrade and nobody ever gets it.
newest="$(git log -500 --format=%ct | awk 'NR == 1 || $1 > m { m = $1 } END { print m + 0 }')"
if [[ "$newest" -gt "$commit_epoch" ]]; then
    echo "make-srpm.sh: an ancestor of HEAD is dated $((newest - commit_epoch))s later than HEAD." >&2
    echo "make-srpm.sh: ${version} would sort below anything built from that commit." >&2
    if [[ "$allow_skew" -eq 0 ]]; then
        echo "make-srpm.sh: commit again to carry HEAD's date past it, or pass --allow-skew." >&2
        exit 1
    fi
    echo "make-srpm.sh: --allow-skew given, publishing it anyway." >&2
fi

# tar reads the working tree, not HEAD, so uncommitted edits land in a tarball
# whose version names a clean commit.
#
# Only modified TRACKED files can do that.  The file list comes from the index,
# so untracked files are never packed -- and that distinction matters here
# because this tree is routinely built in-source, which leaves the checkout
# permanently full of cmake and ninja output.  A plain `git status --porcelain`
# would mark every local build dirty and mean nothing.
if [[ -n "$(git status --porcelain --untracked-files=no 2>/dev/null)" ]]; then
    version="${version}.dirty"
    changelog_note="${changelog_note} (uncommitted local changes)"
    echo "make-srpm.sh: tracked files are modified -- tagging this build .dirty" >&2
fi

# --transform rewrites symlink and hard-link targets as well as member names, so
# a tracked symlink would come out of the tarball pointing somewhere new.  There
# are none today; fail loudly rather than ship a quietly broken archive if that
# changes.
if git ls-files -s | awk '$1 == "120000" { found = 1 } END { exit !found }'; then
    echo "make-srpm.sh: tracked symlinks found -- --transform would rewrite their targets" >&2
    git ls-files -s | awk '$1 == "120000" { print "  " $4 }' >&2
    exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

echo "==> ${NAME}-${version}"

echo "==> packing source tarball"
topdir="${NAME}-${version}"
tarball="${outdir}/${topdir}.tar.gz"
# -z and --null are load-bearing: three tracked templates have spaces in their
# names, and a newline-delimited list would hand tar the fragments instead.
git ls-files -z \
    | tar --create --null --files-from=- \
          --transform="s,^,${topdir}/," \
          --owner=0 --group=0 --numeric-owner \
          --mtime="@${commit_epoch}" \
          --file="${work}/src.tar"
# -n drops the timestamp so the same commit always yields the same bytes.
gzip -n -9 < "${work}/src.tar" > "${tarball}"
printf '    %s (%s)\n' "$(basename "${tarball}")" "$(du -h "${tarball}" | cut -f1)"

echo "==> stamping spec"
stamped="${work}/${NAME}.spec"
sed -e "s|^Version:.*|Version: ${version}|" \
    -e "s|^Release:.*|Release: ${release}%{?dist}|" \
    "${spec}" > "${stamped}"

grep -qx '%changelog' "${stamped}" \
    || { echo "make-srpm.sh: ${spec} has no %changelog section to stamp" >&2; exit 1; }

# Replace the changelog outright rather than prepending to it.  The in-tree spec
# keeps one placeholder entry so `rpmspec -q` works on a plain checkout, but that
# entry's date is whatever was current when somebody wrote it -- newer than the
# commit being built as often as not.  rpm answers an out-of-order changelog by
# discarding every entry from the offender down and then exiting 0 anyway, so
# prepending ships a package missing most of its changelog and says nothing.
# One generated entry, dated from the commit, cannot be out of order with itself.
sed '/^%changelog$/q' "${stamped}" > "${stamped}.new"
printf '* %s packager - %s-%s\n- %s\n' \
    "${changelog_date}" "${version}" "${release}" "${changelog_note}" >> "${stamped}.new"
mv "${stamped}.new" "${stamped}"

echo "==> rpmbuild -bs"
rpmbuild -bs "${stamped}" \
    --define "_topdir ${work}/rpmbuild" \
    --define "_sourcedir ${outdir}" \
    --define "_srcrpmdir ${outdir}" \
    --define "_rpmdir ${work}/rpmbuild/RPMS"

# Match on the version just built rather than "newest file here" -- a reused
# outdir holds older builds, and picking one of those would report success for a
# package that was never produced.
shopt -s nullglob
srpms=("${outdir}/${topdir}"-*.src.rpm)
shopt -u nullglob
if [[ "${#srpms[@]}" -ne 1 ]]; then
    echo "make-srpm.sh: expected one ${topdir}-*.src.rpm in ${outdir}, found ${#srpms[@]}" >&2
    [[ "${#srpms[@]}" -eq 0 ]] || printf '  %s\n' "${srpms[@]}" >&2
    exit 1
fi
srpm="${srpms[0]}"

# rpm never fails a build over a changelog it dislikes; it drops entries and
# exits 0.  A date it cannot parse costs you the whole thing, an out-of-order one
# costs you everything from the offender down -- and that second case leaves
# CHANGELOGTIME populated, so asking merely whether a changelog exists sails
# straight past it.  Assert the entry just generated is the one that survived.
if ! rpm -qp --qf '%{CHANGELOGNAME}\n' "${srpm}" 2>/dev/null \
        | grep -qF -- "${version}-${release}"; then
    echo "make-srpm.sh: ${srpm} carries no changelog entry for ${version}-${release}" >&2
    echo "make-srpm.sh: rpm discarded it -- suspect a bad or out-of-order date" >&2
    rpm -qp --changelog "${srpm}" >&2 || true
    exit 1
fi

echo "==> ${srpm}"

