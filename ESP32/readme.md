# ESP32 Ubuntu通知表示システム (ESP32wrNotify)

Ubuntu システムの通知（D-Bus）をキャプチャ、または任意の画像ファイルを指定して、Bluetooth Classic (SPP: Serial Port Profile) または Wi-Fi (TCP Socket) 経由で ESP32 ディスプレイに画像として転送・表示するシステムです。

---

## 🌟 特長・仕様

* **ディスプレイ**: 1.9インチ ST7789V3 LCD
* **解像度**: 320 x 170 ピクセル
* **カラー絵文字対応**: `Noto Color Emoji` によるカラー絵文字の自動レンダリング（フォールバックとして `Symbola` 等のモノクロ絵文字に対応）
* **省電力・表示時間制御**:
  * **秒数指定 (1〜300秒)**: 指定された秒数表示後、バックライト自動オフ＆スリープ移行。
  * **永久表示 (0秒)**: 次の画像送信があるまで消灯せず永久的に表示を維持。
  * **デフォルト (未指定時)**: 10秒間表示。
* **任意画像送信コマンド**:
  * コマンドライン引数 (`-i`) で指定した画像 (PNG/JPEG等) を320x170に自動変換・表示時間指定して送信可能。
* **高精度カラー・グラデーション表示**:
  * 16ビット RGB565 Little-Endian 送信により、階調段差（色跳び）のない高精度な画像表示を実現。
* **通信仕様・プロトコルヘッダー**:
  * **プロトコルヘッダー (任意)**: 先頭4バイト (`'N'`, `'T'`, `duration_sec` (uint16_t Big-Endian)) ＋ RGB565画像データ (108,800 バイト)。ヘッダーなしの従来形式（108,800バイト固定）とも完全互換。
  * **Wi-Fi (TCP Socket)**: ネットワーク経由での高速画像転送 (ポート 5555)
  * **Bluetooth Classic SPP**: RFCOMM チャンネル 1 通信
  * Wi-Fi / Bluetooth のデュアル待受に対応。送信側スクリプト (`ubuntu_notifier.py`) では Wi-Fi 接続優先・BT 自動フォールバック (`auto` モード) が可能です。

---

## 🛠️ ハードウェア仕様・ピンアサイン

* **開発ボード**: 初代 ESP32 シリーズ (ESP32-D0WD / ESP32-WROOM-32 等)
  * ※ ESP32-C3 / ESP32-C6 / ESP32-S3 等のバリエーションは BLE のみ対応のため非対応
* **ディスプレイコントローラ**: ST7789 / ST7789V3 (170x320)
* **ピンアサイン**:
  * `SCLK`: GPIO 18
  * `MOSI`: GPIO 23
  * `DC`: GPIO 2
  * `CS`: GPIO 15
  * `RST`: GPIO 4
  * `BLK` (バックライト制御): GPIO 32

---

## 🎨 画面配色およびフォント仕様

* **視認性向上・にじみ対策**: 液晶のコントラストを高め文字の視認性を向上させるため、背景色は完全な黒 (`0x000000`)、タイトルは青系統 (`#0078FF`)、本文は白 (`#FFFFFF`) で描画します。
* **絵文字レンダリング**:
  * 第一優先: システムの `Noto Color Emoji` フォントを使用してカラー絵文字を自動抽出・リサイズして合成。
  * フォールバック: カラー絵文字で描画できない場合、モノクロ絵文字フォント (`Symbola` や `DejaVuSans` 等) を使用。

---

## 🚀 セットアップ手順

### 1. デバイス側 (ESP32) のセットアップ

#### 1.1 Bluetooth 設定 (`bt_settings.h`)
`main/bt_settings.h.example` をコピーして `main/bt_settings.h` を作成し、必要に応じてデバイス名を編集します。

```bash
cp main/bt_settings.h.example main/bt_settings.h
```

`main/bt_settings.h` の設定項目:
* `BT_DEVICE_NAME`: Bluetooth デバイス名 (デフォルト: `"ESP32_Notify"`)

#### 1.2 Wi-Fi 設定 (`wifi_settings.h`)
`main/wifi_settings.h.example` をコピーして `main/wifi_settings.h` を作成し、接続先 SSID・パスワード・固定 IP 設定を編集します。

```bash
cp main/wifi_settings.h.example main/wifi_settings.h
```

`main/wifi_settings.h` の設定項目:
* `WIFI_SSID`: 接続先 Wi-Fi の SSID
* `WIFI_PASS`: 接続先 Wi-Fi のパスワード
* `TCP_PORT`: TCP サーバー待受ポート (デフォルト: `5555`)
* `IP_ADDR_0`〜`IP_ADDR_3`: 固定 IP アドレス (例: `192.168.11.100`)

