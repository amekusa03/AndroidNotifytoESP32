# ライブラリーインストール
# pip install pillow dbus-python

import sys
import os
import time
import struct
import socket
import subprocess
from PIL import Image, ImageDraw, ImageFont
import dbus
import dbus.mainloop.glib
from gi.repository import GLib

# journald でログが即座に出るよう stdout をアンバッファーに
sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', buffering=1)
sys.stderr = os.fdopen(sys.stderr.fileno(), 'w', buffering=1)

# --- 設定項目 ---
# 接続モード: "auto" (Wi-Fi 優先 → BT フォールバック), "wifi" (Wi-Fi のみ), "bt" (Bluetooth のみ), "both" (両方に送信)
CONNECT_MODE  = "auto"

# ESP32 の Wi-Fi (TCP Socket) 設定
ESP32_IP      = "192.168.11.100"  # ESP32 の IP アドレス (例: wifi_settings.h で設定した IP)
TCP_PORT      = 5555              # TCP サーバーポート

# ESP32 の Bluetooth MAC アドレス (例: "AA:BB:CC:DD:EE:FF")
# 未設定 ("") の場合は bluetoothctl で "ESP32_Notify" の自動検索を試みます
ESP32_BT_ADDR = ""
RFCOMM_PORT   = 1       # SPP (Serial Port Profile) RFCOMM チャンネル

WIDTH         = 320     # 1.9インチ LCD (ST7789V3) の実解像度
HEIGHT        = 170


def find_esp32_bt_address() -> str | None:
    """bluetoothctl で周囲の ESP32_Notify デバイスの MAC アドレスを検索"""
    try:
        res = subprocess.run(["bluetoothctl", "devices"], capture_output=True, text=True, timeout=5)
        for line in res.stdout.splitlines():
            # 例: "Device 11:22:33:44:55:66 ESP32_Notify"
            if "ESP32_Notify" in line:
                parts = line.split()
                if len(parts) >= 2:
                    return parts[1]
    except Exception:
        pass
    return None


def send_image_via_wifi(raw_data: bytes | bytearray) -> bool:
    """画像を TCP ソケット経由で ESP32 へ送信"""
    if not ESP32_IP:
        print("Wi-Fi 送信エラー: ESP32_IP が設定されていません。")
        return False
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(5.0)
            s.connect((ESP32_IP, TCP_PORT))
            s.sendall(bytes(raw_data))
        print(f"ESP32へ Wi-Fi (TCP) 送信完了 ({len(raw_data)} bytes → {ESP32_IP}:{TCP_PORT})")
        return True
    except Exception as e:
        print(f"Wi-Fi (TCP) 送信エラー: {e}")
        return False


def send_image_via_bt(raw_data: bytes | bytearray) -> bool:
    """画像を Bluetooth RFCOMM 経由で ESP32 へ送信"""
    target_addr = ESP32_BT_ADDR
    if not target_addr:
        target_addr = find_esp32_bt_address()
        if not target_addr:
            print("Bluetooth 送信エラー: ESP32 デバイス ('ESP32_Notify') の MAC アドレスが見つかりません。")
            print("ESP32_BT_ADDR に直接 MAC アドレスを設定するか、Bluetooth ペアリングを行ってください。")
            return False

    try:
        # Linux (Ubuntu) 標準の socket.AF_BLUETOOTH + BTPROTO_RFCOMM を使用
        with socket.socket(socket.AF_BLUETOOTH, socket.SOCK_STREAM, socket.BTPROTO_RFCOMM) as s:
            s.settimeout(10.0)
            s.connect((target_addr, RFCOMM_PORT))
            s.sendall(bytes(raw_data))
        print(f"ESP32へ Bluetooth 送信完了 ({len(raw_data)} bytes → {target_addr}:{RFCOMM_PORT})")
        return True
    except Exception as e:
        print(f"Bluetooth 送信エラー: {e}")
        return False


