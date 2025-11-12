#!/bin/bash

declare -r SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR" || exit 1

CONFIG_FILE="dedcon_servers.conf"
DEFAULT_FILE="dedcon_default.conf"
GAME_LOG_DIR="game_logs"
RUNTIME_DIR=".dedcon_runtime"

mkdir -p "$RUNTIME_DIR"

if [[ ! -f "$CONFIG_FILE" ]]; then
    echo "missing $CONFIG_FILE next to $(basename "$0")"
    exit 1
fi

shopt -s extglob

declare -a server_names
declare -A server_command
declare -A server_config
declare -A server_event
declare -A server_processor
declare -A server_delay
declare -A server_extra
declare -A server_slug
declare -A slug_to_name
declare -A server_index

declare -A supervisor_pid
declare -A supervisor_pid_file

declare -A dedcon_pid_file

declare -A log_pid_file

declare -A log_pid

declare -a default_servers
declare -A default_lookup

lower_slug() {
    local text="$1"
    local lowered="${text,,}"
    local slug="${lowered//[^a-z0-9]/-}"
    while [[ "$slug" == *"--"* ]]; do
        slug="${slug//--/-}"
    done
    slug="${slug#-}"
    slug="${slug%-}"
    if [[ -z "$slug" ]]; then
        slug="srv"
    fi
    echo "$slug"
}

is_pid_alive() {
    local pid="$1"
    [[ -n "$pid" && "$pid" =~ ^[0-9]+$ ]] && kill -0 "$pid" 2>/dev/null
}

load_config() {
    local line idx=0
    while IFS= read -r line || [[ -n "$line" ]]; do
        line="${line%$'\r'}"
        if [[ -z "$line" || "${line:0:1}" == "#" ]]; then
            continue
        fi
        IFS='|' read -r name command config event processor delay extra <<< "$line"
        if [[ -z "$name" ]]; then
            continue
        fi
        server_names+=("$name")
        server_command["$name"]="$command"
        server_config["$name"]="$config"
        server_event["$name"]="$event"
        server_processor["$name"]="$processor"
        if [[ -z "$delay" ]]; then
            delay="10"
        fi
        server_delay["$name"]="$delay"
        server_extra["$name"]="$extra"
        local base_slug="$(lower_slug "$name")"
        local slug="$base_slug"
        local suffix=2
        while [[ -n "${slug_to_name[$slug]}" ]]; do
            slug="${base_slug}-${suffix}"
            ((suffix++))
        done
        server_slug["$name"]="$slug"
        slug_to_name["$slug"]="$name"
        ((idx++))
        server_index["$name"]="$idx"
        local loop_file="$RUNTIME_DIR/${slug}.loop.pid"
        local runner_file="$RUNTIME_DIR/${slug}.dedcon.pid"
        local log_file="$RUNTIME_DIR/${slug}.log.pid"
        supervisor_pid_file["$name"]="$loop_file"
        dedcon_pid_file["$name"]="$runner_file"
        log_pid_file["$name"]="$log_file"
        if [[ -f "$loop_file" ]]; then
            local pid
            pid="$(<"$loop_file")"
            if is_pid_alive "$pid"; then
                supervisor_pid["$name"]="$pid"
            else
                rm -f "$loop_file"
            fi
        fi
        if [[ -f "$runner_file" ]]; then
            local pid
            pid="$(<"$runner_file")"
            if ! is_pid_alive "$pid"; then
                rm -f "$runner_file"
            fi
        fi
        if [[ -f "$log_file" ]]; then
            local pid
            pid="$(<"$log_file")"
            if is_pid_alive "$pid"; then
                log_pid["$name"]="$pid"
            else
                rm -f "$log_file"
            fi
        fi
    done < "$CONFIG_FILE"
}

load_default_list() {
    default_servers=()
    default_lookup=()
    if [[ ! -f "$DEFAULT_FILE" ]]; then
        return
    fi
    local line
    while IFS= read -r line || [[ -n "$line" ]]; do
        line="${line%$'\r'}"
        line="${line##+([[:space:]])}"
        line="${line%%+([[:space:]])}"
        if [[ -z "$line" || "${line:0:1}" == "#" ]]; then
            continue
        fi
        if [[ -z "${server_command[$line]}" ]]; then
            echo "warning: default server '$line' missing from $CONFIG_FILE" >&2
            continue
        fi
        default_servers+=("$line")
        default_lookup["$line"]=1
    done < "$DEFAULT_FILE"
}