#### 1.3 ファームウェアのビルドと書き込み
ESP-IDF 開発環境がセットアップされたターミナルで実行します。

```bash
# ターゲットデバイスの設定
idf.py set-target esp32

# ビルド、書き込み、およびシリアルモニタの開始
idf.py build
idf.py flash monitor
```

---

### 2. ホスト側 (Ubuntu) のセットアップ

#### 2.1 必要なシステムパッケージのインストール
D-Bus 通信、Bluetooth、画像レンダリング、日本語および各種絵文字フォントのインストールを行います。

```bash
sudo apt update
sudo apt install python3-venv python3-dbus python3-gi gir1.2-glib-2.0 \
                 fonts-noto-color-emoji fonts-symbola fonts-noto-cjk bluez
```

#### 2.2 Bluetooth ペアリング（Bluetooth 利用時）
1. ESP32 の電源を入れます。
2. Ubuntu の「設定」→「Bluetooth」から `ESP32_Notify` を探してペアリング（接続）を完了させます。

#### 2.3 Python 仮想環境の作成とライブラリインストール
システムパッケージ (D-Bus / GI) を参照できるよう `--system-site-packages` オプションを指定して仮想環境を作成します。

```bash
# プロジェクトディレクトリに移動
cd /path/to/ESP32wrNotify

# 仮想環境の作成
python3 -m venv --system-site-packages .venv

# 仮想環境の有効化
source .venv/bin/activate

# 画像処理ライブラリ (Pillow) のインストール
pip install pillow
```

*※ `ubuntu_notifier.py` はシステム `python3` から直接起動されてもプロジェクト内の `.venv` 環境へと自動切替を行って実行されます。*

#### 2.4 通知監視モードの実行 (D-Bus デーモン)
`ubuntu_notifier.py` 内の設定を必要に応じて変更します:
* `CONNECT_MODE`: 接続モード (`"auto"`, `"wifi"`, `"bt"`, `"both"`)
* `DISPLAY_DURATION`: デフォルト表示時間 (秒, デフォルト: `10`)
* `ESP32_IP`: ESP32 の IP アドレス (Wi-Fi 用)
* `ESP32_BT_ADDR`: ESP32 の Bluetooth MAC アドレス (空文字 `""` で自動検索)

```bash
# D-Bus 通知監視モードで起動
python3 ubuntu_notifier.py
```

別のターミナルからテスト通知を送信して画面表示を確認します:
```bash
notify-send "テスト通知" "Hello, ESP32 Dual Mode! 🚀"
```

#### 2.5 任意画像ファイルの直接指定送信（コマンドオプション）
任意の画像ファイル（PNG/JPEG等）を320x170ピクセルに自動変換（アスペクト比維持＆黒背景中央配置）し、表示時間を指定してESP32へ直接送信・表示できます。

```bash
# 画像ファイルと表示秒数(例: 30秒)を指定して送信
python3 ubuntu_notifier.py -i /path/to/image.png -d 30

# 0秒指定（次の送信があるまで永久表示）
python3 ubuntu_notifier.py -i /path/to/image.jpg -d 0

# アスペクト比を無視して320x170に強制拡大・縮小
python3 ubuntu_notifier.py -i /path/to/image.png -d 10 --stretch
```

##### CLI コマンドの主要引数一覧:
* `-i`, `--image`: 送信画像ファイルのパス
* `-d`, `--duration`: 表示時間 (秒)。`0` = 永久表示, 最大 `300` (デフォルト: `10`)
* `-m`, `--mode`: 接続モード (`auto`, `wifi`, `bt`, `both`)
* `--stretch`: アスペクト比を無視して320x170に全画面拡大・縮小
* `--color-order`: カラーチャンネル順序 (`rgb`, `rbg`, `bgr`, `brg`, `grb`, `gbr`)
* `--invert`: 画像の明暗・色（ネガ）を反転して送信
* `--swap-bytes`: バイトオーダー反転の有効化 (デフォルト: 有効)
* `--no-swap-bytes`: バイトオーダー反転を無効化 (Big-Endian送信)

#### 2.6 systemd ユーザーサービスによる自動起動設定
システムログイン時にバックグラウンドで自動起動させたい場合は、付属の `release.sh` スクリプトを実行します。

```bash
./release.sh
```

* **サービス状態の確認**:
  ```bash
  systemctl --user status esp32-notify.service
  ```
* **リアルタイムログの確認**:
  ```bash
  journalctl --user -u esp32-notify.service -f
  ```
* **サービスの再起動**:
  ```bash
  systemctl --user restart esp32-notify.service
  ```
