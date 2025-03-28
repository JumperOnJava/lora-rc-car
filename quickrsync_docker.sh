#!/bin/bash

REMOTE_USER="slyvik"
REMOTE_HOST="192.168.1.105"
REMOTE_DIR="/home/slyvik/lora-rc-car-rsync"
LOCAL_DIR="/app"
SSH_PASS="slyvik123"

# Sync files via rsync
sshpass -p "$SSH_PASS" rsync -avz --progress --delete "$LOCAL_DIR" "$REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR"
