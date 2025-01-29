#!/bin/bash
gcc server/serwer.c -o serwerstatki
gcc klient/klient.c -o graczwstatki
dstdir=/usr/games #/usr/share/statki

[ -d "${dstdir}" ] || sudo mkdir ${dstdir}

cat > /etc/systemd/system/statki.service << EOF
[Unit]
Description=Gra w statki 10x10
After=network.target
DefaultDependencies=no

[Service]
Type=simple
ExecStart=${dstdir}/serwerstatki
TimeoutstartSec=0

[Install]
WantedBy=default.target
EOF
sudo cp serwerstatki ${dstdir}
sudo systemctl daemon-reload
sudo systemctl enable statki.service
sudo service statki start
sudo systemctl status statki.service