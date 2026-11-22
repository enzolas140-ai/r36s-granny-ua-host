#!/usr/bin/env bash
# Granny 1.7.9 - dedicated ua_host64 / R36S
set +u
PORTS_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)" || exit 1
GAMEDIR="$PORTS_DIR/granny"
[ -d "$GAMEDIR" ] || exit 2
cd "$GAMEDIR" || exit 2
mkdir -p "$GAMEDIR/logs" "$GAMEDIR/data" "$GAMEDIR/cache"
exec >>"$GAMEDIR/logs/launcher.log" 2>&1

echo
echo "===== Granny ua_host64 launcher ====="
date
echo "script=$PORTS_DIR/Granny.sh"
echo "kernel=$(uname -a)"
echo "arch=$(uname -m)"
set -x

controlfolder=""
for p in "$PM_DIR" /opt/system/Tools/PortMaster /opt/tools/PortMaster "$HOME/.local/share/PortMaster" /storage/.config/PortMaster /var/data/PortMaster /PortMaster /roms/tools/PortMaster /roms2/tools/PortMaster /roms/ports/PortMaster /roms2/ports/PortMaster /storage/roms/ports/PortMaster; do
  if [ -n "$p" ] && [ -r "$p/control.txt" ]; then controlfolder="$p"; break; fi
done
if [ -n "$controlfolder" ]; then
  PM_ROOT="$controlfolder"
  . "$PM_ROOT/control.txt"
  controlfolder="$PM_ROOT"
  [ ! -r "$controlfolder/device_info.txt" ] || . "$controlfolder/device_info.txt"
  case "$CFW_NAME" in ''|*[!A-Za-z0-9._-]*) ;; *) [ ! -r "$controlfolder/mod_$CFW_NAME.txt" ] || . "$controlfolder/mod_$CFW_NAME.txt" ;; esac
  [ ! -r "$controlfolder/funcs.txt" ] || . "$controlfolder/funcs.txt"
  command -v get_controls >/dev/null 2>&1 && get_controls
else
  echo "WARNING: PortMaster control.txt not found"
fi
[ -n "$CUR_TTY" ] || CUR_TTY=/dev/tty0
[ -n "$ESUDO" ] || ESUDO=""
if [ -z "$GPTOKEYB" ] && [ -n "$controlfolder" ] && [ -x "$controlfolder/gptokeyb" ]; then
  GPTOKEYB="$controlfolder/gptokeyb"
fi

for f in "$GAMEDIR/ua_host64" "$GAMEDIR/base.apk" "$GAMEDIR/android_package.json" "$GAMEDIR/lib/libmain.so" "$GAMEDIR/lib/libunity.so" "$GAMEDIR/lib/libil2cpp.so" "$GAMEDIR/granny-dualtouch" "$GAMEDIR/granny-pointer.env"; do
  if [ ! -f "$f" ]; then
    echo "ERROR: missing required file: $f"
    command -v pm_message >/dev/null 2>&1 && pm_message "Granny: arquivo ausente."
    command -v pm_finish >/dev/null 2>&1 && pm_finish
    exit 3
  fi
done

chmod +x "$GAMEDIR/ua_host64" "$GAMEDIR/granny-dualtouch" 2>/dev/null || true
export MALLOC_ARENA_MAX=2 MALLOC_TRIM_THRESHOLD_=131072 MALLOC_MMAP_THRESHOLD_=131072 SDL_GAMECONTROLLER_USE_BUTTON_LABELS=0
[ -z "$sdl_controllerconfig" ] || export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
export HOME="$GAMEDIR/data/home" XDG_DATA_HOME="$GAMEDIR/data" XDG_CONFIG_HOME="$GAMEDIR/data/config" XDG_CACHE_HOME="$GAMEDIR/cache"
mkdir -p "$HOME" "$XDG_CONFIG_HOME"

# Pointer starts hidden: the mapper supplies two Android touch contacts until R3 is pressed.
set -a
. "$GAMEDIR/granny-pointer.env"
set +a

if command -v pm_platform_helper >/dev/null 2>&1; then
  pm_platform_helper "$GAMEDIR/ua_host64"
  echo "pm_platform_helper exit=$?"
else
  echo "WARNING: pm_platform_helper not available"
fi

mapper_pid=""
mapper_pidfile="$GAMEDIR/data/granny-dualtouch.pid"
gptk_pid=""
finish() {
  rc=$?
  trap - EXIT INT TERM HUP
  [ -z "$gptk_pid" ] || kill "$gptk_pid" 2>/dev/null || true
  if [ -f "$mapper_pidfile" ]; then
    touch_pid=$(cat "$mapper_pidfile" 2>/dev/null)
    if [ -n "$touch_pid" ]; then
      if [ -n "$ESUDO" ]; then $ESUDO kill -TERM "$touch_pid" 2>/dev/null || true; else kill -TERM "$touch_pid" 2>/dev/null || true; fi
    fi
    rm -f "$mapper_pidfile"
  fi
  [ -z "$mapper_pid" ] || kill -TERM "$mapper_pid" 2>/dev/null || true
  dmesg 2>/dev/null | tail -100 >"$GAMEDIR/logs/dmesg.log" 2>&1 || true
  command -v pm_finish >/dev/null 2>&1 && pm_finish
  echo "Granny ua_host64 exit=$rc"
  exit "$rc"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'exit 129' HUP

if [ -n "$ESUDO" ]; then
  $ESUDO sh -c '"$1" & child=$!; echo "$child" > "$2"; wait "$child"' sh "$GAMEDIR/granny-dualtouch" "$mapper_pidfile" >"$GAMEDIR/logs/dualtouch.log" 2>&1 &
else
  "$GAMEDIR/granny-dualtouch" >"$GAMEDIR/logs/dualtouch.log" 2>&1 &
fi
mapper_pid=$!
# Give the privileged mapper a moment to create its virtual input devices.
# Do not test/wait on the sudo wrapper PID here: PortMaster may reap that
# wrapper while the root-owned mapper child is still alive. That false
# negative previously prevented ua_host64 from ever launching.
sleep 1
echo "Granny DualTouch launch requested; supervisor PID=$mapper_pid"
if [ -n "$ESUDO" ] && [ -s "$mapper_pidfile" ]; then
  echo "Granny DualTouch child PID=$(cat "$mapper_pidfile" 2>/dev/null)"
fi

if [ -n "$GPTOKEYB" ] && [ -f "$GAMEDIR/granny_hotkey.gptk" ]; then
  $GPTOKEYB "ua_host64" -c "$GAMEDIR/granny_hotkey.gptk" &
  gptk_pid=$!
  echo "GPTOKEYB PID=$gptk_pid"
else
  echo "WARNING: GPTOKEYB unavailable; Select+Start exit hotkey may be unavailable"
fi

echo "Launching Granny with dedicated ua_host64"
"$GAMEDIR/ua_host64" "$GAMEDIR" >"$GAMEDIR/logs/ua_host.log" 2>&1
rc=$?
echo "ua_host64 returned status=$rc"
exit "$rc"
