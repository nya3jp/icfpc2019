#!/bin/bash

while :; do
    block=$(./lambda-cli.py getblockinfo block)
    blockdir="blocks/$block"
    seenfile="$blockdir/SEEN"
    if [[ -f "$seenfile" ]]; then
        sleep 3
        continue
    fi

    echo "New block $block"

    (
        cd "$blockdir"
        tar cvzf block.tar.gz *
    )

    gsutil -m cp "$blockdir/*" "gs://sound-type-system/blocks/$block/" || continue

    nohup ./block.sh "$block" > "$blockdir/block.stdout.txt" 2> "$blockdir/block.stderr.txt" &
    disown

    touch $seenfile
done