def send_image_to_esp32(img: Image.Image) -> None:
    """画像をRGB565に変換して設定されたモードに従いESP32へ送信"""
    img = img.convert('RGB')
    pixels = img.load()

    raw_data = bytearray()
    for y in range(HEIGHT):
        for x in range(WIDTH):
            r, g, b = pixels[x, y]
            # RGB565: R5G6B5 ビッグエンディアン
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            raw_data.extend(struct.pack('>H', rgb565))

    mode = CONNECT_MODE.lower()
    if mode == "wifi":
        send_image_via_wifi(raw_data)
    elif mode == "bt":
        send_image_via_bt(raw_data)
    elif mode == "both":
        send_image_via_wifi(raw_data)
        send_image_via_bt(raw_data)
    else:  # "auto" (Wi-Fi優先、失敗時BTフォールバック)
        if not send_image_via_wifi(raw_data):
            print("Wi-Fi 送信失敗のため、Bluetooth へフォールバック試行中...")
            send_image_via_bt(raw_data)



# --- カラー絵文字対応 ---
COLOR_EMOJI_FONT_PATH = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf"
COLOR_EMOJI_FONT = None
if os.path.exists(COLOR_EMOJI_FONT_PATH):
    try:
        COLOR_EMOJI_FONT = ImageFont.truetype(COLOR_EMOJI_FONT_PATH, 109)
    except Exception:
        COLOR_EMOJI_FONT = None


def render_color_emoji(char: str, target_size: int = 18) -> Image.Image | None:
    """Noto Color Emoji を指定サイズにリサイズした Image (RGBA) として生成"""
    if not COLOR_EMOJI_FONT:
        return None
    try:
        temp_img = Image.new('RGBA', (130, 130), (0, 0, 0, 0))
        temp_draw = ImageDraw.Draw(temp_img)
        temp_draw.text((10, 10), char, font=COLOR_EMOJI_FONT, embedded_color=True)
        bbox = temp_img.getbbox()
        if bbox:
            cropped = temp_img.crop(bbox)
            w, h = cropped.size
            scale = target_size / max(w, h)
            new_w, new_h = max(1, int(w * scale)), max(1, int(h * scale))
            return cropped.resize((new_w, new_h), Image.Resampling.LANCZOS)
    except Exception:
        pass
    return None


def draw_text_with_fallback(img: Image.Image, draw: ImageDraw.ImageDraw, xy: tuple[int, int], text: str, font_primary: ImageFont.FreeTypeFont | ImageFont.ImageFont, font_emoji: ImageFont.FreeTypeFont | ImageFont.ImageFont, fill: tuple[int, int, int], font_size: int = 18) -> None:
    """日本語フォントとカラー絵文字/モノクロ絵文字フォントを文字ごとに切り替えて描画"""
    x, y = xy
    for char in text:
        use_emoji = False
        code = ord(char)
        
        # CJK文字やASCII、主要な日本語記号の範囲外（＝絵文字や特殊記号）であれば絵文字フォントを強制
        if not (
            (0x00 <= code <= 0x7F) or          # ASCII
            (0x2000 <= code <= 0x206F) or      # General Punctuation
            (0x3000 <= code <= 0x303F) or      # CJK Symbols and Punctuation
            (0x3040 <= code <= 0x309F) or      # Hiragana
            (0x30A0 <= code <= 0x30FF) or      # Katakana
            (0x4E00 <= code <= 0x9FFF) or      # CJK Unified Ideographs
            (0xF900 <= code <= 0xFAFF) or      # CJK Compatibility Ideographs
            (0xFF00 <= code <= 0xFFEF)         # Halfwidth and Fullwidth Forms
        ):
            use_emoji = True

        if not use_emoji:
            try:
                bbox = font_primary.getbbox(char)
                if bbox is None or (bbox[2] - bbox[0] == 0 and bbox[3] - bbox[1] == 0 and not char.isspace()):
                    use_emoji = True
            except Exception:
                use_emoji = True

        if use_emoji:
            emoji_img = render_color_emoji(char, target_size=font_size)
            if emoji_img:
                # カラー絵文字合成
                img.paste(emoji_img, (x, y + 2), emoji_img)
                x += emoji_img.width + 2
                continue

        # モノクロ文字またはフォント描画フォールバック
        font = font_emoji if use_emoji else font_primary
        draw.text((x, y), char, font=font, fill=fill)
        x += int(draw.textlength(char, font=font))


