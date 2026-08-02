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

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "bt_settings.h"
#include "wifi_settings.h"

static const char *TAG = "notify_app";

static const int WIDTH       = 320;
static const int HEIGHT      = 170;
static const size_t BUF_SIZE = WIDTH * HEIGHT * 2;  // RGB565 (108,800 bytes)

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



// ── Bluetooth GAP / SPP 設定 ──────────────────────────────
static const esp_spp_sec_t sec_mask = ESP_SPP_SEC_AUTHENTICATE;
static const esp_spp_role_t role_slave = ESP_SPP_ROLE_SLAVE;
static size_t s_bt_rx_received = 0;
static uint8_t s_bt_header_buf[4];
static size_t s_bt_header_bytes = 0;
static bool s_bt_header_checked = false;

// ── ディスプレイ表示時間制御関数 ────────────────────────
static void apply_display_duration(uint16_t duration_sec) {
    if (duration_sec > 300) {
        duration_sec = 300;
    }
    ESP_LOGI(TAG, "表示時間設定: %u 秒", duration_sec);

    gpio_set_level((gpio_num_t)LCD_BLK_PIN, 1);
    lcd.wakeup();

    if (duration_sec == 0) {
        // 0秒: 次の送信まで永久表示（タイマー停止）
        if (s_bl_timer) {
            xTimerStop(s_bl_timer, 0);
        }
    } else {
        // 1〜300秒: 指定秒数後に自動消灯
        if (s_bl_timer) {
            xTimerChangePeriod(s_bl_timer, pdMS_TO_TICKS(duration_sec * 1000), 0);
            xTimerReset(s_bl_timer, 0);
        }
    }
}

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
        } else {
            ESP_LOGE(TAG, "ESP_SPP_START_EVT エラー status: %d", param->start.status);
        }
        break;

    case ESP_SPP_SRV_OPEN_EVT:
        ESP_LOGI(TAG, "ESP_SPP_SRV_OPEN_EVT: クライアント接続完了 (handle:%" PRIu32 ")", param->srv_open.handle);
        s_bt_rx_received = 0;
        s_bt_header_bytes = 0;
        s_bt_header_checked = false;
        break;

    case ESP_SPP_CLOSE_EVT:
        ESP_LOGI(TAG, "ESP_SPP_CLOSE_EVT: 接続切断");
        if (s_bt_rx_received > 0 && s_bt_rx_received < BUF_SIZE) {
            if (s_lcd_mutex) {
                xSemaphoreTake(s_lcd_mutex, portMAX_DELAY);
                lcd.endWrite();
                xSemaphoreGive(s_lcd_mutex);
            }
        }
        s_bt_rx_received = 0;
        s_bt_header_bytes = 0;
        s_bt_header_checked = false;
        break;

    case ESP_SPP_DATA_IND_EVT:
        if (param->data_ind.len > 0 && param->data_ind.data != NULL) {
            if (s_lcd_mutex) xSemaphoreTake(s_lcd_mutex, portMAX_DELAY);

            const uint8_t *src = param->data_ind.data;
            size_t src_len = param->data_ind.len;
            size_t src_idx = 0;

            if (!s_bt_header_checked) {
                while (src_idx < src_len && s_bt_header_bytes < 4) {
                    s_bt_header_buf[s_bt_header_bytes++] = src[src_idx++];
                }
                if (s_bt_header_bytes == 4) {
                    s_bt_header_checked = true;
                    uint16_t duration_sec = 10;
                    size_t unhandled_hdr_pixel_bytes = 0;
                    if (s_bt_header_buf[0] == 'N' && s_bt_header_buf[1] == 'T') {
                        duration_sec = ((uint16_t)s_bt_header_buf[2] << 8) | s_bt_header_buf[3];
                        ESP_LOGI(TAG, "BT ヘッダー検出: 表示時間 %u 秒", duration_sec);
                    } else {
                        duration_sec = 10;
                        unhandled_hdr_pixel_bytes = 4;
                        ESP_LOGI(TAG, "BT ヘッダーなし: デフォルト 10 秒");
                    }

                    apply_display_duration(duration_sec);
                    lcd.startWrite();
                    lcd.setAddrWindow(0, 0, WIDTH, HEIGHT);

                    if (unhandled_hdr_pixel_bytes > 0) {
                        lcd.writePixels((const lgfx::rgb565_t*)s_bt_header_buf, unhandled_hdr_pixel_bytes / 2);
                        s_bt_rx_received += unhandled_hdr_pixel_bytes;
                    }
                }
            }

            if (s_bt_header_checked && src_idx < src_len) {
                size_t remaining_bytes = src_len - src_idx;
                if (s_bt_rx_received + remaining_bytes > BUF_SIZE) {
                    remaining_bytes = BUF_SIZE - s_bt_rx_received;
                }
                if (remaining_bytes >= 2) {
                    lcd.writePixels((const lgfx::rgb565_t*)(src + src_idx), remaining_bytes / 2);
                    s_bt_rx_received += remaining_bytes & ~((size_t)1);
                }
            }

            if (s_bt_rx_received >= BUF_SIZE) {
                ESP_LOGI(TAG, "BT 画像データ全受信・描画完了 (%zu bytes)", s_bt_rx_received);
                lcd.endWrite();
                s_bt_rx_received = 0;
                s_bt_header_bytes = 0;
                s_bt_header_checked = false;
            }
            if (s_lcd_mutex) xSemaphoreGive(s_lcd_mutex);
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

// ── Wi-Fi STA および TCP サーバー設定 ─────────────────────
static bool s_wifi_connected = false;
static char s_wifi_ip_str[32] = "WiFi Connecting...";

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        snprintf(s_wifi_ip_str, sizeof(s_wifi_ip_str), "WiFi Disconnected");
        ESP_LOGI(TAG, "WiFi 切断, 再接続中...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        s_wifi_connected = true;
        snprintf(s_wifi_ip_str, sizeof(s_wifi_ip_str), "IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "WiFi 接続完了, %s", s_wifi_ip_str);
    }
}

static void wifi_init_sta(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();

#if defined(IP_ADDR_0) && defined(IP_ADDR_1) && defined(IP_ADDR_2) && defined(IP_ADDR_3)
    esp_netif_dhcpc_stop(sta_netif);
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, IP_ADDR_0, IP_ADDR_1, IP_ADDR_2, IP_ADDR_3);
    IP4_ADDR(&ip_info.gw, GW_ADDR_0, GW_ADDR_1, GW_ADDR_2, GW_ADDR_3);
    IP4_ADDR(&ip_info.netmask, NETMASK_0, NETMASK_1, NETMASK_2, NETMASK_3);
    esp_netif_set_ip_info(sta_netif, &ip_info);
    ESP_LOGI(TAG, "固定IP設定: %d.%d.%d.%d", IP_ADDR_0, IP_ADDR_1, IP_ADDR_2, IP_ADDR_3);
#endif

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {};
    strlcpy((char*)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strlcpy((char*)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi 初期化完了 (SSID: %s)", WIFI_SSID);
}

static void tcp_server_task(void *pvParameters) {
    uint8_t rx_chunk[2048];

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(TCP_PORT);

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "TCP ソケット作成失敗: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        ESP_LOGE(TAG, "TCP Bind 失敗: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    if (listen(listen_sock, 1) < 0) {
        ESP_LOGE(TAG, "TCP Listen 失敗: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "TCP サーバー起動完了 (Port: %d)", TCP_PORT);

    while (1) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "TCP Accept 失敗: errno %d", errno);
            break;
        }

        char addr_str[128];
        inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        ESP_LOGI(TAG, "TCP クライアント接続: %s", addr_str);

        size_t total_received = 0;
        size_t leftover_len = 0;
        bool header_checked = false;

        while (total_received < BUF_SIZE) {
            size_t to_recv = sizeof(rx_chunk) - leftover_len;
            if (to_recv > (BUF_SIZE - total_received + (header_checked ? 0 : 4))) {
                to_recv = BUF_SIZE - total_received + (header_checked ? 0 : 4);
            }

            int len = recv(sock, rx_chunk + leftover_len, to_recv, 0);
            if (len <= 0) {
                if (len < 0) ESP_LOGE(TAG, "TCP recv エラー: errno %d", errno);
                else ESP_LOGW(TAG, "TCP 切断 (受信: %zu bytes)", total_received);
                break;
            }

            size_t available_bytes = leftover_len + len;
            size_t data_offset = 0;

            if (!header_checked) {
                if (available_bytes >= 4) {
                    header_checked = true;
                    uint16_t duration_sec = 10;
                    if (rx_chunk[0] == 'N' && rx_chunk[1] == 'T') {
                        duration_sec = ((uint16_t)rx_chunk[2] << 8) | rx_chunk[3];
                        data_offset = 4;
                        ESP_LOGI(TAG, "TCP ヘッダー検出: 表示時間 %u 秒", duration_sec);
                    } else {
                        duration_sec = 10;
                        data_offset = 0;
                        ESP_LOGI(TAG, "TCP ヘッダーなし: デフォルト 10 秒");
                    }

                    if (s_lcd_mutex) xSemaphoreTake(s_lcd_mutex, portMAX_DELAY);
                    apply_display_duration(duration_sec);
                    lcd.startWrite();
                    lcd.setAddrWindow(0, 0, WIDTH, HEIGHT);
                    if (s_lcd_mutex) xSemaphoreGive(s_lcd_mutex);
                } else {
                    leftover_len = available_bytes;
                    continue;
                }
            }

            const uint8_t *pixel_src = rx_chunk + data_offset;
            size_t pixel_avail = available_bytes - data_offset;
            if (total_received + pixel_avail > BUF_SIZE) {
                pixel_avail = BUF_SIZE - total_received;
            }

            size_t pixel_bytes = pixel_avail & ~((size_t)1);

            if (pixel_bytes > 0) {
                if (s_lcd_mutex) xSemaphoreTake(s_lcd_mutex, portMAX_DELAY);
                lcd.writePixels((const lgfx::rgb565_t*)pixel_src, pixel_bytes / 2);
                if (s_lcd_mutex) xSemaphoreGive(s_lcd_mutex);

                total_received += pixel_bytes;
                leftover_len = (available_bytes - data_offset) - pixel_bytes;

                if (leftover_len > 0) {
                    memmove(rx_chunk, pixel_src + pixel_bytes, leftover_len);
                }
            } else {
                leftover_len = available_bytes - data_offset;
                if (data_offset > 0 && leftover_len > 0) {
                    memmove(rx_chunk, pixel_src, leftover_len);
                }
            }
        }

        if (s_lcd_mutex) xSemaphoreTake(s_lcd_mutex, portMAX_DELAY);
        lcd.endWrite();
        if (s_lcd_mutex) xSemaphoreGive(s_lcd_mutex);

        if (total_received >= BUF_SIZE) {
            ESP_LOGI(TAG, "TCP 画像データ全受信・描画完了 (%zu bytes)", total_received);
        }

        shutdown(sock, 0);
        close(sock);
    }

    close(listen_sock);
    vTaskDelete(NULL);
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
    lcd.drawString("WiFi/BT Init...", 10, 40);

    // バックライト消灯用の10秒タイマーを作成＆スタート
    s_bl_timer = xTimerCreate("bl_timer", pdMS_TO_TICKS(10000), pdFALSE, (void*)0, backlight_timer_cb);
    if (s_bl_timer) {
        xTimerStart(s_bl_timer, 0);
    }

    // Wi-Fi 初期化
    wifi_init_sta();

    // Bluetooth 初期化
    bt_init();

    // TCP サーバータスク作成
    xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "起動完了 (BT + WiFi Dual Mode / Stream Direct)");
}


