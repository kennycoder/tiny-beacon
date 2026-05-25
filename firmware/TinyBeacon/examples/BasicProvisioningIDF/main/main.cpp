#include "TinyBeacon.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "main";

// Callback function triggered when provisioning is completed successfully
void onDeviceProvisioned(const char* ssid, const char* password, const char* customFieldsJson) {
    ESP_LOGI(TAG, "--- DEVICE PROVISIONING COMPLETED CALLBACK ---");
    ESP_LOGI(TAG, "Connected to SSID: %s", ssid);
    ESP_LOGI(TAG, "Custom configuration payload: %s", customFieldsJson);
    ESP_LOGI(TAG, "---------------------------------------------");
}

extern "C" void app_main() {
    // Delay to allow USB serial port to connect on host
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize TCP/IP and Event Loop (needed for WiFi config)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    ESP_LOGI(TAG, "Starting ESP32 Smart Connector Native ESP-IDF Example...");
    
    // Instantiate TinyBeacon
    static TinyBeacon connector;
    
    // Attach the provisioned callback
    connector.onProvisioned(onDeviceProvisioned);
    
    // Initialize TinyBeacon with publisher ID "A1B2" and app ID "C3D4".
    // This will check NVS for credentials:
    // - If yes: attempts to connect to WiFi and triggers callback on success.
    // - If no: starts advertising BLE name "DEV:A1B2:C3D4" and waits for provisioner app.
    connector.begin("7DFD", "8CE6");
    
    // Main loop task
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            ESP_LOGI(TAG, "Running normally. Custom fields: %s", connector.getCustomFields().c_str());
        } else {
            ESP_LOGI(TAG, "Waiting for WiFi connection or provisioning...");
        }
    }
}
