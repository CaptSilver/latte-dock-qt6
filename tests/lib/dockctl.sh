#!/usr/bin/env bash
# DBus plumbing for the harnesses that drive a live latte-dock. Sourced from inside the
# generated session scripts, so it runs under the nested compositor with the seeded HOME.

# Call a method on the running dock.
#
# Errors are not muted here. addApplet and triggerAppletAction print why they failed on
# stderr, and that line is the only clue when a harness stops at "add-failed (no new applet
# id)". Callers that expect a miss -- the read-only queries during a removal poll -- add
# 2>/dev/null at the call site.
# Refuse to run anywhere but inside the nested compositor. `busctl --user` resolves to
# whatever session bus the caller happens to have, and outside the harness that is the
# developer's own desktop -- so an accidental source-and-call drives their LIVE dock:
# saves their layout, opens their settings window. launch_nested_kwin exports this marker.
require_nested_session() {
    if [ -z "${LATTE_NESTED_SESSION:-}" ]; then
        echo "dockctl: refusing to talk to the session bus without LATTE_NESTED_SESSION -- outside the nested compositor this drives the real dock" >&2
        return 1
    fi
}

dctl() {
    require_nested_session || return 1
    busctl --user call org.kde.lattedock /Latte org.kde.LatteDock "$@"
}

# Block until the dock owns its bus name, then give it time to build its views.
#
# This poll cannot tell one dock from another: on the developer's own bus a dock is always
# present, so it returns success immediately and every dctl call after it drives that dock
# instead of the harness's. Hence the same refusal here.
#
#   wait_for_dock <dock-pid> <tries> <settle-seconds>
#
# Returns 1 if it never appeared. A dead process short-circuits the wait: once the dock has
# crashed there is nothing left to appear, and burning the remaining tries only delays the
# log tail that says why.
wait_for_dock() {
    require_nested_session || return 1

    local pid="$1" tries="$2" settle="$3"
    local i

    for ((i = 0; i < tries; i++)); do
        if busctl --user list 2>/dev/null | grep -q org.kde.lattedock; then
            sleep "$settle"
            return 0
        fi
        kill -0 "$pid" 2>/dev/null || return 1
        sleep 1
    done

    return 1
}
