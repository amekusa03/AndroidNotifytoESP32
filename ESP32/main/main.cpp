#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"
#include "esp_bt_device.h"

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "bt_settings.h"

static const char *TAG = "notify_bt";

static const int WIDTH       = 320;
static const int HEIGHT      = 170;
static const size_t BUF_SIZE = WIDTH * HEIGHT * 2;  // RGB565 (108,800 bytes)

static uint8_t *s_rx_buf = NULL;
static size_t s_rx_received = 0;

// ── LCD ミューテックスとタイマー ─────────────────────────
static SemaphoreHandle_t s_lcd_mutex = NULL;
static TimerHandle_t s_bl_timer = NULL;

// ── ディスプレイ設定（1.9インチ LCD 170x320 / ST7789V3）────
// ピンアサイン: SCLK=18, MOSI=23, DC=2, CS=15, RST=4, BLK=32
#define LCD_BLK_PIN 32  // バックライト制御

class LGFX_ESP32_19inch_LCD : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel;
    lgfx::Bus_SPI      _bus;
public:
    LGFX_ESP32_19inch_LCD() {
        { auto c = _bus.config();
          c.spi_host   = SPI2_HOST;
          c.spi_mode   = 0;          // ST7789 は MODE0
          c.freq_write = 40000000;
          c.freq_read  = 16000000;
          c.pin_sclk   = 18;  // SCLK (D18 / GPIO18)
          c.pin_mosi   = 23;  // MOSI (D23 / GPIO23)
          c.pin_miso   = -1;
          c.pin_dc     = 2;   // DC (D2 / GPIO2)
          _bus.config(c);
          _panel.setBus(&_bus); }
        { auto c = _panel.config();
          c.pin_cs          = 15;  // CS (D15 / GPIO15)
          c.pin_rst         = 4;   // RST (D4 / GPIO4)
          c.pin_busy        = -1;
          c.memory_width    = 240; // ST7789のGRAMは240x320
          c.memory_height   = 320;
          c.panel_width     = 170;
          c.panel_height    = 320;
          c.offset_x        = 35;
          c.offset_y        = 0;
          c.offset_rotation = 0;
          c.dummy_read_pixel = 8;
          c.dummy_read_bits  = 1;
          c.readable  = false;
          c.invert    = true;
          c.rgb_order = true;
          _panel.config(c); }
        setPanel(&_panel);
    }
};

static LGFX_ESP32_19inch_LCD lcd;

// バックライトタイマーコールバック
static void backlight_timer_cb(TimerHandle_t xTimer) {
    // バックライトOFF ＆ ディスプレイICをスリープ
    gpio_set_level((gpio_num_t)LCD_BLK_PIN, 0);
    if (s_lcd_mutex) {
        xSemaphoreTake(s_lcd_mutex, portMAX_DELAY);
        lcd.sleep();
        xSemaphoreGive(s_lcd_mutex);
    }
}

// ── LCD ヘルパー（ミューテックス保護済み）────────────────
static void lcd_show_status(const char *l1, const char *l2,
                             uint32_t bg = TFT_BLACK, uint32_t title_color = TFT_GREEN, uint32_t content_color = TFT_BLUE) {
    if (!s_lcd_mutex) return;
    xSemaphoreTake(s_lcd_mutex, portMAX_DELAY);
    lcd.wakeup();
    lcd.fillScreen(bg);
    lcd.setTextColor(title_color);
    lcd.drawString(l1, 10, 10);
    if (l2) {
        lcd.setTextColor(content_color);
        lcd.drawString(l2, 10, 32);
    }
    xSemaphoreGive(s_lcd_mutex);
}

