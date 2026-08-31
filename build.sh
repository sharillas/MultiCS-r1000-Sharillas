#!/bin/sh
# ============================================================
# MultiCS r1000 - Build Script (Linux, Zig cross-compile)
# Gera build/multics.x64 (x86_64 musl static)
# ============================================================
set -e
cd "$(dirname "$0")/src"

ZIG="${ZIG:-zig}"

OPTS="-DCHECK_NEXTDCW -DSID_FILTER -DNEWCACHE \
-DCCCAM_CLI -DRADEGAST_CLI -DCAMD35_CLI -DCS378X_CLI \
-DHTTP_SRV -DTELNET -DMGCAMD_SRV -DCCCAM_SRV \
-DCAMD35_SRV -DCS378X_SRV -DSRV_CSCACHE \
-DEXPIREDATE -DDCWSWAP -DCACHEEX -DIPLIST -DTESTCHANNEL \
-DTHREAD_DCW -DEPOLL_NEWCAMD -DEPOLL_CCCAM -DEPOLL_MGCAMD \
-DEPOLL_ECM -DPEERLIST -DECMLIST -DEPOLL_FREECCCAM -DSIG_HANDLER \
-DCLI_CSCACHE -DSRV_CSCACHE"

SRCS="sha1.c des.c md5.c aes.c dcw.c convert.c tools.c \
debug.c parser.c ipdata.c threads.c sockets.c \
msg-newcamd.c msg-cccam.c msg-radegast.c config.c \
ecmdata.c httpserver.c telnet.c main.c"

BUILDDIR="../build"
OBJDIR="$BUILDDIR/obj/x64"
mkdir -p "$OBJDIR" "$BUILDDIR"

OBJS=""
for s in $SRCS; do
  o="$OBJDIR/$(basename "$s" .c).o"
  $ZIG cc -target x86_64-linux-musl -O3 -fpack-struct -I. -std=gnu90 $OPTS -c "$s" -o "$o"
  OBJS="$OBJS $o"
done
$ZIG cc -target x86_64-linux-musl $OBJS -o "$BUILDDIR/multics.x64" -pthread -s
echo "multics.x64: $(stat -c%s "$BUILDDIR/multics.x64") bytes"
