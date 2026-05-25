#include "TinyBeacon.h"

#ifndef ARDUINO
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include <cstring>
#include <algorithm>
#include <cctype>

static const char* TAG = "TinyBeacon";

static EventGroupHandle_t wifi_event_group = nullptr;
const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_FAIL_BIT = BIT1;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Retrying WiFi connection...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        if (wifi_event_group) {
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

static const ble_uuid128_t gatt_svr_svc_uuid = {
    .u = { .type = BLE_UUID_TYPE_128 },
    .value = { 0xd0, 0x62, 0xd2, 0x94, 0xb1, 0x07, 0xe8, 0x90, 0x0f, 0x44, 0x77, 0xc9, 0x00, 0x11, 0x2a, 0x4a }
};

static const ble_uuid128_t gatt_svr_chr_wifi_uuid = {
    .u = { .type = BLE_UUID_TYPE_128 },
    .value = { 0xd0, 0x62, 0xd2, 0x94, 0xb1, 0x07, 0xe8, 0x90, 0x0f, 0x44, 0x77, 0xc9, 0x01, 0x11, 0x2a, 0x4a }
};

static const ble_uuid128_t gatt_svr_chr_custom_uuid = {
    .u = { .type = BLE_UUID_TYPE_128 },
    .value = { 0xd0, 0x62, 0xd2, 0x94, 0xb1, 0x07, 0xe8, 0x90, 0x0f, 0x44, 0x77, 0xc9, 0x02, 0x11, 0x2a, 0x4a }
};

static const ble_uuid128_t gatt_svr_chr_status_uuid = {
    .u = { .type = BLE_UUID_TYPE_128 },
    .value = { 0xd0, 0x62, 0xd2, 0x94, 0xb1, 0x07, 0xe8, 0x90, 0x0f, 0x44, 0x77, 0xc9, 0x03, 0x11, 0x2a, 0x4a }
};
static const ble_uuid128_t gatt_svr_chr_mac_uuid = {
    .u = { .type = BLE_UUID_TYPE_128 },
    .value = { 0xd0, 0x62, 0xd2, 0x94, 0xb1, 0x07, 0xe8, 0x90, 0x0f, 0x44, 0x77, 0xc9, 0x04, 0x11, 0x2a, 0x4a }
};

static uint16_t status_val_handle = 0;
static uint8_t g_status_val = 0;

static int gatt_svr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_uuid.u,
        .includes = NULL,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &gatt_svr_chr_wifi_uuid.u,
                .access_cb = gatt_svr_access,
                .arg = NULL,
                .descriptors = NULL,
                .flags = BLE_GATT_CHR_F_WRITE,
                .min_key_size = 0,
                .val_handle = NULL,
                .cpfd = NULL
            },
            {
                .uuid = &gatt_svr_chr_custom_uuid.u,
                .access_cb = gatt_svr_access,
                .arg = NULL,
                .descriptors = NULL,
                .flags = BLE_GATT_CHR_F_WRITE,
                .min_key_size = 0,
                .val_handle = NULL,
                .cpfd = NULL
            },
            {
                .uuid = &gatt_svr_chr_status_uuid.u,
                .access_cb = gatt_svr_access,
                .arg = NULL,
                .descriptors = NULL,
                .flags = (ble_gatt_chr_flags)(BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY),
                .min_key_size = 0,
                .val_handle = &status_val_handle,
                .cpfd = NULL
            },
            {
                .uuid = &gatt_svr_chr_mac_uuid.u,
                .access_cb = gatt_svr_access,
                .arg = NULL,
                .descriptors = NULL,
                .flags = BLE_GATT_CHR_F_READ,
                .min_key_size = 0,
                .val_handle = NULL,
                .cpfd = NULL
            },
            {
                .uuid = NULL,
                .access_cb = NULL,
                .arg = NULL,
                .descriptors = NULL,
                .flags = 0,
                .min_key_size = 0,
                .val_handle = NULL,
                .cpfd = NULL
            }
        },
    },
    {
        .type = 0,
        .uuid = NULL,
        .includes = NULL,
        .characteristics = NULL
    }
};

