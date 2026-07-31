#!/bin/bash
set -e

# 定数定義
SERVICE_NAME="esp32-notify.service"
SERVICE_DIR="$HOME/.config/systemd/user"
SERVICE_FILE="$SERVICE_DIR/$SERVICE_NAME"
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== ESP32 Notification Sender サービス登録スクリプト ==="

# 1. systemd ユーザー用ディレクトリの確認
echo "[1/4] systemd ユーザーディレクトリを確認しています..."
mkdir -p "$SERVICE_DIR"

# 2. サービス定義ファイルの生成
echo "[2/4] サービス定義ファイルを生成しています..."
cat << EOF > "$SERVICE_FILE"
[Unit]
Description=ESP32 Notification Sender Daemon
After=network.target

[Service]
Type=simple
WorkingDirectory=$PROJECT_DIR
ExecStart=$PROJECT_DIR/.venv/bin/python3 ubuntu_notifier.py
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=default.target
EOF
echo "  -> 生成完了: $SERVICE_FILE"

# 3. サービスの登録と起動
echo "[3/4] systemd サービスを登録して起動しています..."
systemctl --user daemon-reload
systemctl --user enable "$SERVICE_NAME"
systemctl --user restart "$SERVICE_NAME"

# 4. 動作確認
echo "[4/4] サービスの起動ステータスを確認しています..."
sleep 1
systemctl --user status "$SERVICE_NAME" --no-pager

echo "=========================================================="
echo "リリース完了しました。"
echo "テスト送信をするには、以下を実行してください："
echo "  notify-send \"テスト\" \"こんにちは\""
echo "=========================================================="
