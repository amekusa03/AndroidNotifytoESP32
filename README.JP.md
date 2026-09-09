# AndroidNotifytoESP32 / ESP32wrNotify

[English](README.md) | [日本語](README.JP.md)

Androidスマートフォンの通知、またはUbuntu / Linux デスクトップのD-Bus通知をキャッチし、320x170ピクセルの画像データ（RGB565形式）に変換してESP32液晶ディスプレイへ転送・表示するアプリケーション＆システムです。

---

## 🌟 主な機能

- **通知の自動キャッチ＆画像化**:
  - **Android アプリ**: システム通知を取得し、タイトル、本文、時刻をリアルタイム画像化。
  - **Linux (C++/Qt) アプリ**: タスクトレイ常駐型GUIアプリ。D-Bus通知を自動キャッチ＆ESP32へ転送。設定画面や自動起動（Autostart）に対応。
  - **Ubuntu (Python) スクリプト**: D-Bus通知を監視し、カラー絵文字対応で画像化。CLIからの任意画像送信にも対応。
- **ESP32 起動時 Welcome 画面**:
  - ESP32 起動時に「Welcome / ESP32 Notify」画面を10秒間表示後、自動スリープ。初期化メッセージ非表示で視認性を向上。
- **2つの通信モード**:
  - **TCP (Wi-Fi / mDNS)**: mDNS対応により、固定IP指定不要で `esp32-notify.local` またはIPアドレス経由で高速送信 (ポート 5555)。
  - **Bluetooth Classic (SPP)**: Wi-Fi環境がない場所でも、Bluetoothシリアル経由で送信可能 (RFCOMM Ch 1)。
- **表示時間制御オプション (0秒で永久表示、最大300秒)**:
  - **1〜300秒**: 指定された秒数表示後、バックライト消灯＆ディスプレイICスリープ移行。
  - **0秒**: 次の画像送信があるまで消灯せず永久的に表示を維持。
  - **デフォルト**: 未指定時は10秒間表示。
- **Ubuntu/Linux 任意画像送信コマンド (Python)**:
  - 指定した画像ファイル（PNG/JPEG等）を自動的に320x170にリサイズし、表示時間を指定して送信可能。
- **高精度カラー表示**:
  - Little-Endian RGB565最適化により、階調段差（色跳び）のない高精度な画像・グラデーション表示を実現。

---

## 📱 Android アプリの使い方

### 1. 初期設定
1. アプリ起動後、「通知へのアクセス設定を開く」ボタンから通知アクセス権限を許可します。
2. Bluetooth 経由で送信する場合は、事前に Android の「設定」→「Bluetooth」で ESP32 との**ペアリング**を完了させてください。

### 2. 操作方法
1. **送信モード選択**: 「TCP (Wi-Fi)」または「Bluetooth」を選択します。
2. **接続情報・設定**:
   - **TCP**: ESP32 の IP アドレスまたは mDNS ホスト名 (`esp32-notify.local`) を入力します。
   - **Bluetooth**: ペアリング済み ESP32 のデバイス名を入力します。
   - **表示時間 (秒)**: 画像の表示秒数を入力します（0: 永久表示、1〜300秒、デフォルト: 10秒）。
3. **通知の転送**: 通知を受信すると、自動的に画像化されて ESP32 へ送信されます。

---

## 🐧 Linux / Ubuntu アプリの使い方

### 1. Qt5 C++ GUI版 (`Linux/`) ※推奨常駐アプリ

タスクトレイ常駐、GUI設定画面、自動起動に対応した C++ クライアントです。

#### ビルドと実行
```bash
# 依存パッケージのインストール
sudo apt update
sudo apt install build-essential cmake qtbase5-dev libqt5dbus5

# ビルド
cd Linux
cmake -B build -S .
cmake --build build

# 実行
./build/esp32-notifier
```
詳細は [`Linux/README.JP.md`](Linux/README.JP.md) を参照してください。

---

### 2. Python スクリプト版 (`ESP32/ubuntu_notifier.py`)

#### D-Bus 通知監視モード（常駐）
```bash
python3 ESP32/ubuntu_notifier.py
```
*システム通知を受信すると自動的に画像化して ESP32 へ送信します。*

#### 任意画像ファイルの指定送信（CLI コマンド）
任意の画像ファイル（PNG/JPEG等）を320x170に自動リサイズし、表示時間を指定して送信できます。

```bash
# 画像と表示秒数（例: 30秒）を指定して送信
python3 ESP32/ubuntu_notifier.py -i /path/to/image.png -d 30

# 0秒指定（次の送信まで永久表示）
python3 ESP32/ubuntu_notifier.py -i /path/to/image.jpg -d 0

# アスペクト比を無視して全画面拡大・縮小
python3 ESP32/ubuntu_notifier.py -i /path/to/image.png -d 10 --stretch
```

##### 主な CLI オプション
- `-i`, `--image`: 送信画像ファイルのパス
- `-d`, `--duration`: 表示時間（秒）。`0` = 永久表示、最大 `300`（デフォルト: `10`）
- `-m`, `--mode`: 接続モード (`auto`, `wifi`, `bt`, `both`)
- `--stretch`: アスペクト比を無視して320x170に全画面拡大・縮小
- `--color-order`: カラーチャンネル順序 (`rgb`, `rbg`, `bgr` 等)
- `--invert`: 画像の明暗・色（ネガ）反転

---

## ⚡ ESP32 ファームウェアのセットアップ

ピンアサイン、Wi-Fi / Bluetooth 設定、ファームウェアビルド手順については [`ESP32/README.JP.md`](ESP32/README.JP.md) を参照してください。

---

## 🛠️ 技術仕様

- **解像度**: 320 x 170 ピクセル
- **データ形式**: 16ビット RGB565 (Little-Endian, 108,800 バイト/フレーム)
- **プロトコルヘッダー (任意)**: 先頭4バイト (`'N'`, `'T'`, `duration_sec` (uint16_t Big-Endian))
- **Bluetooth プロファイル**: SPP (Serial Port Profile) - UUID: `00001101-0000-1000-8000-00805f9b34fb`

---

## 開発環境
- **Android アプリ**: Android Studio Ladybug | Kotlin / Jetpack Navigation / ViewBinding
- **Linux アプリ (Qt)**: Qt5 / C++17 / CMake (D-Bus, Widgets, Network)
- **ESP32 ファームウェア**: ESP-IDF v5.4 / LovyanGFX
- **Ubuntu クライアント (Python)**: Python 3.12 / Pillow / dbus-python