static int gatt_svr_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "Provisioner app connected over BLE");
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Provisioner app disconnected");
            wifi_ap_record_t ap_info;
            if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
                ESP_LOGI(TAG, "Not connected to WiFi; restarting advertising");
                TinyBeacon::getInstance()->startAdvertising();
            } else {
                ESP_LOGI(TAG, "Connected to WiFi; keeping BLE advertising stopped");
            }
            break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            ESP_LOGI(TAG, "Client subscribed to notifications");
            break;
    }
    return 0;
}

static int gatt_svr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg) {
    const ble_uuid_t *uuid = ctxt->chr->uuid;
    
    if (ble_uuid_cmp(uuid, &gatt_svr_chr_wifi_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
            std::string wifi_data(len, '\0');
            ble_hs_mbuf_to_flat(ctxt->om, &wifi_data[0], len, NULL);
            TinyBeacon::getInstance()->handleWifiWrite(wifi_data);
            return 0;
        }
    } else if (ble_uuid_cmp(uuid, &gatt_svr_chr_custom_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
            std::string custom_data(len, '\0');
            ble_hs_mbuf_to_flat(ctxt->om, &custom_data[0], len, NULL);
            TinyBeacon::getInstance()->handleCustomWrite(custom_data);
            return 0;
        }
    } else if (ble_uuid_cmp(uuid, &gatt_svr_chr_status_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            int rc = os_mbuf_append(ctxt->om, &g_status_val, sizeof(g_status_val));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
    } else if (ble_uuid_cmp(uuid, &gatt_svr_chr_mac_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            uint8_t mac[6];
            char macStr[18];
            if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
                sprintf(macStr, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            } else {
                strcpy(macStr, "00:00:00:00:00:00");
            }
            int rc = os_mbuf_append(ctxt->om, macStr, strlen(macStr));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
    }
    
    return BLE_ATT_ERR_REQ_NOT_SUPPORTED;
}

static void nimble_host_task(void *param) {
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

TinyBeacon::TinyBeacon() {
    _instance = this;
    _callback = nullptr;
    _isAdvertising = false;
}

void TinyBeacon::begin(const char* publisherId, const char* appId) {
    _publisherId = publisherId;
    _appId = appId;
    
    ESP_LOGI(TAG, "Initializing...");
    
    // Safe initialization of NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    
    if (isProvisioned()) {
        ESP_LOGI(TAG, "Device is already provisioned.");
        if (connectWiFi()) {
            if (_callback) {
                nvs_handle_t handle;
                if (nvs_open("smartconn", NVS_READONLY, &handle) == ESP_OK) {
                    size_t ssid_len = 0, pass_len = 0;
                    nvs_get_str(handle, "ssid", nullptr, &ssid_len);
                    nvs_get_str(handle, "password", nullptr, &pass_len);
                    
                    std::string ssid(ssid_len - 1, '\0');
                    std::string password(pass_len - 1, '\0');
                    
                    nvs_get_str(handle, "ssid", &ssid[0], &ssid_len);
                    nvs_get_str(handle, "password", &password[0], &pass_len);
                    nvs_close(handle);
                    
                    std::string custom = getCustomFields();
                    _callback(ssid.c_str(), password.c_str(), custom.c_str());
                }
            }
            return;
        }
    }
    
    setupBLE();
}

bool TinyBeacon::isProvisioned() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("smartconn", NVS_READONLY, &handle);
    if (err != ESP_OK) return false;
    
    uint8_t prov = 0;
    err = nvs_get_u8(handle, "prov", &prov);
    nvs_close(handle);
    return (err == ESP_OK && prov == 1);
}

bool TinyBeacon::connectWiFi(unsigned long timeoutMs) {
    nvs_handle_t handle;
    if (nvs_open("smartconn", NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    
    size_t ssid_len = 0, pass_len = 0;
    if (nvs_get_str(handle, "ssid", nullptr, &ssid_len) != ESP_OK ||
        nvs_get_str(handle, "password", nullptr, &pass_len) != ESP_OK) {
        nvs_close(handle);
        return false;
    }
    
    std::string ssid(ssid_len - 1, '\0');
    std::string password(pass_len - 1, '\0');
    nvs_get_str(handle, "ssid", &ssid[0], &ssid_len);
    nvs_get_str(handle, "password", &password[0], &pass_len);
    nvs_close(handle);
    
    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid.c_str());
    
    esp_err_t err;
    static bool wifi_init_done = false;
    static esp_netif_t* sta_netif = nullptr;
    
    if (!wifi_init_done) {
        err = esp_netif_init();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "esp_netif_init failed: %d", err);
            return false;
        }
        
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "esp_event_loop_create_default failed: %d", err);
            return false;
        }
        
        sta_netif = esp_netif_create_default_wifi_sta();
        if (!sta_netif) {
            sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        }
        
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        err = esp_wifi_init(&cfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_init failed: %d", err);
            return false;
        }
        
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);
        
        wifi_init_done = true;
    }
    
    wifi_event_group = xEventGroupCreate();
    
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, ssid.c_str(), sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, password.c_str(), sizeof(wifi_config.sta.password));
    
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(timeoutMs));
            
    bool success = (bits & WIFI_CONNECTED_BIT) != 0;
    vEventGroupDelete(wifi_event_group);
    wifi_event_group = nullptr;
    
    if (success) {
        ESP_LOGI(TAG, "WiFi Connected successfully.");
        return true;
    } else {
        ESP_LOGE(TAG, "WiFi Connection failed.");
        return false;
    }
}

