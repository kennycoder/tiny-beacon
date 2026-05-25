#include "TinyBeacon.h"

#ifdef ARDUINO
#include <WiFi.h>
#include <BLE2902.h>
#include <esp_mac.h>

#define SERVICE_UUID           "4a2a1100-c977-440f-90e8-07b194d262d0"
#define CHAR_WIFI_UUID         "4a2a1101-c977-440f-90e8-07b194d262d0"
#define CHAR_CUSTOM_UUID       "4a2a1102-c977-440f-90e8-07b194d262d0"
#define CHAR_STATUS_UUID       "4a2a1103-c977-440f-90e8-07b194d262d0"
#define CHAR_MAC_UUID          "4a2a1104-c977-440f-90e8-07b194d262d0"

// BLE Server callbacks to detect connection changes
class TinyBeaconServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        Serial.println("[TinyBeacon] Provisioner app connected over BLE");
    }

    void onDisconnect(BLEServer* pServer) {
        Serial.println("[TinyBeacon] Provisioner app disconnected");
        // Only restart advertising if not connected to WiFi
        if (WiFi.status() != WL_CONNECTED) {
            delay(500); // Give the stack time to collapse
            pServer->startAdvertising();
        }
    }
};

// BLE Characteristic Callback for WiFi Config
class WiFiConfigCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) {
        String value = String(pChar->getValue().c_str());
        if (value.length() == 0) return;
        
        Serial.println("[TinyBeacon] Received WiFi credentials payload");
        
        // Expected format: ssid\npassword
        int newlineIdx = value.indexOf('\n');
        if (newlineIdx == -1) {
            Serial.println("[TinyBeacon] Error: Invalid WiFi payload format (expected 'ssid\\npassword')");
            TinyBeacon::getInstance()->setStatus(3); // Failed
            return;
        }
        
        String ssid = value.substring(0, newlineIdx);
        String password = value.substring(newlineIdx + 1);
        
        // Remove trailing/leading whitespaces if any
        ssid.trim();
        password.trim();
        
        Serial.printf("[TinyBeacon] SSID: %s\n", ssid.c_str());
        
        // Store in Preferences (NVS)
        TinyBeacon::getInstance()->_prefs.begin("smartconn", false);
        TinyBeacon::getInstance()->_prefs.putString("ssid", ssid);
        TinyBeacon::getInstance()->_prefs.putString("password", password);
        TinyBeacon::getInstance()->_prefs.putBool("prov", true);
        TinyBeacon::getInstance()->_prefs.end();
        
        // Trigger asynchronous WiFi connection task
        TinyBeacon::getInstance()->startWiFiConnectionTask(ssid, password);
    }
};

// BLE Characteristic Callback for Custom Fields
class CustomConfigCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) {
        String value = String(pChar->getValue().c_str());
        Serial.printf("[TinyBeacon] Received custom fields: %s\n", value.c_str());
        
        // Store raw json in Preferences (NVS)
        TinyBeacon::getInstance()->_prefs.begin("smartconn", false);
        TinyBeacon::getInstance()->_prefs.putString("custom", value);
        TinyBeacon::getInstance()->_prefs.end();
    }
};

TinyBeacon::TinyBeacon() {
    _instance = this;
    _callback = nullptr;
    _isAdvertising = false;
}

void TinyBeacon::begin(const char* publisherId, const char* appId) {
    _publisherId = String(publisherId);
    _appId = String(appId);
    
    Serial.println("[TinyBeacon] Initializing...");
    
    // Check if we already have credentials stored
    _prefs.begin("smartconn", true);
    bool prov = _prefs.getBool("prov", false);
    _prefs.end();
    
    if (prov) {
        Serial.println("[TinyBeacon] Device is already provisioned.");
        // Try connecting automatically
        if (connectWiFi()) {
            return; 
        }
    }
    
    // If not provisioned or connection failed, startup BLE
    setupBLE();
}

bool TinyBeacon::isProvisioned() {
    _prefs.begin("smartconn", true);
    bool prov = _prefs.getBool("prov", false);
    _prefs.end();
    return prov;
}

bool TinyBeacon::connectWiFi(unsigned long timeoutMs) {
    _prefs.begin("smartconn", true);
    String ssid = _prefs.getString("ssid", "");
    String password = _prefs.getString("password", "");
    _prefs.end();
    
    if (ssid.length() == 0) {
        Serial.println("[TinyBeacon] No WiFi credentials stored in NVS.");
        return false;
    }
    
    Serial.printf("[TinyBeacon] Connecting to stored WiFi: %s\n", ssid.c_str());
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str());
    
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start < timeoutMs)) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[TinyBeacon] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    } else {
        Serial.println("\n[TinyBeacon] Connection timed out.");
        return false;
    }
}

