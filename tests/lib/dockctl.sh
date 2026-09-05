#!/usr/bin/env bash
# DBus plumbing for the harnesses that drive a live latte-dock. Sourced from inside the
# generated session scripts, so it runs under the nested compositor with the seeded HOME.

# Call a method on the running dock.
#
# Errors are not muted here. addApplet and triggerAppletAction print why they failed on
# stderr, and that line is the only clue when a harness stops at "add-failed (no new applet
# id)". Callers that expect a miss -- the read-only queries during a removal poll -- add
# 2>/dev/null at the call site.
dctl() {
    busctl --user call org.kde.lattedock /Latte org.kde.LatteDock "$@"
}

# Block until the dock owns its bus name, then give it time to build its views.
#
#   wait_for_dock <dock-pid> <tries> <settle-seconds>
#
# Returns 1 if it never appeared. A dead process short-circuits the wait: once the dock has
# crashed there is nothing left to appear, and burning the remaining tries only delays the
# log tail that says why.
wait_for_dock() {
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
