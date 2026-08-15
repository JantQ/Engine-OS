#!/bin/bash
set -e

NAME=$1
DISK=${2:-disk.img}
OUT=esp/export/EFI/BOOT

if [ -z "$NAME" ]; then
    echo "usage: ./getgame.sh <gamename> [disk.img]"
    exit 1
fi

u8()  { od -An -tu1 -j "$1" -N1 "$DISK" | tr -d ' '; }
u32() { od -An -tu4 -j "$1" -N4 "$DISK" | tr -d ' '; }
u64() { od -An -tu8 -j "$1" -N8 "$DISK" | tr -d ' '; }
hexbytes() { od -An -tx1 -j "$1" -N "$2" "$DISK" | tr -d ' \n'; }
str() { tail -c +$(( $1 + 1 )) "$DISK" | head -c "$2" | tr -d '\0'; }

[ "$(str 512 8)" = "EFI PART" ] || { echo "getgame: no gpt on $DISK"; exit 1; }

ENTRYLBA=$(u64 $(( 512 + 72 )))
ECOUNT=$(u32 $(( 512 + 80 )))
ESIZE=$(u32 $(( 512 + 84 )))
GUID=a37e1f924c6b4d8a9f023d5c71e8b46a

BASE=""
i=0
while [ "$i" -lt "$ECOUNT" ]; do
    OFF=$(( ENTRYLBA * 512 + i * ESIZE ))
    if [ "$(hexbytes "$OFF" 16)" = "$GUID" ]; then
        BASE=$(( $(u64 $(( OFF + 32 ))) * 512 ))
        break
    fi
    i=$(( i + 1 ))
done
[ -n "$BASE" ] || { echo "getgame: no engine partition"; exit 1; }

[ "$(str "$BASE" 8)" = "ENFS0001" ] || { echo "getgame: no enfs volume"; exit 1; }
BS=$(u32 $(( BASE + 12 )))
TABLE=$(( BASE + BS ))

i=0
while [ "$i" -lt 64 ]; do
    E=$(( TABLE + i * 64 ))
    i=$(( i + 1 ))

    [ "$(u8 $(( E + 56 )))" = "1" ] || continue
    [ "$(str "$E" 40)" = "$NAME" ] || continue

    START=$(u64 $(( E + 40 )))
    LEN=$(u64 $(( E + 48 )))

    mkdir -p "$OUT"
    tail -c +$(( BASE + START * BS + 1 )) "$DISK" | head -c "$LEN" > "$OUT/$NAME"
    echo "copied $NAME ($LEN bytes) -> $OUT/$NAME"
    exit 0
done

echo "getgame: file not found: $NAME"
exit 1