void TinyBeacon::clearCredentials() {
    _prefs.begin("smartconn", false);
    _prefs.remove("ssid");
    _prefs.remove("password");
    _prefs.remove("custom");
    _prefs.putBool("prov", false);
    _prefs.end();
    Serial.println("[TinyBeacon] NVS Credentials cleared.");
}

void TinyBeacon::onProvisioned(ProvisioningCallback callback) {
    _callback = callback;
}

void TinyBeacon::setStatus(uint8_t status) {
    if (_pStatusChar) {
        _pStatusChar->setValue(&status, 1);
        _pStatusChar->notify();
    }
}

String TinyBeacon::getCustomFields() {
    _prefs.begin("smartconn", true);
    String custom = _prefs.getString("custom", "{}");
    _prefs.end();
    return custom;
}

void TinyBeacon::setupBLE() {
    // Temporarily modify MAC address to bypass aggressive GATT caches during development
    uint8_t baseMac[6];
    if (esp_read_mac(baseMac, ESP_MAC_WIFI_STA) == ESP_OK) {
        baseMac[5] ^= 0x02; // Toggle last bit to create a new MAC address
        esp_base_mac_addr_set(baseMac);
    }

    // Generate advertisement name: "DEV:A1B2:C3D4"
    String bleName = "DEV:" + _publisherId + ":" + _appId;
    
    Serial.printf("[TinyBeacon] Starting BLE under name: %s\n", bleName.c_str());
    Serial.println("[TinyBeacon] INITIALIZING BLE WITH CCCD DESCRIPTOR FIX");
    
    BLEDevice::init(bleName.c_str());
    
    // Set MTU to support longer writes (custom fields JSON)
    BLEDevice::setMTU(256);
    
    _pServer = BLEDevice::createServer();
    _pServer->setCallbacks(new TinyBeaconServerCallbacks());
    
    _pService = _pServer->createService(SERVICE_UUID);
    
    // WiFi config characteristic (Write)
    _pWifiChar = _pService->createCharacteristic(
        CHAR_WIFI_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    _pWifiChar->setCallbacks(new WiFiConfigCallbacks());
    
    // Custom JSON characteristic (Write)
    _pCustomChar = _pService->createCharacteristic(
        CHAR_CUSTOM_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    _pCustomChar->setCallbacks(new CustomConfigCallbacks());
    
    // Status characteristic (Read & Notify)
    _pStatusChar = _pService->createCharacteristic(
        CHAR_STATUS_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    _pStatusChar->addDescriptor(new BLE2902());
    
    // Initialize status to 0 (Idle)
    uint8_t initialStatus = 0;
    _pStatusChar->setValue(&initialStatus, 1);
    
    // MAC address characteristic (Read)
    _pMacChar = _pService->createCharacteristic(
        CHAR_MAC_UUID,
        BLECharacteristic::PROPERTY_READ
    );
    
    uint8_t mac[6];
    char macStr[18];
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
        sprintf(macStr, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        strcpy(macStr, "00:00:00:00:00:00");
    }
    _pMacChar->setValue(macStr);
    
    _pService->start();
    
    startAdvertising();
}

void TinyBeacon::startAdvertising() {
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    
    BLEDevice::startAdvertising();
    _isAdvertising = true;
    Serial.println("[TinyBeacon] BLE Advertising started.");
}

void TinyBeacon::stopAdvertising() {
    if (_isAdvertising) {
        BLEDevice::getAdvertising()->stop();
        _isAdvertising = false;
        Serial.println("[TinyBeacon] BLE Advertising stopped.");
    }
}

void TinyBeacon::startWiFiConnectionTask(String ssid, String password) {
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
    Serial.println("[TinyBeacon] Async WiFi Connection task started.");
    
    // Connect to WiFi
    WiFi.disconnect();
    WiFi.begin(self->_ssidToConnect.c_str(), self->_passwordToConnect.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[TinyBeacon] WiFi Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        self->setStatus(2); // Success
        self->stopAdvertising();
        
        // Trigger callback if registered
        if (self->_callback) {
            String customFields = self->getCustomFields();
            self->_callback(self->_ssidToConnect.c_str(), self->_passwordToConnect.c_str(), customFields.c_str());
        }
    } else {
        Serial.println("\n[TinyBeacon] WiFi Connection failed.");
        self->setStatus(3); // Failed
    }
    
    vTaskDelete(NULL);
}
#endif
