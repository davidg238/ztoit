#!/bin/sh
# Host tests of the agent's plain-C parts: the codec (zcbor, canonical) against
# rete cval / porta wire goldens, staging (hash, ed25519, progress), and the
# table of app observations (obs.c).
set -e
d=$(dirname "$0")
z=${ZCBOR:-$HOME/zephyr-esp32/modules/lib/zcbor}
m=${MBEDTLS:-$HOME/zephyr-esp32/modules/crypto/mbedtls}
o=/tmp/ztoit-agent-test
mkdir -p $o
f="-std=c11 -fsanitize=address,undefined -DZCBOR_CANONICAL -I$z/include -I$m/include"
# -Werror on our code only: zcbor's, mbedTLS's and TweetNaCl's sources carry
# warnings, and TweetNaCl left-shifts negative values (as every compiler means it).
cc $f -Wall -Wextra -Werror -c -o $o/msg.o "$d/../msg.c"
cc $f -Wall -Wextra -Werror -c -o $o/msg_test.o "$d/msg_test.c"
cc $f -o $o/msg_test $o/msg.o $o/msg_test.o \
  "$z/src/zcbor_common.c" "$z/src/zcbor_encode.c" "$z/src/zcbor_decode.c"
cc $f -Wall -Wextra -Werror -c -o $o/stage.o "$d/../stage.c"
cc $f -Wall -Wextra -Werror -c -o $o/stage_test.o "$d/stage_test.c"
cc $f -fno-sanitize=shift -w -c -o $o/ed25519.o "$d/../ed25519.c"
cc $f -w -c -o $o/sha256.o "$m/library/sha256.c"
cc $f -w -c -o $o/platform_util.o "$m/library/platform_util.c"
cc $f -o $o/stage_test $o/stage.o $o/stage_test.o $o/ed25519.o $o/sha256.o $o/platform_util.o
cc $f -Wall -Wextra -Werror -o $o/obs_test "$d/../obs.c" "$d/obs_test.c"
$o/msg_test
$o/stage_test
$o/obs_test
