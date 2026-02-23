#!/bin/bash
# hardening-check.sh <ircd_dir>
# Example: ./hardening-check.sh ~/unrealircd
# Exit codes: 0 = pass, 1 = fail
#
# This is used by BuildBot to make sure we use RELRO and CFI and such.
# Requirements: 'checksec' and 'readelf'
# It is AI-generated code (Claude Opus 4.6) but seems to work well,
# also verified to fail with a deliberately "bad" library.
# We only check libraries that we control, not system libs that are
# out of our control. Similarly, FreeBSD doesn't support CFI (CET)
# at the moment so we ignore it there, again.. out of our control.

if [ -z "$1" ]; then
    echo "Usage: $0 <unrealircd-directory>"
    exit 1
fi

IRCD_DIR="$1"
FAIL=0

# Collect all binaries to check
files=("$IRCD_DIR/bin/unrealircd")
while IFS= read -r f; do
    files+=("$f")
done < <(find "$IRCD_DIR/lib" -name '*.so*' -type f)

# --- checksec: Full RELRO, Canary, NX, FORTIFY ---
for f in "${files[@]}"; do
    out=$(checksec --format=csv --file="$f" 2>/dev/null)
    name=$(basename "$f")

    if ! echo "$out" | grep -qi "Full RELRO"; then
        echo "FAIL: $name — missing Full RELRO"
        FAIL=1
    fi
    if ! echo "$out" | grep -qi "Canary found"; then
        echo "FAIL: $name — missing stack canary"
        FAIL=1
    fi
    if ! echo "$out" | grep -qi "NX enabled"; then
        echo "FAIL: $name — missing NX"
        FAIL=1
    fi
    # FORTIFY column: check for "Yes" but not in other fields
    fortify=$(echo "$out" | awk -F',' '{print $8}')
    if [ "$fortify" != "Yes" ]; then
        echo "WARN: $name — no FORTIFY (may be OK for small libs)"
    fi
done

# --- CFI: CET on x86_64, BTI/PAC on aarch64 (Linux only) ---
if [ "$(uname -s)" = "Linux" ]; then
    ARCH=$(uname -m)
    if [ "$ARCH" = "x86_64" ]; then
        for f in "${files[@]}"; do
            name=$(basename "$f")
            props=$(readelf -n "$f" 2>/dev/null | grep "x86 feature:")
            if [ -z "$props" ]; then
                echo "FAIL: $name — no CET property note"
                FAIL=1
            else
                if ! echo "$props" | grep -q "IBT"; then
                    echo "FAIL: $name — missing IBT"
                    FAIL=1
                fi
                if ! echo "$props" | grep -q "SHSTK"; then
                    echo "FAIL: $name — missing SHSTK"
                    FAIL=1
                fi
            fi
        done
    elif [ "$ARCH" = "aarch64" ]; then
        for f in "${files[@]}"; do
            name=$(basename "$f")
            props=$(readelf -n "$f" 2>/dev/null | grep "aarch64 feature:")
            if [ -z "$props" ]; then
                echo "FAIL: $name — no BTI/PAC property note"
                FAIL=1
            else
                if ! echo "$props" | grep -q "BTI"; then
                    echo "FAIL: $name — missing BTI"
                    FAIL=1
                fi
                if ! echo "$props" | grep -q "PAC"; then
                    echo "FAIL: $name — missing PAC"
                    FAIL=1
                fi
            fi
        done
    fi
fi

if [ "$FAIL" -eq 1 ]; then
    echo "HARDENING CHECK FAILED"
    exit 1
else
    echo "All hardening checks passed."
    exit 0
fi