void TinyBeacon::clearCredentials() {
    nvs_handle_t handle;
    if (nvs_open("smartconn", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_erase_key(handle, "ssid");
        nvs_erase_key(handle, "password");
        nvs_erase_key(handle, "custom");
        nvs_set_u8(handle, "prov", 0);
        nvs_commit(handle);
        nvs_close(handle);
    }
    ESP_LOGI(TAG, "NVS Credentials cleared.");
}

void TinyBeacon::onProvisioned(ProvisioningCallback callback) {
    _callback = callback;
}

void TinyBeacon::setStatus(uint8_t status) {
    g_status_val = status;
    if (status_val_handle != 0) {
        ble_gatts_chr_updated(status_val_handle);
    }
}

std::string TinyBeacon::getCustomFields() {
    nvs_handle_t handle;
    if (nvs_open("smartconn", NVS_READONLY, &handle) != ESP_OK) {
        return "{}";
    }
    
    size_t custom_len = 0;
    if (nvs_get_str(handle, "custom", nullptr, &custom_len) != ESP_OK) {
        nvs_close(handle);
        return "{}";
    }
    
    std::string custom(custom_len - 1, '\0');
    nvs_get_str(handle, "custom", &custom[0], &custom_len);
    nvs_close(handle);
    return custom;
}

void TinyBeacon::handleWifiWrite(const std::string& data) {
    if (data.length() == 0) return;
    
    ESP_LOGI(TAG, "Received WiFi credentials payload");
    
    size_t newlineIdx = data.find('\n');
    if (newlineIdx == std::string::npos) {
        ESP_LOGE(TAG, "Error: Invalid WiFi payload format (expected 'ssid\\npassword')");
        setStatus(3); // Failed
        return;
    }
    
    std::string ssid = data.substr(0, newlineIdx);
    std::string password = data.substr(newlineIdx + 1);
    
    // Trim
    auto trim = [](std::string& s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), s.end());
    };
    trim(ssid);
    trim(password);
    
    ESP_LOGI(TAG, "SSID: %s", ssid.c_str());
    
    // Store in NVS
    nvs_handle_t handle;
    if (nvs_open("smartconn", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_str(handle, "ssid", ssid.c_str());
        nvs_set_str(handle, "password", password.c_str());
        nvs_set_u8(handle, "prov", 1);
        nvs_commit(handle);
        nvs_close(handle);
    }
    
    // Trigger asynchronous WiFi connection task
    startWiFiConnectionTask(ssid, password);
}

