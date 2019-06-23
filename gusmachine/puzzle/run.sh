#!/bin/bash
# One line explanation of run.sh.
entry="$1"
input="../../lambda-client/blocks/${entry}/puzzle.cond"
result="results/${entry}/puzzle.desc"
mkdir -p results/${entry}
./solve-puzzle < "$input" > "$result"
nodejs ../../puzzle_checker/check.js "$input" "$result"
