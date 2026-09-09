# ESP32 Notifier for Linux (Qt5 C++版)

[English](README.md) | [日本語](README.JP.md)

Ubuntu / Linux デスクトップ向けの D-Bus 通知キャッチ＆ESP32 転送アプリケーション（Qt5 / C++ 実装）です。  
D-Bus (`org.freedesktop.Notifications`) 経由でデスクトップ通知をリアルタイムに監視し、通知のタイトルや本文を 320x170 ピクセルの RGB565 画像に変換して TCP 経由で ESP32 へ自動転送します。

---

## 🌟 特長

- **D-Bus 通知自動キャッチ**: システム通知メッセージ（`org.freedesktop.Notifications`）を受信し、自動的に ESP32 ディスプレイ用画像（320x170）へ描画して送信。
- **タスクトレイ (System Tray) 常駐**: タスクトレイアイコンから設定画面の呼出、テスト通知の送信、アプリ終了が可能。
- **GUI 設定ダイアログ**:
  - ESP32 の IP アドレスおよび TCP ポート番号の変更
  - ログイン時の自動起動（Autostart）のオン/オフ切り替え
  - 動作確認用テスト通知送信
  - リアルタイム送信ログ表示
- **自動起動（Autostart）対応**: XDG Desktop Autostart 規格（`~/.config/autostart/esp32-notifier.desktop`）に準拠した自動起動設定をサポート。
- **高速＆軽量**: Qt5 / C++17 ネイティブ実装により、バックグラウンドでの低リソース・軽量動作を実現。

---

## 📦 依存パッケージ (ビルド前提条件)

Ubuntu / Debian 系 Linux ディストリビューションに必要なパッケージをインストールしてください:

```bash
sudo apt update
sudo apt install build-essential cmake qtbase5-dev libqt5dbus5
```

---

## 🛠️ ビルドと実行方法

### 1. ビルド
```bash
cd Linux
cmake -B build -S .
cmake --build build
```

ビルドが完了すると `build/esp32-notifier` 実行ファイルが生成されます。

### 2. 実行
```bash
./build/esp32-notifier
```

---

## ⚙️ 使い方

1. **起動**: アプリを起動するとタスクトレイ（システムトレイ）にアイコンが表示されます。
2. **設定**:
   - トレイアイコンをクリック（または右クリックメニューから「設定...」を選択）して設定ダイアログを開きます。
   - ESP32 の IP アドレス（デフォルト: `192.168.11.100`）および TCP ポート番号（デフォルト: `5555`）を設定し、「保存して閉じる」をクリックします。
3. **テスト送信**:
   - 設定ダイアログの「テスト通知送信」ボタン、またはトレイアイコンの右クリックメニュー「テスト通知送信」を押すと、動作確認用通知が ESP32 へ送信されます。
4. **自動起動 (Autostart)**:
   - 設定ダイアログの「ログイン時に自動実行する (Autostart)」にチェックを入れると、次回ログイン時から自動的にバックグラウンド起動します。

---

## 📐 技術仕様

- **対象画面サイズ**: 320 x 170 ピクセル (16-bit RGB565 Little-Endian)
- **ネットワーク通信**: TCP ソケット（デフォルト ポート: `5555`）
- **プロトコルヘッダー**: 4 バイトヘッダー (`'N'`, `'T'`, `duration_hi`, `duration_lo`) + 108,800 バイト (320x170x2) Raw RGB565 データ
- **設定ファイル保存先**: `~/.config/ESP32Notifier/Config.conf` (QSettings)
- **自動起動設定ファイル保存先**: `~/.config/autostart/esp32-notifier.desktop`