// ── Bluetooth GAP / SPP 設定 ──────────────────────────────
static const esp_spp_sec_t sec_mask = ESP_SPP_SEC_AUTHENTICATE;
static const esp_spp_role_t role_slave = ESP_SPP_ROLE_SLAVE;

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    switch (event) {
    case ESP_SPP_INIT_EVT:
        if (param->init.status == ESP_SPP_SUCCESS) {
            ESP_LOGI(TAG, "ESP_SPP_INIT_EVT: SPP 初期化完了");
            esp_spp_start_srv(sec_mask, role_slave, 0, BT_DEVICE_NAME);
        } else {
            ESP_LOGE(TAG, "ESP_SPP_INIT_EVT エラー status: %d", param->init.status);
        }
        break;

    case ESP_SPP_START_EVT:
        if (param->start.status == ESP_SPP_SUCCESS) {
            ESP_LOGI(TAG, "ESP_SPP_START_EVT: SPP サーバー起動完了 (handle:%" PRIu32 ")", param->start.handle);
            esp_bt_dev_set_device_name(BT_DEVICE_NAME);
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
//            lcd_show_status("ESP32 Notify", "BT Waiting...");
        } else {
            ESP_LOGE(TAG, "ESP_SPP_START_EVT エラー status: %d", param->start.status);
        }
        break;

    case ESP_SPP_SRV_OPEN_EVT:
        ESP_LOGI(TAG, "ESP_SPP_SRV_OPEN_EVT: クライアント接続完了 (handle:%" PRIu32 ")", param->srv_open.handle);
        s_rx_received = 0;
//        lcd_show_status("ESP32 Notify", "BT Connected!", TFT_BLACK, TFT_GREEN, TFT_CYAN);
        break;

    case ESP_SPP_CLOSE_EVT:
        ESP_LOGI(TAG, "ESP_SPP_CLOSE_EVT: 接続切断");
        s_rx_received = 0;
        break;

    case ESP_SPP_DATA_IND_EVT:
        if (param->data_ind.len > 0 && param->data_ind.data != NULL) {
            size_t copy_len = param->data_ind.len;
            if (s_rx_received + copy_len > BUF_SIZE) {
                copy_len = BUF_SIZE - s_rx_received;
            }
            if (copy_len > 0 && s_rx_buf != NULL) {
                memcpy(s_rx_buf + s_rx_received, param->data_ind.data, copy_len);
                s_rx_received += copy_len;
            }

            // BUF_SIZE (108,800 バイト) のデータ受領が完了した場合
            if (s_rx_received >= BUF_SIZE) {
                ESP_LOGI(TAG, "画像データ全受信完了 (%zu bytes)", s_rx_received);

                // ディスプレイのスリープ解除
                xSemaphoreTake(s_lcd_mutex, portMAX_DELAY);
                lcd.wakeup();
                xSemaphoreGive(s_lcd_mutex);

                // バックライトをON(1)にし、10秒タイマーをリセット
                gpio_set_level((gpio_num_t)LCD_BLK_PIN, 1);
                if (s_bl_timer) {
                    xTimerReset(s_bl_timer, 0);
                }

                // スライドインアニメーション
                const int steps = 15;
                for (int i = 0; i <= steps; i++) {
                    float t = (float)i / steps;
                    float ease = 1.0f - (1.0f - t) * (1.0f - t); // Ease-Out Quad
                    int y = (int)(-HEIGHT + ease * HEIGHT);

                    xSemaphoreTake(s_lcd_mutex, portMAX_DELAY);
                    lcd.startWrite();
                    lcd.pushImage(0, y, WIDTH, HEIGHT, (lgfx::rgb565_t*)s_rx_buf);
                    lcd.endWrite();
                    xSemaphoreGive(s_lcd_mutex);

                    vTaskDelay(pdMS_TO_TICKS(15));
                }

                ESP_LOGI(TAG, "LCD 描画完了 (スライドインアニメーション)");
                s_rx_received = 0;  // 次の受信に備えてクリア
            }
        }
        break;

    default:
        break;
    }
}

static void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "ペアリング成功: %s", param->auth_cmpl.device_name);
        } else {
            ESP_LOGE(TAG, "ペアリング失敗, status:%d", param->auth_cmpl.stat);
        }
        break;

    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI(TAG, "SSP パスキー確認要求 (自動承認)");
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;

    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(TAG, "パスキー通知: %" PRIu32, param->key_notif.passkey);
        break;

    case ESP_BT_GAP_KEY_REQ_EVT:
        ESP_LOGI(TAG, "パスキー入力要求");
        break;

    default:
        break;
    }
}

static void bt_init() {
    esp_err_t ret;

    // BT コントローラ初期化
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    bt_cfg.mode = ESP_BT_MODE_CLASSIC_BT;
    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGE(TAG, "esp_bt_controller_init 失敗: %s", esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
        ESP_LOGE(TAG, "esp_bt_controller_enable 失敗: %s", esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_init()) != ESP_OK) {
        ESP_LOGE(TAG, "esp_bluedroid_init 失敗: %s", esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_enable()) != ESP_OK) {
        ESP_LOGE(TAG, "esp_bluedroid_enable 失敗: %s", esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_gap_register_callback(esp_bt_gap_cb)) != ESP_OK) {
        ESP_LOGE(TAG, "esp_bt_gap_register_callback 失敗: %s", esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_spp_register_callback(esp_spp_cb)) != ESP_OK) {
        ESP_LOGE(TAG, "esp_spp_register_callback 失敗: %s", esp_err_to_name(ret));
        return;
    }

    esp_spp_cfg_t bt_spp_cfg = {
        .mode = ESP_SPP_MODE_CB,
        .enable_l2cap_ertm = true,
        .tx_buffer_size = 0,
    };
    if ((ret = esp_spp_enhanced_init(&bt_spp_cfg)) != ESP_OK) {
        ESP_LOGE(TAG, "esp_spp_enhanced_init 失敗: %s", esp_err_to_name(ret));
        return;
    }

    // Secure Simple Pairing 設定 (Just Works)
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
}

// ── エントリーポイント ───────────────────────────────────
extern "C" void app_main() {
    // NVS 初期化
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 受信バッファの確保
    s_rx_buf = (uint8_t *)malloc(BUF_SIZE);
    if (!s_rx_buf) {
        ESP_LOGE(TAG, "受信バッファの malloc 失敗");
        return;
    }

    // LCD ミューテックス作成
    s_lcd_mutex = xSemaphoreCreateMutex();

    // ディスプレイ バックライト制御
    gpio_reset_pin((gpio_num_t)LCD_BLK_PIN);
    gpio_set_direction((gpio_num_t)LCD_BLK_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)LCD_BLK_PIN, 1); // 1 = ON

    // LCD 初期化（メインタスクから1回だけ）
    lcd.init();
    lcd.setRotation(1);
    lcd.fillScreen(TFT_BLACK);

    lcd.setTextColor(TFT_GREEN);
    lcd.setTextSize(2);
    lcd.drawString("ESP32 Notify", 10, 10);
    lcd.setTextColor(TFT_BLUE);
    lcd.drawString("BT Starting...", 10, 40);

    // バックライト消灯用の10秒タイマーを作成＆スタート
    s_bl_timer = xTimerCreate("bl_timer", pdMS_TO_TICKS(10000), pdFALSE, (void*)0, backlight_timer_cb);
    if (s_bl_timer) {
        xTimerStart(s_bl_timer, 0);
    }

    // Bluetooth 初期化
    bt_init();

    ESP_LOGI(TAG, "起動完了");
}
