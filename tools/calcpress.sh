#!/bin/bash
# Press Calculator buttons by grid cell (window position resolved at runtime),
# then dump display lines. usage: calcpress.sh AC 7 mul 8 eq
cell_for() {
  case "$1" in
    BS|bs) echo "0 0";; AC) echo "1 0";; %) echo "2 0";; div|÷) echo "3 0";;
    7) echo "0 1";; 8) echo "1 1";; 9) echo "2 1";; mul|×|x) echo "3 1";;
    4) echo "0 2";; 5) echo "1 2";; 6) echo "2 2";; sub|-|−) echo "3 2";;
    1) echo "0 3";; 2) echo "1 3";; 3) echo "2 3";; add|+) echo "3 3";;
    pm|±) echo "0 4";; 0) echo "1 4";; ,|.) echo "2 4";; eq|=) echo "3 4";;
    *) echo "";;
  esac
}
winpos() {
  osascript <<'EOF' 2>/dev/null
tell application "System Events" to tell process "Calculator"
    set p to position of window 1
    return (item 1 of p) & " " & (item 2 of p)
end tell
EOF
}
osascript -e 'tell application "Calculator" to activate' >/dev/null 2>&1
sleep 0.3
for key in "$@"; do
  cell=$(cell_for "$key")
  [ -z "$cell" ] && { echo "unknown key: $key"; continue; }
  read wx wy <<< "$(winpos | tr -cs '0-9-' ' ')"
  [ -z "$wx" ] && { echo "no window"; exit 1; }
  set -- $cell; x=$((wx + 10 + $1 * 54 + 5)); y=$((wy + 133 + $2 * 54 + 5))
  osascript - $x $y <<'EOF' >/dev/null 2>&1
on run argv
    set x to item 1 of argv as integer
    set y to item 2 of argv as integer
    tell application "System Events" to tell process "Calculator"
        tell group 1 of group 1 of splitter group 1 of group 1 of window 1
            repeat with e in UI elements
                if role of e is "AXButton" then
                    set p to position of e
                    if ((item 1 of p) - x) ^ 2 + ((item 2 of p) - y) ^ 2 < 900 then
                        click e
                        exit repeat
                    end if
                end if
            end repeat
        end tell
    end tell
end run
EOF
  sleep 0.25
done
sleep 0.4
osascript <<'EOF' 2>/dev/null
tell application "System Events" to tell process "Calculator"
    set out to ""
    tell group 1 of group 1 of splitter group 1 of group 1 of window 1
        repeat with e in UI elements
            if role of e is "AXScrollArea" then
                repeat with txt in UI elements of e
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