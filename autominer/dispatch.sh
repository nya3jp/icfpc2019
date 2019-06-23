#!/bin/bash

while :; do
    block=$(./lambda-cli.py getblockinfo block)
    blockdir="blocks/$block"
    seenfile="$blockdir/SEEN"
    if [[ ! -d "$blockdir" ]] || [[ -f "$seenfile" ]]; then
        sleep 3
        continue
    fi

    echo "New block $block"
    sleep 3

    (
        cd "$blockdir"
        tar cvzf block.tar.gz *
    )

    gsutil -m cp "$blockdir/*" "gs://sound-type-system/blocks/$block/" || continue

    ./mine.sh "$block" &

    touch $seenfile
done
