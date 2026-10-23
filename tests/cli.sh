#!/bin/sh
# Runs every command-line command against the built binary: plate_cli <binary>
set -eu

PLATE="$1"
DIR="$(mktemp -d)"
trap 'rm -rf "$DIR"' EXIT
export HOME="$DIR/home" XDG_DATA_HOME="$DIR/home/.local/share"
mkdir -p "$HOME/.claude"
FAILED=0

pass() { echo "PASS   : $1"; }
fail() { echo "FAIL!  : $1"; FAILED=1; }
check() { # name, command...
    name="$1"; shift
    if "$@" >"$DIR/out" 2>"$DIR/err"; then pass "$name"; else fail "$name"; cat "$DIR/err"; fi
}
refuses() { # name, command...: must exit 1 with a message on stderr
    name="$1"; shift
    if "$@" >"$DIR/out" 2>"$DIR/err"; then fail "$name (succeeded)"; elif [ -s "$DIR/err" ]; then pass "$name"; else fail "$name (no message)"; fi
}

check "new" "$PLATE" new "$DIR/a.plate" --unit "ng/mL"
refuses "new refuses to overwrite" "$PLATE" new "$DIR/a.plate"
refuses "new refuses an odd size" "$PLATE" new "$DIR/b.plate" --wells 100
check "new from the built-in template" "$PLATE" new "$DIR/elisa.plate" --template ELISA
refuses "new refuses a missing template" "$PLATE" new "$DIR/c.plate" --template Nope
refuses "new refuses --wells with --template" "$PLATE" new "$DIR/c.plate" --template ELISA --wells 384
check "templates" "$PLATE" templates --json
grep -q '"name": "ELISA"' "$DIR/out" && pass "templates lists ELISA" || fail "templates lists ELISA"

check "check a valid plate" "$PLATE" check "$DIR/elisa.plate" --json
grep -q '"valid": true' "$DIR/out" && pass "check says valid" || fail "check says valid"
printf '%s' '{"format":"plate","version":1,"rows":8,"columns":12,"wells":[{"well":"A1","type":1}]}' >"$DIR/bad.plate"
refuses "check reports a bad type" "$PLATE" check "$DIR/bad.plate"
refuses "check reports a missing file" "$PLATE" check "$DIR/missing.plate"

printf 'Sample ID\nP-001\nP-002\n"Smith, J"\n' >"$DIR/names.csv"
check "names" "$PLATE" names "$DIR/a.plate" --from "$DIR/names.csv" --at B3 --json
grep -q '"range": "B3:D3"' "$DIR/out" && pass "names fills B3:D3" || fail "names fills B3:D3"
refuses "names refuses a well off the plate" "$PLATE" names "$DIR/a.plate" --from "$DIR/names.csv" --at Z1

check "export CSV to standard output" "$PLATE" export "$DIR/a.plate" --csv -
grep -q '^D3,Sample,"Smith, J",' "$DIR/out" && pass "the CSV quotes a comma" || fail "the CSV quotes a comma"
[ "$(wc -l <"$DIR/out")" -eq 97 ] && pass "the CSV has every well" || fail "the CSV has every well"
check "export every format" "$PLATE" export "$DIR/elisa.plate" --csv "$DIR/e.csv" --svg "$DIR/e.svg" --pdf "$DIR/e.pdf" --json
[ -s "$DIR/e.csv" ] && [ -s "$DIR/e.svg" ] && head -c 5 "$DIR/e.pdf" | grep -q '%PDF' && pass "exports land" || fail "exports land"
refuses "export refuses an SVG to standard output" "$PLATE" export "$DIR/a.plate" --svg -
refuses "export refuses a plate with problems" "$PLATE" export "$DIR/bad.plate" --csv "$DIR/x.csv"

check "skill prints" "$PLATE" skill
check "skill install" "$PLATE" skill install --json
[ -L "$HOME/.claude/skills/plate" ] && [ -f "$HOME/.agents/skills/plate/SKILL.md" ] && pass "skill links into ~/.claude" || fail "skill links into ~/.claude"
check "help" "$PLATE" help

exit "$FAILED"
