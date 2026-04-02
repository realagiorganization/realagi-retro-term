#!/usr/bin/env bash
set -euo pipefail

ARTIFACTS_DIR="${1:?usage: $0 ARTIFACTS_DIR}"
TMUX_SOCKET="realagi-mouse-bdd"
TMUX_CONF="$ARTIFACTS_DIR/tmux.conf"

mkdir -p "$ARTIFACTS_DIR"
rm -f "$ARTIFACTS_DIR"/tmux-*.txt

export TERM="${TERM:-xterm-256color}"

capture_terminal_size() {
    local size
    for _ in $(seq 1 100); do
        size="$(stty size)"
        if [[ "$size" != "0 0" ]]; then
            printf '%s\n' "$size" > "$ARTIFACTS_DIR/terminal-size.txt"
            return 0
        fi
        sleep 0.1
    done
    printf '%s\n' "$size" > "$ARTIFACTS_DIR/terminal-size.txt"
    return 1
}

capture_terminal_size

tmux -L "$TMUX_SOCKET" kill-server >/dev/null 2>&1 || true

cat > "$TMUX_CONF" <<EOF
set -g mouse on
set -g status off
set -g history-limit 5000
bind-key -n F12 run-shell "tmux -L $TMUX_SOCKET list-panes -F '#{?pane_active,1,0} #{pane_id}' > '$ARTIFACTS_DIR/tmux-active-pane.txt'"
EOF

tmux -L "$TMUX_SOCKET" -f "$TMUX_CONF" new-session -d -s mousebdd \
  "bash -lc 'printf \"TOP PANE READY\n\"; seq 1 400 | sed \"s/^/TOP-/\"; exec bash'"
tmux -L "$TMUX_SOCKET" split-window -v -t mousebdd:0 \
  "bash -lc 'printf \"BOTTOM PANE READY\n\"; seq 1 400 | sed \"s/^/BOTTOM-/\"; exec bash'"
tmux -L "$TMUX_SOCKET" select-layout -t mousebdd:0 even-vertical
tmux -L "$TMUX_SOCKET" list-panes -F '#{pane_id} #{pane_left} #{pane_top} #{pane_width} #{pane_height}' \
  > "$ARTIFACTS_DIR/tmux-panes.txt"

exec tmux -L "$TMUX_SOCKET" attach-session -t mousebdd