def create_notification_image(title: str, body: str) -> Image.Image:
    """320×170 の通知画像を生成"""
    img  = Image.new('RGB', (WIDTH, HEIGHT), color=(0, 0, 0))
    draw = ImageDraw.Draw(img)

    # 日本語対応フォントを探す
    font_candidates = [
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/fonts-japanese-gothic.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    ]
    title_font = body_font = ImageFont.load_default()
    for fp in font_candidates:
        if os.path.exists(fp):
            try:
                title_font = ImageFont.truetype(fp, 20)
                body_font  = ImageFont.truetype(fp, 18)
                break
            except IOError:
                pass

    # 絵文字フォントを探す（モノクロフォールバック用）
    emoji_font_candidates = [
        "/usr/share/fonts/truetype/ancient-scripts/Symbola_hint.ttf",
        "/usr/share/fonts/truetype/gdouros/Symbola.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/noto/NotoEmoji-Regular.ttf",
    ]
    emoji_font = ImageFont.load_default()
    for fp in emoji_font_candidates:
        if os.path.exists(fp):
            try:
                emoji_font = ImageFont.truetype(fp, 18)
                break
            except IOError:
                pass

    # タイトル (青)
    draw_text_with_fallback(img, draw, (12, 10), f"🔔 {title}", title_font, emoji_font, fill=(0, 120, 255), font_size=20)

    # 本文（簡易折り返し、白）
    if body:
        chars_per_line = 16
        lines = [body[i:i+chars_per_line]
                 for i in range(0, min(len(body), chars_per_line * 3), chars_per_line)]
        for i, line in enumerate(lines[:3]):
            draw_text_with_fallback(img, draw, (12, 45 + i * 32), line, body_font, emoji_font, fill=(255, 255, 255), font_size=18)

    # タイムスタンプ (青)
    ts = time.strftime("%H:%M")
    draw_text_with_fallback(img, draw, (WIDTH - 65, HEIGHT - 28), ts, body_font, emoji_font, fill=(0, 120, 255), font_size=18)

    return img


def notification_handler(bus, message) -> None:
    """D-Bus から通知をキャッチしたときのコールバック"""
    if message.get_member() != "Notify":
        return
    try:
        args  = message.get_args_list()
        title = str(args[3]) if len(args) > 3 else "(タイトルなし)"
        body  = str(args[4]) if len(args) > 4 else ""
        print(f"通知受信: [{title}] {body}")
        img = create_notification_image(title, body)
        send_image_to_esp32(img)
    except Exception as e:
        print(f"通知処理エラー: {e}")


def main() -> None:
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SessionBus()

    try:
        bus.add_match_string(
            "type='method_call',"
            "interface='org.freedesktop.Notifications',"
            "member='Notify',"
            "eavesdrop=true"
        )
        print("D-Busマッチルール設定完了 (eavesdrop mode)")
    except dbus.exceptions.DBusException as e:
        print(f"eavesdrop モード失敗 ({e}), 通常モードで続行")
        bus.add_match_string(
            "type='method_call',"
            "interface='org.freedesktop.Notifications',"
            "member='Notify'"
        )

    bus.add_message_filter(notification_handler)

    print(f"通知監視開始 (モード: {CONNECT_MODE}, Wi-Fi: {ESP32_IP}:{TCP_PORT}, BT: {ESP32_BT_ADDR or 'Auto'})")
    print("テスト: notify-send 'タイトル' '本文'")

    try:
        GLib.MainLoop().run()
    except KeyboardInterrupt:
        print("\n終了します。")


if __name__ == '__main__':
    main()
