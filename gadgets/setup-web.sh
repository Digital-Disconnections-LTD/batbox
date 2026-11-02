#!/bin/bash
# Set up Batbox's optional web helpers without Docker:
#   Scrapling - web fetch and browser MCP server, installed with pipx or uv
#   SearXNG   - metasearch, in its own virtual environment, run as a user
#               service on 127.0.0.1 (launchd on macOS, systemd --user on Linux)
# Run it as `batbox setup web` or `bash gadgets/setup-web.sh`. Safe to re-run.
# Windows is not covered; see docs/connectors.md for a manual setup.
set -euo pipefail

# SearXNG has no versioned releases; this pins upstream master from 2026-10-25
# ("[fix] tagesschau: remove from general category (#6882)").
SEARXNG_COMMIT=54ca1d9cfb08674f66261d7f351b063558dfcc62
SEARXNG_VERSION=2026.10.25+54ca1d9cf
SEARXNG_ARCHIVE="https://github.com/searxng/searxng/archive/${SEARXNG_COMMIT}.tar.gz"
SETTINGS_MARKER='# Written by batbox setup web.'
SERVICE_LABEL=com.batbox.searxng
SYSTEMD_UNIT=batbox-searxng.service

port=8888
dry_run=false
uninstall=false
force=false
want_scrapling=true
want_searxng=true
browsers=true
merge_value=''
merge_requested=false

usage() {
  cat <<'EOF'
Usage: batbox setup web [options]

Installs Scrapling (pipx or uv) and a local SearXNG service. No Docker.

  --dry-run            Print the plan and change nothing
  --uninstall          Stop and remove the SearXNG service, its virtual
                       environment and its settings (Scrapling is left alone)
  --port N             SearXNG port (default 8888)
  --scrapling-only     Only set up Scrapling
  --searxng-only       Only set up SearXNG
  --no-browsers        Skip `scrapling install` (browser downloads); WebFetch
                       only needs HTTP
  --force              Replace a searxng/settings.yml this script did not write
  --write-scrapling-command PATH
                       Only record PATH as scrapling_command in
                       native-client.json (keeps every other key; makes a .bak)
  -h, --help           Show this help
EOF
}
fail() { echo "batbox setup web: $*" >&2; exit 2; }
say() { printf '%s\n' "$*"; }
step() { printf '\n== %s\n' "$*"; }

while (($#)); do
  case "$1" in
    --dry-run) dry_run=true ;;
    --uninstall) uninstall=true ;;
    --force) force=true ;;
    --port) (($# >= 2)) || fail '--port needs a number'; port=$2; shift ;;
    --port=*) port=${1#--port=} ;;
    --scrapling-only) want_searxng=false ;;
    --no-browsers) browsers=false ;;
    --searxng-only) want_scrapling=false ;;
    --write-scrapling-command)
      (($# >= 2)) || fail '--write-scrapling-command needs a path'
      merge_value=$2; merge_requested=true; shift ;;
    --write-scrapling-command=*) merge_value=${1#*=}; merge_requested=true ;;
    -h|--help) usage; exit 0 ;;
    *) usage >&2; fail "unknown option: $1" ;;
  esac
  shift
