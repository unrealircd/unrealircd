#!/bin/sh
# Entrypoint for the ObbyIRCd Docker image.
#
# On first start (when the conf volume is empty) the conf is generated
# from /etc/obbyircd/obbyircd.conf.template by substituting environment
# variables, a self-signed TLS cert is issued (1 day, testing only) and
# random cloak keys are produced.  On subsequent starts the existing
# state is left alone.
#
# Drop *.c files into the custom-modules volume to have them compiled
# at startup against this image's source tree and installed as
# third/<name>.so.
#
set -eu

export SERVER_NAME="${SERVER_NAME:-irc.example.com}"
export NETWORK_NAME="${NETWORK_NAME:-ObbyNetwork}"
export ADMIN_EMAIL="${ADMIN_EMAIL:-admin@example.com}"
export SSL_PORT="${SSL_PORT:-6697}"
export WS_PORT="${WS_PORT:-8080}"
export RPC_PORT="${RPC_PORT:-8600}"
export RPC_PASSWORD="${RPC_PASSWORD:-}"
export FILEHOST_URL="${FILEHOST_URL:-}"
export MOTD_TEXT="${MOTD_TEXT:-Welcome to ObbyIRCd!}"
export OPER_NAME="${OPER_NAME:-admin}"
export OPER_PASSWORD="${OPER_PASSWORD:-}"
# OPER_MASK restricts where the oper can authenticate from.  Default *
# is permissive; override for production deployments.
export OPER_MASK="${OPER_MASK:-*}"

CONF_DIR="/home/obbyircd/obby/conf"
DATA_DIR="/home/obbyircd/obby/data"
TLS_DIR="/home/obbyircd/obby/tls"
LOGS_DIR="/home/obbyircd/obby/logs"
TEMPLATE_FILE="/etc/obbyircd/obbyircd.conf.template"
CONFIG_FILE="$CONF_DIR/obbyircd.conf"
CUSTOM_MOD_DIR="/home/obbyircd/obby/custom-modules"
SOURCE_TREE="/tmp/obbyircd-source"
export TLS_DIR

echo "Starting ObbyIRCd container..."
echo "Server:  $SERVER_NAME"
echo "Network: $NETWORK_NAME"

# WebSocket listener block (always emitted; use plaintext in container,
# put a TLS-terminating reverse proxy in front for production).
if [ -n "$WS_PORT" ]; then
    export WS_CONFIG="loadmodule \"webserver\";
loadmodule \"websocket\";
listen {
    ip *;
    port $WS_PORT;
    options { websocket { type text; }; };
};"
else
    export WS_CONFIG=""
fi

# Optional filehost block.
if [ -n "$FILEHOST_URL" ]; then
    export FILEHOST_CONFIG="filehosts { host \"$FILEHOST_URL\"; };"
    echo "Filehost: $FILEHOST_URL"
else
    export FILEHOST_CONFIG=""
fi

# Optional JSON-RPC.  Off by default; set RPC_PASSWORD to enable.
if [ -n "$RPC_PASSWORD" ]; then
    export RPC_CONFIG="include \"rpc.modules.default.conf\";
