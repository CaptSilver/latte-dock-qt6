#!/usr/bin/env bash
# Shared plumbing for the throwaway nested kwin_wayland sessions that the render gate, the
# widget e2e and the live coverage capture all need. Source it and call launch_nested_kwin;
# there is nothing to run here directly.
#
# Each of those harnesses used to carry its own copy of the launch line. Sizes and timeouts
# genuinely differ, so they are parameters -- but all three also carried the same broken ICD
# lookup, which is the kind of drift a single definition prevents.

# Absolute path of a Mesa Vulkan driver's ICD manifest, matched to this machine's architecture.
#
#   vulkan_icd lvp   ->  /usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#
# The obvious `ls ${driver}_icd.*.json | head -1` is what this replaces, and it is worse than
# it looks: Mesa ships the i686 and x86_64 manifests side by side, i686 sorts first, and a
# 64-bit process handed the 32-bit manifest does not get a missing-file error. The loader reads
# the manifest, finds no driver it can load, and vkCreateInstance returns
# ERROR_INCOMPATIBLE_DRIVER -- which reached the caller as "QVulkanInstance::create failed
# (err -9)" with nothing naming the ICD.
vulkan_icd() {
    local driver="$1"
    local dir="${VULKAN_ICD_DIR:-/usr/share/vulkan/icd.d}"
    local arch candidate
    arch="$(uname -m)"

    # The unsuffixed name is the fallback for distros that ship one manifest per driver.
    for candidate in "$dir/${driver}_icd.${arch}.json" "$dir/${driver}_icd.json"; do
        if [ -r "$candidate" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    echo "no $driver Vulkan ICD manifest for $arch under $dir" >&2
    return 1
}

# Run a session script under a throwaway nested kwin_wayland, and return what the session
# returned.
#
#   launch_nested_kwin [--width N] [--height N] [--timeout SECS] [--env K=V]... -- SESSION
#
# --env puts a variable on kwin's OWN process environment. That distinction matters: the
# compositor reads its permission flags there (KWIN_SCREENSHOT_NO_PERMISSION_CHECKS is checked
# by kwin's ScreenShot2 effect, not by the client asking for a shot), while anything the
# session needs belongs in the session script. Getting it backwards fails silently -- the
# screenshot is simply denied and the harness blames the product.
#
# Output is deliberately not captured. Buffering it to a tempfile and dumping it at the end
# means a two-minute e2e run says nothing at all while it works, and loses most of the trail
# if the timeout fires mid-run.
launch_nested_kwin() {
    local width=1280 height=800 timeout=120
    local -a outer_env=()

    while [ $# -gt 0 ]; do
        case "$1" in
            --width) width="$2"; shift 2;;
            --height) height="$2"; shift 2;;
            --timeout) timeout="$2"; shift 2;;
            --env) outer_env+=("$2"); shift 2;;
            --) shift; break;;
            *) echo "launch_nested_kwin: unknown option $1" >&2; return 2;;
        esac
    done

    local session="${1:-}"
    [ -x "$session" ] || { echo "launch_nested_kwin: no executable session script at '$session'" >&2; return 2; }

    local rt rc
    rt="$(mktemp -d /tmp/nested-kwin-xdg.XXXXXX)" || return 2
    chmod 700 "$rt"

    # --exit-with-session propagates the session's exit code verbatim, so this status is the
    # caller's answer. scriptguardtest pins that, because it is a compositor behaviour the
    # scripts depend on and cannot see.
    env XDG_RUNTIME_DIR="$rt" KWIN_WAYLAND_NO_PERMISSION_CHECKS=1 "${outer_env[@]}" \
        timeout "$timeout" dbus-run-session -- \
        kwin_wayland --virtual --width "$width" --height "$height" \
        --no-lockscreen --exit-with-session "$session"
    rc=$?

    # xdg-document-portal FUSE-mounts $XDG_RUNTIME_DIR/doc when the session activates it, and
    # rm cannot unlink a mountpoint. Without this, every run leaked its runtime dir and said so
    # as "rm: cannot remove '.../doc': Is a directory" -- an EISDIR that reads like a bug in the
    # cleanup rather than a mount still standing. Which helper exists varies: the distrobox has
    # only fusermount3, the host has both.
    local unmount
    for unmount in fusermount3 fusermount; do
        command -v "$unmount" >/dev/null 2>&1 || continue
        if "$unmount" -u "$rt/doc" 2>/dev/null; then
            break
        fi
    done
    rm -rf "$rt"

    return "$rc"
}
