#!/bin/sh
# Host test of the agent's codec (zcbor, canonical) against rete cval goldens.
set -e
d=$(dirname "$0")
z=${ZCBOR:-$HOME/zephyr-esp32/modules/lib/zcbor}
f="-std=c11 -fsanitize=address,undefined -DZCBOR_CANONICAL -I$z/include"
# -Werror on our code only: zcbor's own sources carry warnings.
cc $f -Wall -Wextra -Werror -c -o /tmp/ztoit-agent-msg.o "$d/../msg.c"
cc $f -Wall -Wextra -Werror -c -o /tmp/ztoit-agent-msg-test.o "$d/msg_test.c"
cc $f -o /tmp/ztoit-agent-msg-test /tmp/ztoit-agent-msg.o /tmp/ztoit-agent-msg-test.o \
  "$z/src/zcbor_common.c" "$z/src/zcbor_encode.c" "$z/src/zcbor_decode.c"
/tmp/ztoit-agent-msg-test
