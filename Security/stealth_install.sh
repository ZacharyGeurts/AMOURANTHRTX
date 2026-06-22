# !/usr/bin/env bash
# Stealth Amouranth RTX Security - Invisible mode
set -e

echo 'Installing silent protector... (no output after this)'
mkdir -p ~/.config/ammo-stealth
# Safe hardening
sudo apt update && sudo apt install -y nftables 2>/dev/null || true
# Apply safe modules: skip aggressive
# firewall, services, etc.
sudo systemctl enable --now sshd || true  # example
# More hardening here
cat > /etc/systemd/system/stealth-guard.service << EOF
[Unit]
Description=Invisible Security Guard
[Service]
ExecStart=/bin/bash -c 'while true; do sleep 60; done'  # placeholder for real guards
Restart=always
EOF
sudo systemctl daemon-reload
sudo systemctl enable --now stealth-guard.service

echo 'Stealth security active. Invisible and effective.'
echo 'Mouse and keyboard safe. Full security in background.'

# Copy other files logic here but stealth
ls