listen { ip *; port $RPC_PORT; options { rpc; }; };
rpc-user admin { match { ip *; } rpc-class full; password \"$RPC_PASSWORD\"; };"
    echo "RPC: enabled on port $RPC_PORT"
else
    export RPC_CONFIG=""
fi

# Random cloak keys if the operator didn't supply any.  Operators
# running multiple servers should set CLOAK_KEY1/2/3 in .env so all
# nodes agree.
if [ -z "${CLOAK_KEY1:-}" ] || [ -z "${CLOAK_KEY2:-}" ] || [ -z "${CLOAK_KEY3:-}" ]; then
    echo "Generating random cloak keys (set CLOAK_KEY1/2/3 in .env to fix them)"
    CLOAK_KEY1=$(head -c 32 /dev/urandom | od -An -tx1 | tr -d ' \n')
    CLOAK_KEY2=$(head -c 32 /dev/urandom | od -An -tx1 | tr -d ' \n')
    CLOAK_KEY3=$(head -c 32 /dev/urandom | od -An -tx1 | tr -d ' \n')
fi
export CLOAK_KEY1 CLOAK_KEY2 CLOAK_KEY3

# TLS cert: idempotent.  Lives in its own volume, so a fresh tls
# volume must self-heal even when conf is already initialised.
if [ ! -f "$TLS_DIR/server.cert.pem" ] || [ ! -f "$TLS_DIR/server.key.pem" ]; then
    echo "Issuing a self-signed TLS cert (valid 1 day -- replace with a real one for production)"
    openssl req -x509 -nodes -newkey rsa:2048 -days 1 \
        -keyout "$TLS_DIR/server.key.pem" \
        -out "$TLS_DIR/server.cert.pem" \
        -subj "/CN=$SERVER_NAME" \
        >/dev/null 2>&1
    chmod 600 "$TLS_DIR/server.key.pem"
    chmod 644 "$TLS_DIR/server.cert.pem"
fi

# Oper password: required.  Generate a random one on first run if the
# operator didn't supply one and persist it so subsequent restarts
# don't change behind their back.  This avoids ever shipping a known
# default like "admin123".
OPER_PASSWORD_FILE="$DATA_DIR/.oper_password"
if [ -z "$OPER_PASSWORD" ]; then
    if [ -f "$OPER_PASSWORD_FILE" ]; then
        OPER_PASSWORD=$(cat "$OPER_PASSWORD_FILE")
        echo "Loaded oper password from $OPER_PASSWORD_FILE"
    else
        OPER_PASSWORD=$(head -c 18 /dev/urandom | od -An -tx1 | tr -d ' \n')
        printf '%s' "$OPER_PASSWORD" > "$OPER_PASSWORD_FILE"
        chmod 600 "$OPER_PASSWORD_FILE"
        echo ""
        echo "==========================================================="
        echo "Generated oper credentials (saved to $OPER_PASSWORD_FILE):"
        echo "  /OPER $OPER_NAME $OPER_PASSWORD"
        echo "Set OPER_PASSWORD in .env to a fixed value for stable creds."
        echo "==========================================================="
        echo ""
    fi
fi
export OPER_PASSWORD

FIRST_RUN_MARKER="$CONF_DIR/.docker_initialized"
if [ ! -f "$FIRST_RUN_MARKER" ]; then
    echo "Empty conf volume detected -- populating from /etc/obbyircd/conf-defaults/"
    cp -r /etc/obbyircd/conf-defaults/. "$CONF_DIR/"

    # Render to a temp file first; promote only if validation passes
    # below.  Without this, a bad render would still create the
    # marker and lock the operator out of re-rendering after fixing
    # the offending env var.
    envsubst < "$TEMPLATE_FILE" > "$CONFIG_FILE.tmp"
    mv "$CONFIG_FILE.tmp" "$CONFIG_FILE"

    chown -R obbyircd:obbyircd "$CONF_DIR" "$TLS_DIR" "$DATA_DIR" "$LOGS_DIR"
    echo "Generated configuration (validation pending)"
else
    echo "Reusing existing config (delete $FIRST_RUN_MARKER to regenerate)"
fi

# obbyscript autoloads every *.js it finds in $CONFDIR/scripts/.
# Make sure the directory exists so the module doesn't log a warning
# on every restart even when the operator hasn't placed any scripts.
mkdir -p "$CONF_DIR/scripts"

# Always make sure the unprivileged user owns the runtime dirs --
# fixes the case where someone bind-mounts a host directory the first
# time and root-owned conf files get created during a previous run.
chown obbyircd:obbyircd "$DATA_DIR" "$LOGS_DIR" "$TLS_DIR" 2>/dev/null || true
chown obbyircd:obbyircd "$CONF_DIR/scripts" 2>/dev/null || true
chown obbyircd:obbyircd "$OPER_PASSWORD_FILE" 2>/dev/null || true

# Compile any user-dropped custom modules.  Failures are logged and
# the server still starts.
if [ -d "$CUSTOM_MOD_DIR" ]; then
    for src in "$CUSTOM_MOD_DIR"/*.c; do
        [ -f "$src" ] || continue
        modname=$(basename "$src" .c)
        out="/home/obbyircd/obby/modules/third/${modname}.so"
        echo "Compiling custom module: $modname"
        if su-exec obbyircd gcc -shared -fPIC -DPIC -DDYNAMIC_LINKING \
            -Wl,-export-dynamic -Wl,-z,relro -Wl,-z,now \
            -o "$out" "$src" \
            -I"$SOURCE_TREE/include" -I"$SOURCE_TREE" \
            $(pkg-config --cflags openssl 2>/dev/null || true); then
            echo "  -> $out"
        else
            echo "  WARNING: $modname failed to compile, skipping"
        fi
    done
fi

cd /home/obbyircd/obby

echo "Validating configuration..."
if ! su-exec obbyircd ./bin/obbyircd -c; then
    echo "ERROR: configuration validation failed (see output above)"
    echo "Fix the offending environment variable and restart -- the"
    echo "container will re-render the conf because no init marker has"
    echo "been written yet."
    exit 1
fi

# Validation passed; promote first-run marker so we don't re-render
# the rendered conf on every restart.
if [ ! -f "$FIRST_RUN_MARKER" ]; then
    su-exec obbyircd touch "$FIRST_RUN_MARKER"
    echo "First-run initialisation complete"
fi

echo "Starting ObbyIRCd..."
if [ "$#" -ge 2 ] && [ "$1" = "./bin/obbyircd" ] && [ "$2" = "-F" ]; then
    exec su-exec obbyircd "$@"
fi
exec su-exec obbyircd "$@"