print_servers() {
    printf "%-3s  %-35s %-22s %-8s %-8s %-8s\n" "#" "name" "slug" "loop" "dedcon" "log"
    local i name
    for (( i = 0; i < ${#server_names[@]}; ++i )); do
        name="${server_names[$i]}"
        local slug="${server_slug[$name]}"
        local loop_state="-"
        local loop_file="${supervisor_pid_file[$name]}"
        if [[ -f "$loop_file" ]]; then
            local pid="$(<"$loop_file")"
            if is_pid_alive "$pid"; then
                loop_state="$pid"
            else
                loop_state="dead"
            fi
        fi
        local ded_state="-"
        local dfile="${dedcon_pid_file[$name]}"
        if [[ -f "$dfile" ]]; then
            local pid="$(<"$dfile")"
            if is_pid_alive "$pid"; then
                ded_state="$pid"
            else
                ded_state="dead"
            fi
        fi
        local log_state="-"
        local lfile="${log_pid_file[$name]}"
        if [[ -f "$lfile" ]]; then
            local pid="$(<"$lfile")"
            if is_pid_alive "$pid"; then
                log_state="$pid"
            else
                log_state="dead"
            fi
        fi
        printf "%3s  %-35s %-22s %-8s %-8s %-8s\n" "$((i+1))" "$name" "$slug" "$loop_state" "$ded_state" "$log_state"
    done
}

ensure_python() {
    if ! command -v python3 >/dev/null 2>&1; then
        echo "python3 not found in PATH"
        return 1
    fi
    return 0
}

start_log_processor() {
    local name="$1"
    local processor="${server_processor[$name]}"
    if [[ -z "$processor" || "$processor" == "-" ]]; then
        return 0
    fi
    if [[ ! -f "$processor" ]]; then
        echo "missing $processor for $name, skipping log watcher"
        return 0
    fi
    local log_file="${log_pid_file[$name]}"
    if [[ -f "$log_file" ]]; then
        local pid="$(<"$log_file")"
        if is_pid_alive "$pid"; then
            log_pid["$name"]="$pid"
            return 0
        else
            rm -f "$log_file"
        fi
    fi
    ensure_python || return 1
    mkdir -p "$GAME_LOG_DIR"
    local slug="${server_slug[$name]}"
    python3 "$processor" "${server_event[$name]}" "$GAME_LOG_DIR" >>"$RUNTIME_DIR/${slug}_processor.log" 2>&1 &
    local pid=$!
    log_pid["$name"]="$pid"
    echo "$pid" > "$log_file"
    echo "started log processor for $name (pid $pid)"
}

start_server() {
    local name="$1"
    if [[ -z "${server_command[$name]}" ]]; then
        echo "unknown server $name"
        return 1
    fi
    local loop_file="${supervisor_pid_file[$name]}"
    if [[ -f "$loop_file" ]]; then
        local existing="$(<"$loop_file")"
        if is_pid_alive "$existing"; then
            echo "$name already running (supervisor $existing)"
            return 0
        else
            rm -f "$loop_file"
        fi
    fi
    local command="${server_command[$name]}"
    if [[ "$command" == ./* ]]; then
        if [[ ! -x "$command" ]]; then
            echo "command $command not executable for $name"
            return 1
        fi
    else
        if ! command -v "$command" >/dev/null 2>&1; then
            echo "command $command not found for $name"
            return 1
        fi
    fi
    start_log_processor "$name"
    mkdir -p "$GAME_LOG_DIR"
    local -a extra_args=()
    if [[ -n "${server_extra[$name]}" ]]; then
        read -r -a extra_args <<< "${server_extra[$name]}"
    fi
    local ded_file="${dedcon_pid_file[$name]}"
    local delay="${server_delay[$name]}"
    (
        trap 'exit 0' SIGTERM SIGINT
        while true; do
            echo "[$(date '+%Y-%m-%d %H:%M:%S')] starting $name"
            "$command" "${server_config[$name]}" "${extra_args[@]}" &
            local child=$!
            echo "$child" > "$ded_file"
            wait "$child"
            local exit_code=$?
            rm -f "$ded_file"
            echo "[$(date '+%Y-%m-%d %H:%M:%S')] $name exited (code $exit_code). waiting $delay seconds"
            sleep "$delay"
        done
    ) &
    local loop_pid=$!
    echo "$loop_pid" > "$loop_file"
    supervisor_pid["$name"]="$loop_pid"
    echo "started $name supervisor (pid $loop_pid)"
}

stop_server() {
    local name="$1"
    local loop_file="${supervisor_pid_file[$name]}"
    local ded_file="${dedcon_pid_file[$name]}"
    local log_file="${log_pid_file[$name]}"
    local pid
    if [[ -f "$loop_file" ]]; then
        pid="$(<"$loop_file")"
        if is_pid_alive "$pid"; then
            kill "$pid" 2>/dev/null
            wait "$pid" 2>/dev/null
            echo "stopped supervisor for $name"
        fi
        rm -f "$loop_file"
    fi
    if [[ -f "$ded_file" ]]; then
        pid="$(<"$ded_file")"
        if is_pid_alive "$pid"; then
            kill "$pid" 2>/dev/null
            wait "$pid" 2>/dev/null
            echo "stopped running Dedcon instance for $name"
        fi
        rm -f "$ded_file"
    fi
    if [[ -f "$log_file" ]]; then
        pid="$(<"$log_file")"
        if is_pid_alive "$pid"; then
            kill "$pid" 2>/dev/null
            wait "$pid" 2>/dev/null
            echo "stopped log processor for $name"
        fi
        rm -f "$log_file"
    fi
    unset supervisor_pid["$name"]
    unset log_pid["$name"]
}

restart_server() {
    local name="$1"
    stop_server "$name"
    start_server "$name"
}

resolve_name() {
    local token="$1"
    if [[ -z "$token" ]]; then
        return 1
    fi
    local lower="${token,,}"
    if [[ "$lower" == "all" ]]; then
        echo "all"
        return 0
    fi
    if [[ "$token" =~ ^[0-9]+$ ]]; then
        local idx=$((token-1))
        if (( idx >= 0 && idx < ${#server_names[@]} )); then
            echo "${server_names[$idx]}"
            return 0
        fi
    fi
    local slug="$(lower_slug "$token")"
    if [[ -n "${slug_to_name[$slug]}" ]]; then
        echo "${slug_to_name[$slug]}"
        return 0
    fi
    for name in "${server_names[@]}"; do
        if [[ "${name,,}" == "$lower" ]]; then
            echo "$name"
            return 0
        fi
    done
    return 1
}

start_all() {
    local name
    for name in "${server_names[@]}"; do
        start_server "$name"
    done
}

start_default() {
    if [[ ${#default_servers[@]} -eq 0 ]]; then
        echo "no default servers listed in $DEFAULT_FILE"
        return 1
    fi
    local name
    local status=0
    for name in "${default_servers[@]}"; do
        start_server "$name" || status=1
    done
    return $status
}

stop_all() {
    local name
    for name in "${server_names[@]}"; do
        stop_server "$name"
    done
}

restart_all() {
    local name
    for name in "${server_names[@]}"; do
        restart_server "$name"
    done
}

follow_log() {
    local name="$1"
    local event="${server_event[$name]}"
    if [[ -z "$event" ]]; then
        echo "no event log configured for $name"
        return 1
    fi
    if [[ ! -f "$event" ]]; then
        echo "no log file at $event yet"
        return 1
    fi
    tail -f "$event"
}

help_text() {
    cat <<'EOF'
commands:
  list                     show all configured servers with ids + slugs
  status                   show supervisor/dedcon/log pids
  start <id|slug|name|all> start one server or all of them
  start default            start servers listed in dedcon_default.conf
  stop <id|slug|name|all>  stop one server or all of them
  restart <target>         restart target server(s)
  logs <target>            tail -f the Dedcon event log
  help                     show this help
  quit                     exit (servers keep running)

use slugs or numbers to avoid quoting long names.
EOF
}

handle_action() {
    local cmd="$1"
    local arg="$2"
    case "$cmd" in
        list)
            print_servers
            ;;
        status)
            print_servers
            ;;
        start)
            if [[ "${arg,,}" == "default" ]]; then
                start_default
                return $?
            fi
            local target="$(resolve_name "$arg")" || {
                echo "unknown target $arg"
                return 1
            }
            if [[ "$target" == "all" ]]; then
                start_all
            else
                start_server "$target"
            fi
            ;;
        stop)
            local target="$(resolve_name "$arg")" || {
                echo "unknown target $arg"
                return 1
            }
            if [[ "$target" == "all" ]]; then
                stop_all
            else
                stop_server "$target"
            fi
            ;;
        restart)
            local target="$(resolve_name "$arg")" || {
                echo "unknown target $arg"
                return 1
            }
            if [[ "$target" == "all" ]]; then
                restart_all
            else
                restart_server "$target"
            fi
            ;;
        logs)
            local target="$(resolve_name "$arg")" || {
                echo "unknown target $arg"
                return 1
            }
            if [[ "$target" == "all" ]]; then
                echo "logs command needs a single server"
                return 1
            fi
            follow_log "$target"
            ;;
        help)
            help_text
            ;;
        quit|exit)
            exit 0
            ;;
        *)
            echo "unknown command $cmd"
            help_text
            ;;
    esac
}

main_loop() {
    help_text
    print_servers
    while true; do
        printf 'dedcon> '
        local line
        IFS= read -r line || break
        line="${line##+([[:space:]])}"
        line="${line%%+([[:space:]])}"
        if [[ -z "$line" ]]; then
            continue
        fi
        local cmd="${line%% *}"
        local arg=""
        if [[ "$line" == *" "* ]]; then
            arg="${line#* }"
        fi
        handle_action "$cmd" "$arg"
    done
}

load_config
load_default_list
if [[ ${#server_names[@]} -eq 0 ]]; then
    echo "no servers defined in $CONFIG_FILE"
    exit 1
fi

if [[ $# -gt 0 ]]; then
    cmd="$1"
    shift
    arg="$*"
    handle_action "$cmd" "$arg"
    exit $?
fi

main_loop
