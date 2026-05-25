#ifndef SMART_CONNECTOR_H
#define SMART_CONNECTOR_H

#ifdef ARDUINO
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Preferences.h>
#else
#include <string>
#endif

typedef void (*ProvisioningCallback)(const char* ssid, const char* password, const char* customFieldsJson);

class TinyBeacon {
public:
    TinyBeacon();
    
    // Helper to get singleton instance
    static TinyBeacon* getInstance() { return _instance; }
    
    // Start BLE provisioning service.
    // publisherId: 4-character hex ID (e.g. "A1B2")
    // appId: 4-character hex ID (e.g. "C3D4")
    void begin(const char* publisherId, const char* appId);
    
    // Check if device is already provisioned
    bool isProvisioned();
    
    // Load credentials from NVS and connect to WiFi
    bool connectWiFi(unsigned long timeoutMs = 30000);
    
    // Clear credentials from NVS
    void clearCredentials();
    
    // Register custom callback when provisioning completes
    void onProvisioned(ProvisioningCallback callback);
    
    // Set current connection/provisioning status
    void setStatus(uint8_t status);

    // Get stored custom fields JSON
#ifdef ARDUINO
    String getCustomFields();
#else
    std::string getCustomFields();
#endif

private:
    static TinyBeacon* _instance;
    
#ifdef ARDUINO
    String _publisherId;
    String _appId;
    Preferences _prefs;
    ProvisioningCallback _callback;
    
    BLEServer* _pServer;
    BLEService* _pService;
    BLECharacteristic* _pWifiChar;
    BLECharacteristic* _pCustomChar;
    BLECharacteristic* _pStatusChar;
    BLECharacteristic* _pMacChar;
    
    bool _isAdvertising;
    
    String _ssidToConnect;
    String _passwordToConnect;
    
    void setupBLE();
    void startAdvertising();
    void stopAdvertising();
    void startWiFiConnectionTask(String ssid, String password);
    static void wifiTask(void* parameter);
    
    friend class WiFiConfigCallbacks;
    friend class CustomConfigCallbacks;
#else
    std::string _publisherId;
    std::string _appId;
    ProvisioningCallback _callback;
    bool _isAdvertising;
    
    std::string _ssidToConnect;
    std::string _passwordToConnect;
    
    void setupBLE();
    void startWiFiConnectionTask(const std::string& ssid, const std::string& password);
    static void wifiTask(void* parameter);
public:
    void startAdvertising();
    void stopAdvertising();
    void handleWifiWrite(const std::string& data);
    void handleCustomWrite(const std::string& data);
#endif
};

#endif // SMART_CONNECTOR_H

