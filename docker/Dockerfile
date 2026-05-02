# syntax=docker/dockerfile:1.6
#
# ObbyIRCd Docker image.  Builds the IRCd straight from this repository,
# so every native module shipped with the fork (account-registration,
# persistence, metadata, filehost, authtoken, smtp, obbyscript, ...) is
# available out of the box.
#
# Build:   docker build -f docker/Dockerfile -t obbyircd .
# Compose: docker compose up -d

FROM alpine:3.19

# Build + runtime dependencies.  sqlite-dev is needed by the
# account-registration module; the rest cover the core IRCd, TLS, JSON
# logging, PCRE2 regexes, argon2 password hashing, libsodium for the
# RPC subsystem and gettext for the entrypoint's envsubst.
RUN apk add --no-cache \
    build-base \
    openssl-dev \
    openssl \
    pcre2-dev \
    argon2-dev \
    libsodium-dev \
    jansson-dev \
    curl-dev \
    sqlite-dev \
    pkgconf \
    wget \
    tar \
    gettext \
    su-exec \
    netcat-openbsd \
    && rm -rf /var/cache/apk/*

RUN adduser -D -s /bin/sh obbyircd

# Copy the entire repository in.  .dockerignore keeps build artifacts,
# the ObsidianIRC client checkout, and other dev junk out.
WORKDIR /tmp/obbyircd-source
COPY --chown=obbyircd:obbyircd . .

# Pre-seed config.settings so ./Config -quick can build non-interactively.
RUN install -d -o obbyircd -g obbyircd \
        /home/obbyircd/obby \
        /home/obbyircd/obby/bin \
        /home/obbyircd/obby/data \
        /home/obbyircd/obby/conf \
        /home/obbyircd/obby/modules \
        /home/obbyircd/obby/logs \
        /home/obbyircd/obby/cache \
        /home/obbyircd/obby/doc \
        /home/obbyircd/obby/tmp \
        /home/obbyircd/obby/lib \
    && cat > /tmp/obbyircd-source/config.settings <<'EOF'
BASEPATH="/home/obbyircd/obby"
BINDIR="/home/obbyircd/obby/bin"
DATADIR="/home/obbyircd/obby/data"
CONFDIR="/home/obbyircd/obby/conf"
MODULESDIR="/home/obbyircd/obby/modules"
LOGDIR="/home/obbyircd/obby/logs"
CACHEDIR="/home/obbyircd/obby/cache"
DOCDIR="/home/obbyircd/obby/doc"
TMPDIR="/home/obbyircd/obby/tmp"
PRIVATELIBDIR="/home/obbyircd/obby/lib"
MAXCONNECTIONS_REQUEST="auto"
NICKNAMEHISTORYLENGTH="2000"
GEOIP="none"
DEFPERM="0600"
SSLDIR=""
REMOTEINC=""
CURLDIR=""
NOOPEROVERRIDE=""
OPEROVERRIDEVERIFY=""
GENCERTIFICATE="0"
SANITIZER=""
EXTRAPARA=""
ADVANCED=""
EOF

RUN chmod +x Config \
    && chown -R obbyircd:obbyircd /tmp/obbyircd-source /home/obbyircd

USER obbyircd
RUN ./Config -quick && make -j"$(nproc)" && make install

USER root

# Snapshot the freshly-installed conf tree somewhere outside the volume
# mount points so first-run can repopulate a fresh volume.
RUN mkdir -p /etc/obbyircd/conf-defaults \
    && cp -r /home/obbyircd/obby/conf/. /etc/obbyircd/conf-defaults/ \
    && chmod -R a+rX /etc/obbyircd

RUN mkdir -p /home/obbyircd/obby/conf \
             /home/obbyircd/obby/data \
             /home/obbyircd/obby/logs \
             /home/obbyircd/obby/tls \
             /home/obbyircd/obby/tmp \
             /home/obbyircd/obby/custom-modules \
    && chown -R obbyircd:obbyircd /home/obbyircd

COPY docker/docker-entrypoint.sh /usr/local/bin/docker-entrypoint.sh
COPY docker/obbyircd.conf.template /etc/obbyircd/obbyircd.conf.template
RUN chmod +x /usr/local/bin/docker-entrypoint.sh

ENV SERVER_NAME=irc.example.com \
    NETWORK_NAME=ObbyNetwork \
    ADMIN_EMAIL=admin@example.com \
    SSL_PORT=6697 \
    WS_PORT=8080 \
    RPC_PORT=8600 \
    MOTD_TEXT="Welcome to ObbyIRCd!"

VOLUME ["/home/obbyircd/obby/conf", \
        "/home/obbyircd/obby/data", \
        "/home/obbyircd/obby/logs", \
        "/home/obbyircd/obby/tls", \
        "/home/obbyircd/obby/custom-modules"]

WORKDIR /home/obbyircd/obby

ENTRYPOINT ["docker-entrypoint.sh"]
CMD ["./bin/obbyircd", "-F"]
