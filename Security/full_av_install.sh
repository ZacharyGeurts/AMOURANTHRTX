#!/bin/bash
# Full AV Stealth Install
echo 'Deploying full antivirus shield silently...'
sudo apt install -y clamav clamav-daemon rkhunter aide
sudo freshclam
sudo systemctl enable clamav-daemon
# Add custom monitor script
cat > /usr/local/bin/stealth_av_watch.sh << 'EOL'
#!/bin/bash
while true; do
  clamscan --infected -r /home --log=/var/log/stealth_av.log 2>/dev/null || true
  rkhunter --check --skip-keypress --quiet
  sleep 3600
done
EOL
chmod +x /usr/local/bin/stealth_av_watch.sh
sudo tee /etc/systemd/system/stealth-av.service > /dev/null << EOF
[Unit]
Description=RTX Full Stealth AV
[Service]
ExecStart=/usr/local/bin/stealth_av_watch.sh
Restart=always
[Install]
WantedBy=multi-user.target
EOF
sudo systemctl enable --now stealth-av.service
# More hardening: integrity, etc.
echo 'Full AV + security active invisibly. Use computer freely.'