done
if ! [[ $port =~ ^[0-9]+$ ]] || ((port < 1 || port > 65535)); then fail "invalid port: $port"; fi
$want_scrapling || $want_searxng || fail 'nothing to do: both helpers were excluded'
[[ -n ${HOME:-} && $HOME = /* ]] || fail 'HOME must be an absolute directory'

os=$(uname -s)
case "$os" in
  Darwin|Linux) ;;
  *) fail "unsupported system: $os (macOS and Linux only; see docs/connectors.md)" ;;
esac

# Same lookup as Batbox: BATBOX_CONFIG_DIR, else ~/.batbox, with the older
# XDG client file used only when ~/.batbox has none.
if [[ -n ${BATBOX_CONFIG_DIR:-} ]]; then
  config_dir=$BATBOX_CONFIG_DIR
  [[ $config_dir = /* ]] || config_dir="$PWD/$config_dir"
  config_file="$config_dir/native-client.json"
else
  config_dir="$HOME/.batbox"
  config_file="$config_dir/native-client.json"
  legacy_file="${XDG_CONFIG_HOME:-$HOME/.config}/batbox/native-client.json"
  [[ ! -f $config_file && -f $legacy_file ]] && config_file=$legacy_file
fi
settings_file="$config_dir/searxng/settings.yml"
data_dir="${XDG_DATA_HOME:-$HOME/.local/share}/batbox/searxng"
venv="$data_dir/venv"
source_dir="$data_dir/src"
plist="$HOME/Library/LaunchAgents/$SERVICE_LABEL.plist"
log_dir="$HOME/Library/Logs/batbox"
unit_file="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user/$SYSTEMD_UNIT"
search_url="http://127.0.0.1:$port/search"

written=()
notes=()
failures=0

run() {
  if $dry_run; then say "  would run: $*"; else "$@"; fi
}
have() { command -v "$1" >/dev/null 2>&1; }
python_ok() {
  "$1" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 10) else 1)' 2>/dev/null
}

# Sets scrapling_command in the client file without touching any other key.
merge_scrapling_command() {
  local value=$1
  if $dry_run; then say "  would set scrapling_command in $config_file (backup: .bak)"; return; fi
  have python3 || fail 'python3 is needed to edit native-client.json safely'
  python3 -I - "$config_file" "$value" <<'PY'
import json, os, re, shutil, stat, sys, tempfile
path, value = sys.argv[1], sys.argv[2]
text, data = None, {}
if os.path.exists(path):
    with open(path, encoding="utf-8") as handle:
        text = handle.read()
    if text.strip():
        data = json.loads(text)
    if not isinstance(data, dict):
        sys.exit(f"{path} is not a JSON object; left untouched")
    if data.get("scrapling_command") == value:
        print(f"  scrapling_command already set in {path}")
        sys.exit(0)
    backup = path + ".bak"
    shutil.copy2(path, backup)
    os.chmod(backup, 0o600)
    print(f"  backup: {backup}")
else:
    os.makedirs(os.path.dirname(path) or ".", mode=0o700, exist_ok=True)
data["scrapling_command"] = value
indent = 2
if text:
    match = re.search(r"\{\s*\n([ \t]+)\S", text)
    if match:
        indent = match.group(1) if "\t" in match.group(1) else len(match.group(1))
directory = os.path.dirname(os.path.abspath(path))
handle, temporary = tempfile.mkstemp(prefix=".native-client.", dir=directory)
try:
    with os.fdopen(handle, "w", encoding="utf-8") as output:
        json.dump(data, output, indent=indent, ensure_ascii=False, sort_keys=False)
        output.write("\n")
    os.chmod(temporary, 0o600)
    os.replace(temporary, path)
except BaseException:
    os.unlink(temporary)
    raise
os.chmod(path, 0o600)
print(f"  set scrapling_command in {path}")
PY
  written+=("$config_file (scrapling_command; backup at $config_file.bak if it existed)")
}

if $merge_requested; then
  [[ -n $merge_value ]] || fail '--write-scrapling-command needs a non-empty path'
  merge_scrapling_command "$merge_value"
  exit 0
fi

# ---------------------------------------------------------------- uninstall
if $uninstall; then
  step 'Removing the SearXNG service'
  if [[ $os = Darwin ]]; then
    if launchctl print "gui/$(id -u)/$SERVICE_LABEL" >/dev/null 2>&1; then
      run launchctl bootout "gui/$(id -u)/$SERVICE_LABEL" || true
    fi
    [[ -f $plist ]] && run rm -f "$plist" && say "  removed $plist"
  elif have systemctl; then
    if [[ -f $unit_file ]]; then
      run systemctl --user disable --now "$SYSTEMD_UNIT" || true
      run rm -f "$unit_file"
      run systemctl --user daemon-reload || true
      say "  removed $unit_file"
    fi
  fi
  if [[ -d $data_dir ]]; then
    run rm -rf "$data_dir"
    run rmdir "$(dirname "$data_dir")" 2>/dev/null || true
    say "  removed $data_dir"
  fi
  for log in "$log_dir/searxng.stdout.log" "$log_dir/searxng.stderr.log"; do
    [[ -f $log ]] && run rm -f "$log"
  done
  [[ -d $log_dir ]] && run rmdir "$log_dir" 2>/dev/null || true
  if [[ -f $settings_file ]] && grep -qF "$SETTINGS_MARKER" "$settings_file"; then
    run rm -f "$settings_file"
    run rmdir "$(dirname "$settings_file")" 2>/dev/null || true
    say "  removed $settings_file"
  elif [[ -f $settings_file ]]; then
    say "  kept $settings_file (not written by this script)"
  fi
  say ''
  say 'Scrapling was left installed. To remove it:'
  say '  pipx uninstall scrapling    or    uv tool uninstall scrapling'
  say "If you set scrapling_command in $config_file, remove that key too."
  exit 0
fi

$dry_run && say 'Dry run: nothing will be changed.'

# ---------------------------------------------------------------- Scrapling
scrapling_path=''
# Prints the Scrapling version when the executable's Python has Scrapling and
# its MCP dependencies.
scrapling_mcp_version() {
  local interpreter
  interpreter=$(head -n 1 "$1" 2>/dev/null | sed -n 's/^#![[:space:]]*//p' | awk '{print $1}')
  [[ -n $interpreter && -x $interpreter ]] || return 1
  "$interpreter" -I -c 'import importlib.metadata as m, scrapling, mcp; print(m.version("scrapling"))' 2>/dev/null
}
install_scrapling() {
  step 'Scrapling'
  local installer='' bin_dir='' existing version
  existing=$(command -v scrapling 2>/dev/null || true)
  if [[ -n $existing ]] && version=$(scrapling_mcp_version "$existing"); then
    scrapling_path=$existing
    say "  already installed: $existing (Scrapling $version, MCP server available)"
  else
    if have pipx; then installer=pipx
    elif have uv; then installer=uv
    else
      say '  Neither pipx nor uv is installed. Install one, then re-run:'
      if [[ $os = Darwin ]]; then say '    brew install pipx && pipx ensurepath'
      else say '    python3 -m pip install --user pipx && python3 -m pipx ensurepath'; fi
      say '    (or uv: https://docs.astral.sh/uv/getting-started/installation/)'
      failures=$((failures + 1))
      return
    fi
    if [[ $installer = pipx ]]; then
      run pipx install 'scrapling[ai]'
      $dry_run || bin_dir=$(pipx environment --value PIPX_BIN_DIR 2>/dev/null || true)
      [[ -n $bin_dir ]] || bin_dir="${PIPX_BIN_DIR:-$HOME/.local/bin}"
    else
      run uv tool install 'scrapling[ai]'
      $dry_run || bin_dir=$(uv tool dir --bin 2>/dev/null || true)
      [[ -n $bin_dir ]] || bin_dir="${UV_TOOL_BIN_DIR:-$HOME/.local/bin}"
    fi
    scrapling_path="$bin_dir/scrapling"
    if $dry_run; then
      say "  would check that $scrapling_path has Scrapling's MCP server"
    elif version=$(scrapling_mcp_version "$scrapling_path"); then
      say "  OK: Scrapling $version with its MCP server at $scrapling_path"
    else
      say "  $scrapling_path lacks Scrapling's MCP server; reinstall with: $installer install --force 'scrapling[ai]'"
      failures=$((failures + 1))
      return
    fi
  fi
  if $browsers; then
    say '  installing browser dependencies (scrapling install; WebFetch itself only needs HTTP)'
    run "$scrapling_path" install
  else
    say '  skipped browser dependencies (--no-browsers); run later: scrapling install'
  fi
  # Batbox runs "scrapling" from PATH by default; record the path only when
  # this shell would find a different executable or none.
  local on_path
  on_path=$(command -v scrapling 2>/dev/null || true)
  if [[ -n $on_path ]] && { $dry_run || [[ $on_path -ef $scrapling_path ]]; }; then
    say '  scrapling is on PATH; no configuration change needed'
  else
    say "  $(dirname "$scrapling_path") is not on PATH; recording the full path for Batbox"
    merge_scrapling_command "$scrapling_path"
    notes+=("Add $(dirname "$scrapling_path") to PATH to run scrapling from your shell (pipx ensurepath).")
  fi
}

# ---------------------------------------------------------------- SearXNG
port_in_use() {
  python3 -I - "$1" <<'PY' 2>/dev/null
import socket, sys
with socket.socket() as s:
    s.settimeout(0.5)
    sys.exit(0 if s.connect_ex(("127.0.0.1", int(sys.argv[1]))) == 0 else 1)
PY
}
service_loaded() {
  if [[ $os = Darwin ]]; then launchctl print "gui/$(id -u)/$SERVICE_LABEL" >/dev/null 2>&1
  else have systemctl && systemctl --user is-active --quiet "$SYSTEMD_UNIT" 2>/dev/null; fi
}

write_settings() {
  local secret=''
  if [[ -f $settings_file ]]; then
    if grep -qF "$SETTINGS_MARKER" "$settings_file"; then
      secret=$(sed -n 's/^  secret_key: "\([0-9a-f]*\)"$/\1/p' "$settings_file" | head -n 1)
    elif ! $force; then
      fail "$settings_file exists and was not written by this script. Move it away or re-run with --force (it is backed up)."
    else
      run cp -p "$settings_file" "$settings_file.bak"
      say "  backup: $settings_file.bak"
    fi
  fi
  [[ -n $secret ]] || secret=$(python3 -I -c 'import secrets; print(secrets.token_hex(32))')
  if $dry_run; then say "  would write $settings_file (port $port, JSON on, 5 engines)"; return; fi
  mkdir -p "$(dirname "$settings_file")"
  local temporary
  temporary=$(mktemp "$settings_file.XXXXXX")
  cat >"$temporary" <<EOF
$SETTINGS_MARKER Batbox reads server.port and
# server.bind_address here to find this instance; edit freely, or remove it
# with: batbox setup web --uninstall
use_default_settings:
  engines:
    keep_only:
      - duckduckgo
      - brave
      - wikipedia
      - github
      - stackoverflow
general:
  debug: false
  instance_name: "Batbox SearXNG"
server:
  port: $port
  bind_address: "127.0.0.1"
  secret_key: "$secret"
  limiter: false
  public_instance: false
search:
  formats:
    - html
    - json
EOF
  chmod 600 "$temporary"
  mv "$temporary" "$settings_file"
  written+=("$settings_file (SearXNG settings; Batbox finds the instance here)")
}

install_searxng_code() {
  local python=''
  if have python3 && python_ok python3; then python=python3
  elif ! have uv; then
    fail 'SearXNG needs Python 3.10 or newer (python3), or uv to provide one'
  fi
  local fetch=''
  if have curl; then fetch='curl -fsSL -o'
  elif have wget; then fetch='wget -q -O'
  else fail 'curl or wget is needed to download SearXNG'; fi
  if [[ -f $source_dir/.batbox-commit && $(cat "$source_dir/.batbox-commit") = "$SEARXNG_COMMIT" ]]; then
    say "  source: $source_dir (commit ${SEARXNG_COMMIT:0:12}, already present)"
  else
    say "  downloading SearXNG ${SEARXNG_COMMIT:0:12}"
    if $dry_run; then
      say "  would download $SEARXNG_ARCHIVE into $source_dir"
    else
      mkdir -p "$data_dir"
      local download
      download=$(mktemp -d "$data_dir/download.XXXXXX")
      $fetch "$download/searxng.tar.gz" "$SEARXNG_ARCHIVE"
      mkdir "$download/tree"
      tar -xzf "$download/searxng.tar.gz" -C "$download/tree" --strip-components 1
      # No git checkout here, so give SearXNG its version directly.
      cat >"$download/tree/searx/version_frozen.py" <<EOF
VERSION_STRING = "$SEARXNG_VERSION"
VERSION_TAG = "$SEARXNG_VERSION"
DOCKER_TAG = "${SEARXNG_VERSION/+/-}"
GIT_URL = "https://github.com/searxng/searxng"
GIT_BRANCH = "master"
EOF
      printf '%s\n' "$SEARXNG_COMMIT" >"$download/tree/.batbox-commit"
      rm -rf "$source_dir" "$venv"
      mv "$download/tree" "$source_dir"
      rm -rf "$download"
    fi
  fi
  if [[ -x $venv/bin/searxng-run && -f $venv/.batbox-commit &&
        $(cat "$venv/.batbox-commit") = "$SEARXNG_COMMIT" ]]; then
    say "  virtual environment: $venv (up to date)"
    return
  fi
  say "  creating $venv"
  local -a pip
  if [[ -n $python ]]; then
    run "$python" -m venv "$venv"
    pip=("$venv/bin/python" -m pip install --quiet --disable-pip-version-check)
    run "${pip[@]}" -U pip
  else
    run uv venv --quiet --python 3.12 "$venv"
    pip=(uv pip install --quiet --python "$venv/bin/python")
  fi
  # SearXNG's documented non-container install: build helpers first, then an
  # editable install without build isolation.
  run "${pip[@]}" -U setuptools wheel pyyaml msgspec typing-extensions pybind11
  say '  installing SearXNG and its dependencies (a few minutes)'
  run "${pip[@]}" --use-pep517 --no-build-isolation -e "$source_dir"
  $dry_run || printf '%s\n' "$SEARXNG_COMMIT" >"$venv/.batbox-commit"
}

install_service() {
  if [[ $os = Darwin ]]; then
    say "  launchd agent: $SERVICE_LABEL"
    if $dry_run; then
      say "  would write $plist and run: launchctl bootstrap gui/$(id -u) $plist"
      return
    fi
    mkdir -p "$(dirname "$plist")" "$log_dir"
    cat >"$plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>$SERVICE_LABEL</string>
    <key>ProgramArguments</key>
    <array>
        <string>$venv/bin/searxng-run</string>
    </array>
    <key>WorkingDirectory</key>
    <string>$source_dir</string>
    <key>EnvironmentVariables</key>
    <dict>
        <key>SEARXNG_SETTINGS_PATH</key>
        <string>$settings_file</string>
        <key>PYTHONUNBUFFERED</key>
        <string>1</string>
    </dict>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
    <key>StandardOutPath</key>
    <string>$log_dir/searxng.stdout.log</string>
    <key>StandardErrorPath</key>
    <string>$log_dir/searxng.stderr.log</string>
</dict>
</plist>
EOF
    written+=("$plist (launchd agent $SERVICE_LABEL; logs in $log_dir)")
    launchctl bootout "gui/$(id -u)/$SERVICE_LABEL" >/dev/null 2>&1 || true
    for _ in 1 2 3 4 5; do
      launchctl bootstrap "gui/$(id -u)" "$plist" 2>/dev/null && return
      sleep 1
    done
    launchctl bootstrap "gui/$(id -u)" "$plist"
  else
    say "  systemd user unit: $SYSTEMD_UNIT"
    if $dry_run; then
      say "  would write $unit_file and run: systemctl --user enable --now $SYSTEMD_UNIT"
      return
    fi
    mkdir -p "$(dirname "$unit_file")"
    cat >"$unit_file" <<EOF
[Unit]
Description=SearXNG for Batbox (127.0.0.1:$port)

[Service]
ExecStart=$venv/bin/searxng-run
WorkingDirectory=$source_dir
Environment=SEARXNG_SETTINGS_PATH=$settings_file
Environment=PYTHONUNBUFFERED=1
Restart=on-failure
RestartSec=3

[Install]
WantedBy=default.target
EOF
    written+=("$unit_file (systemd user unit; logs: journalctl --user -u $SYSTEMD_UNIT)")
    if have systemctl && systemctl --user daemon-reload 2>/dev/null; then
      systemctl --user enable "$SYSTEMD_UNIT"
      systemctl --user restart "$SYSTEMD_UNIT"
    else
      say '  systemd --user is not available here; start SearXNG yourself with:'
      say "    SEARXNG_SETTINGS_PATH='$settings_file' '$venv/bin/searxng-run'"
      notes+=('SearXNG is installed but not running as a service (no systemd --user).')
    fi
  fi
}

verify_searxng() {
  if $dry_run; then say "  would wait for 127.0.0.1:$port and run one JSON search"; return; fi
  say "  waiting for 127.0.0.1:$port"
  for _ in $(seq 1 90); do
    port_in_use "$port" && break
    sleep 1
  done
  if ! port_in_use "$port"; then
    say "  SearXNG did not start; see its log:"
    if [[ $os = Darwin ]]; then say "    tail -n 50 $log_dir/searxng.stderr.log"
    else say "    journalctl --user -u $SYSTEMD_UNIT -n 50"; fi
    failures=$((failures + 1))
    return
  fi
  local result=''
  for _ in 1 2 3 4 5; do
    if result=$(curl -fsS --max-time 20 "$search_url?q=batbox&format=json" 2>/dev/null |
        python3 -I -c 'import json, sys; print(len(json.load(sys.stdin)["results"]))' 2>/dev/null); then
      break
    fi
    result=''
    sleep 2
  done
  if [[ -n $result ]]; then
    say "  OK: JSON search at $search_url returned $result results"
    ((result > 0)) || notes+=('SearXNG answered with 0 results; its engines may be rate-limited for a while.')
  else
    say "  SearXNG is listening but the JSON search failed: curl '$search_url?q=batbox&format=json'"
    failures=$((failures + 1))
  fi
}

setup_searxng() {
  step 'SearXNG'
  have python3 || have uv || fail 'python3 or uv is needed for SearXNG'
  if ! service_loaded && port_in_use "$port"; then
    fail "something else is listening on 127.0.0.1:$port; choose another port with --port N"
  fi
  install_searxng_code
  write_settings
  install_service
  verify_searxng
}

$want_scrapling && install_scrapling
$want_searxng && setup_searxng

step 'Summary'
if $dry_run; then
  say 'Dry run finished; nothing was changed.'
  exit 0
fi
if ((${#written[@]})); then
  say 'Written:'
  for item in "${written[@]}"; do say "  $item"; done
fi
if $want_searxng; then
  say "SearXNG: $search_url (found through $settings_file; no native-client.json change)"
  say "  code and virtual environment: $data_dir"
fi
[[ -n $scrapling_path ]] && say "Scrapling: $scrapling_path"
for note in "${notes[@]+"${notes[@]}"}"; do say "Note: $note"; done
say 'Undo with: batbox setup web --uninstall'
if ((failures)); then
  say "$failures step(s) need attention; see above."
  exit 1
fi
say 'Done. Restart Batbox and check /connectors.'