void TinyBeacon::handleCustomWrite(const std::string& data) {
    ESP_LOGI(TAG, "Received custom fields: %s", data.c_str());
    
    nvs_handle_t handle;
    if (nvs_open("smartconn", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_str(handle, "custom", data.c_str());
        nvs_commit(handle);
        nvs_close(handle);
    }
}

void TinyBeacon::setupBLE() {
    std::string bleName = "DEV:" + _publisherId + ":" + _appId;
    ESP_LOGI(TAG, "Starting BLE under name: %s", bleName.c_str());
    
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init nimble port: %d", ret);
        return;
    }
    
    // Initialize GAP and GATT services
    ble_svc_gap_init();
    ble_svc_gatt_init();
    
    ble_hs_cfg.sync_cb = []() {
        TinyBeacon::getInstance()->startAdvertising();
    };
    
    int rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to count GATT svcs: %d", rc);
        return;
    }
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to add GATT svcs: %d", rc);
        return;
    }
    
    rc = ble_svc_gap_device_name_set(bleName.c_str());
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set device name: %d", rc);
        return;
    }
    
    nimble_port_freertos_init(nimble_host_task);
}

void TinyBeacon::startAdvertising() {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    struct ble_hs_adv_fields rsp_fields;
    int rc;

    // 1. Main advertisement packet (contains flags and 128-bit service UUID)
    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    fields.uuids128 = &gatt_svr_svc_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting advertisement fields; rc=%d", rc);
        return;
    }

    // 2. Scan response packet (contains the device name)
    memset(&rsp_fields, 0, sizeof(rsp_fields));
    std::string bleName = "DEV:" + _publisherId + ":" + _appId;
    rsp_fields.name = (uint8_t *)bleName.c_str();
    rsp_fields.name_len = bleName.length();
    rsp_fields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting scan response fields; rc=%d", rc);
        return;
    }

    // 3. Start advertising
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, gatt_svr_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "error enabling advertisement; rc=%d", rc);
        return;
    }
    
    _isAdvertising = true;
    ESP_LOGI(TAG, "BLE Advertising started.");
}

void TinyBeacon::stopAdvertising() {
    if (_isAdvertising) {
        ble_gap_adv_stop();
        _isAdvertising = false;
        ESP_LOGI(TAG, "BLE Advertising stopped.");
    }
}

void TinyBeacon::startWiFiConnectionTask(const std::string& ssid, const std::string& password) {
    _ssidToConnect = ssid;
    _passwordToConnect = password;
    
    xTaskCreate(
        wifiTask,
        "wifi_connect_task",
        4096,
        this,
        5,
        NULL
    );
}

void TinyBeacon::wifiTask(void* parameter) {
    TinyBeacon* self = (TinyBeacon*)parameter;
    
    self->setStatus(1); // Connecting
    ESP_LOGI(TAG, "Async WiFi Connection task started.");
    
    esp_wifi_disconnect();
    
    if (self->connectWiFi()) {
        self->setStatus(2); // Success
        self->stopAdvertising();
        if (self->_callback) {
            std::string customFields = self->getCustomFields();
            self->_callback(self->_ssidToConnect.c_str(), self->_passwordToConnect.c_str(), customFields.c_str());
        }
    } else {
        self->setStatus(3); // Failed
    }
    
    vTaskDelete(NULL);
}
#endif
