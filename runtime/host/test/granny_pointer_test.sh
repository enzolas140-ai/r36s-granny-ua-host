#!/bin/sh
# Run only ua_host's pointer/cursor tests. The full toolkit script also runs unrelated GL arena tests.
set -e
cd "$(dirname "$0")/.."
docker run --rm --platform linux/arm64 -v "$PWD/..":/w -w /w/host ua-build-arm64 sh -c '
  set -e
  O=out/arm64/test
  mkdir -p "$O"
  gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -shared -fPIC -Wl,-soname,libGLESv2.so.2 -o "$O/libGLESv2.so.2" test/fake_gles.c
  gcc -O2 -g -Wall -Wextra -Wno-unused-parameter -I../jni -o "$O/test_pointer" test/test_pointer.c pointer.c "$O/libGLESv2.so.2" -ldl -lm
  for s in off mouse touch keymap swap dstick dstickauto toggle es2; do
    LD_LIBRARY_PATH="$O" "$O/test_pointer" "$s"
  done
'
