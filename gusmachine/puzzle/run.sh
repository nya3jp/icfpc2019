#!/bin/bash
# One line explanation of run.sh.
entry="$1"
input="../../lambda-client/blocks/${entry}/puzzle.cond"
result="results/${entry}/puzzle."
./solve-puzzle < "$input" > "$result"
