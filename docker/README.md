# ObbyIRCd Docker

Self-contained image of this fork built straight from the local
source tree.  Every native module ObbyIRCd ships with -- account
registration, persistence, metadata, filehost, authtoken, smtp,
obbyscript, ... -- is compiled into the image and ready to load.

## Quick start

```bash
cp .env.example .env
# edit .env (SERVER_NAME, NETWORK_NAME, ADMIN_EMAIL, ports, ...)
docker compose up -d
docker compose logs -f obbyircd
```

The first time the conf volume is empty, the entrypoint:

1. Copies `modules.default.conf` and the rest of the bundled conf
   files into `/home/obbyircd/obby/conf/`.
2. Renders `obbyircd.conf` from `docker/obbyircd.conf.template` by
   substituting environment variables.
3. Issues a self-signed TLS cert valid for 1 day (testing only --
   replace with a real cert before any real traffic).
4. Generates random cloak keys if `CLOAK_KEY1/2/3` weren't supplied.

Subsequent runs reuse whatever is on disk.  Delete
`conf/.docker_initialized` (or remove the conf volume entirely) to
trigger a fresh init.

## Connecting

* **SSL/TLS IRC:** `ircs://<host>:6697`
* **WebSocket:** `ws://<host>:8080` -- plaintext.  Front this with a
  TLS-terminating reverse proxy (nginx / Traefik / Caddy) for
  production.

The default oper credentials in the rendered conf are
`admin / admin123`.  Change them before exposing the server.

## Volumes

| Volume                    | Container path                            | Purpose |
|---------------------------|-------------------------------------------|---------|
| `obbyircd_conf`           | `/home/obbyircd/obby/conf`                | Configuration. |
| `obbyircd_data`           | `/home/obbyircd/obby/data`                | Persistent state: account DB (`obsidian.db`), TKL bans, channeldb, persistence ghosts, RPC socket. |
| `obbyircd_logs`           | `/home/obbyircd/obby/logs`                | Server logs. |
| `obbyircd_tls`            | `/home/obbyircd/obby/tls`                 | `server.cert.pem` + `server.key.pem`. |
| `obbyircd_custom_modules` | `/home/obbyircd/obby/custom-modules`      | Drop `.c` files here to compile and load extra third-party modules without rebuilding the image. |

Set the corresponding `*_BIND` env var in `.env` to an absolute host
path to use a bind mount instead of a named volume.

## Environment variables

| Variable          | Default                | Notes |
|-------------------|------------------------|-------|
| `SERVER_NAME`     | `irc.example.com`      | The hostname clients connect to. |
| `NETWORK_NAME`    | `ObbyNetwork`          | Shown in MOTD / `005`. |
| `ADMIN_EMAIL`     | `admin@example.com`    | Used in the `admin {}` block and `kline-address`. |
| `MOTD_TEXT`       | `Welcome to ObbyIRCd!` | Currently informational; the real MOTD lives in `conf/motd.txt`. |
| `OPER_NAME`       | `admin`                | Network admin oper login. |
| `OPER_PASSWORD`   | *(generated)*          | If unset, a random secret is generated on first run and persisted to `data/.oper_password` (printed once to the container log). |
| `OPER_MASK`       | `*`                    | Hostmask the oper can /OPER from. Tighten this in production. |
| `SSL_PORT`        | `6697`                 | Internal TLS port. |
| `SSL_HOST_PORT`   | `6697`                 | Host-side port mapping. |
| `WS_PORT`         | `8080`                 | Internal plain WebSocket port. |
| `WS_HOST_PORT`    | `8080`                 | Host-side port mapping. |
| `FILEHOST_URL`    | *(unset)*              | Set to enable the `filehost {}` block. |
| `RPC_PASSWORD`    | *(unset)*              | Set to enable JSON-RPC over `RPC_PORT`. |
| `RPC_PORT`        | `8600`                 | Internal only. |
| `CLOAK_KEY1/2/3`  | *(generated)*          | Set explicitly for stable cloaks across restarts / linked nodes. |

## Custom modules at runtime

`/home/obbyircd/obby/custom-modules/*.c` is scanned at every container
start; each `.c` file is compiled with the source tree headers from
`/tmp/obbyircd-source/` and installed as `modules/third/<name>.so`.
Failing modules emit a warning and don't block the server starting.

Copy a module into the running container's volume in a portable way:

```bash
docker compose cp my-extra-module.c obbyircd:/home/obbyircd/obby/custom-modules/
docker compose restart obbyircd
# then add `loadmodule "third/my-extra-module";` to obbyircd.conf and:
docker compose exec obbyircd ./bin/obbyircd rehash
```

If you'd rather edit modules directly from the host, set
`CUSTOM_MODULES_BIND` in `.env` to a host directory and bind-mount the
volume there:

```bash
CUSTOM_MODULES_BIND=/srv/obbyircd/custom-modules
# then on the host:
cp my-extra-module.c /srv/obbyircd/custom-modules/
docker compose restart obbyircd
```

## Replacing the TLS cert

```bash
docker compose cp /path/to/fullchain.pem  obbyircd:/home/obbyircd/obby/tls/server.cert.pem
docker compose cp /path/to/privkey.pem    obbyircd:/home/obbyircd/obby/tls/server.key.pem
docker compose restart obbyircd
```

(Or, if you used `TLS_BIND` to mount the directory from the host, just
overwrite the files there.)

## Building manually

```bash
docker build -f docker/Dockerfile -t obbyircd:local .
```
