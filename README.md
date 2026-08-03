# AndroidNotifytoESP32 / ESP32wrNotify

Androidスマートフォンの通知、またはUbuntu desktopのD-Bus通知をキャッチし、320x170ピクセルの画像データ（RGB565形式）に変換してESP32液晶ディスプレイへ転送・表示するアプリケーション＆システムです。

---

## 🌟 主な機能

- **通知の自動キャッチ＆画像化**:
  - **Android アプリ**: システム通知を取得し、タイトル、本文、時刻をリアルタイム画像化。
  - **Ubuntu スクリプト**: D-Bus通知を監視し、カラー絵文字対応で画像化。サービス起動時には `🔔 PC Service Start` を ESP32 へ自動送信。
- **ESP32 起動時 Welcome 画面**:
  - ESP32 起動時に「Welcome / ESP32 Notify」画面を10秒間表示後、自動スリープ。初期化メッセージ非表示で視認性を向上。
- **2つの通信モード**:
  - **TCP (Wi-Fi)**: ローカルネットワーク内のESP32へTCPソケット経由で高速送信 (ポート 5555)。
  - **Bluetooth Classic (SPP)**: Wi-Fi環境がない場所でも、Bluetoothシリアル経由で送信可能 (RFCOMM Ch 1)。
- **表示時間制御オプション (0秒で永久表示、最大300秒)**:
  - **1〜300秒**: 指定された秒数表示後、バックライト消灯＆ディスプレイICスリープ移行。
  - **0秒**: 次の画像送信があるまで消灯せず永久的に表示を維持。
  - **デフォルト**: 未指定時10秒間表示。
- **Ubuntu 任意画像送信コマンド**:
  - 指定した画像ファイル (PNG/JPEG等) を320x170ピクセルに自動変換・表示時間指定して送信。
- **高精度カラーレンダリング**:
  - Little-Endian RGB565 最適化により、階調段差（色跳び）のない滑らかな写真・グラデーション表示を実現。

---

## 📱 Android アプリの使い方

### 1. セットアップ
1. アプリ起動後、「通知アクセス設定を開く」ボタンから通知アクセス権限を許可します。
2. Bluetooth送信を利用する場合は、あらかじめAndroid設定からESP32と**ペアリング**を完了させてください。

### 2. 操作方法
1. **送信モードの選択**: 「TCP (Wi-Fi)」または「Bluetooth」を選択。
2. **接続情報・設定**:
   - **TCP**: ESP32の固定IPアドレスを入力。
   - **Bluetooth**: ペアリング済みのESP32デバイス名を入力。
   - **表示時間(秒)**: 画像の表示時間を入力 (0: 永久表示, 1〜300秒, デフォルト: 10秒)。
3. **通知の転送**: 通知が届くと自動的にESP32へ画像が送信されます。

---

## 🐧 Ubuntu スクリプトの使い方 (`ESP32/ubuntu_notifier.py`)

### 1. D-Bus 通知監視モード (常駐)
```bash
python3 ubuntu_notifier.py
```
*システム通知を自動監視し、通知が届くとESP32へ送信します。*

### 2. 任意画像ファイルの指定送信 (CLIコマンド)
任意の画像ファイル（PNG/JPEG等）を320x170に自動リサイズし、表示時間を指定して送信します。

```bash
# 画像と表示時間(例: 30秒)を指定して送信
python3 ubuntu_notifier.py -i /path/to/image.png -d 30

# 0秒指定（次の送信があるまで永久表示）
python3 ubuntu_notifier.py -i /path/to/image.jpg -d 0

# アスペクト比を無視して320x170に強制拡大・縮小
python3 ubuntu_notifier.py -i /path/to/image.png -d 10 --stretch
```

#### CLI コマンドの主要オプション
- `-i`, `--image`: 送信画像ファイルのパス
- `-d`, `--duration`: 表示時間 (秒)。`0` = 永久表示, 最大 `300` (デフォルト: `10`)
- `-m`, `--mode`: 接続モード (`auto`, `wifi`, `bt`, `both`)
- `--stretch`: アスペクト比を無視して320x170に全画面リサイズ
- `--color-order`: カラーチャンネル順序 (`rgb`, `rbg`, `bgr` 等)
- `--invert`: 画像の明暗・色（ネガ）を反転

---

## 🛠️ 技術仕様

- **解像度**: 320 x 170 ピクセル
- **データ形式**: 16ビット RGB565 (Little-Endian, 108,800 バイト/フレーム)
- **プロトコルヘッダー (オプション)**: 先頭4バイト (`'N'`, `'T'`, `duration_sec` (uint16_t Big-Endian))
- **Bluetoothプロファイル**: SPP (Serial Port Profile) - UUID: `00001101-0000-1000-8000-00805f9b34fb`

---

## 開発環境
- **Android App**: Android Studio Ladybug | Kotlin / Jetpack Navigation / ViewBinding
- **ESP32 Firmware**: ESP-IDF v5.4 / LovyanGFX
- **Ubuntu Client**: Python 3.12 / Pillow / dbus-python
