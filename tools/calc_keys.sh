#!/bin/bash
# Drive Calculator.app via keystrokes, then dump both display lines.
# usage: calc_keys.sh "keystrokes"  — special tokens: {esc} {del} {ret}
SEQ="$1"
tell="tell application \"System Events\" to tell process \"Calculator\""
osascript -e 'tell application "Calculator" to activate' >/dev/null 2>&1
sleep 0.4
# build keystroke script
script='tell application "System Events" to tell process "Calculator"'
i=0
while [ $i -lt ${#SEQ} ]; do
  ch="${SEQ:$i:1}"
  if [ "$ch" = "{" ]; then
    end=$((i+1)); tok=""
    while [ $end -lt ${#SEQ} ] && [ "${SEQ:$end:1}" != "}" ]; do tok="$tok${SEQ:$end:1}"; end=$((end+1)); done
    i=$end
    case "$tok" in
      esc) script="$script; key code 53";;
      del) script="$script; key code 51";;
      ret) script="$script; key code 36";;
      *) ;;
    esac
  else
    script="$script; keystroke \"$ch\""
  fi
  i=$((i+1))
done
script="$script; end tell"
osascript -e "$script" >/dev/null 2>&1
sleep 0.5
osascript <<'EOF' 2>/dev/null
tell application "System Events" to tell process "Calculator"
    set out to ""
    tell group 1 of group 1 of splitter group 1 of group 1 of window 1
        repeat with sa in UI elements
            if role of sa is "AXScrollArea" then
                repeat with txt in UI elements of sa
                    try
                        set out to out & "TEXT: " & (value of txt as text) & "\n"
                    end try
                end repeat
            end if
        end repeat
    end tell
    return out
end tell
EOF