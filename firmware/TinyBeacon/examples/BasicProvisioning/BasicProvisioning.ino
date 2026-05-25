#include <TinyBeacon.h>
#include <WiFi.h>
 
TinyBeacon connector;

// Callback function triggered when provisioning is completed successfully
void onDeviceProvisioned(const char* ssid, const char* password, const char* customFieldsJson) {
    Serial.println("--- DEVICE PROVISIONING COMPLETED CALLBACK ---");
    Serial.printf("Connected to SSID: %s\n", ssid);
    Serial.printf("Custom configuration payload: %s\n", customFieldsJson);
    Serial.println("---------------------------------------------");
    
    // You can now parse customFieldsJson using ArduinoJson and save specific values or configure your sensors!
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("Starting ESP32 Smart Connector Example...");
    
    // Attach the provisioned callback
    connector.onProvisioned(onDeviceProvisioned);
    
    // Initialize TinyBeacon with publisher ID "A1B2" and app ID "C3D4".
    // This will check if credentials exist:
    // - If yes: attempts to connect to WiFi.
    // - If no: starts advertising BLE name "DEV:A1B2:C3D4" and waits for provisioner app.
    // connector.begin("A1B2", "C3D4");
    connector.begin("7DFD", "8CE6");

}

void loop() {
    // If you want to allow resetting the credentials (e.g., via a physical button hold)
    // you can call:
    //   connector.clearCredentials();
    //   ESP.restart();
    
    // Your main loop code goes here
    if (WiFi.status() == WL_CONNECTED) {
        // Blink LED to show normal operation or run your main logic
        static unsigned long lastMsg = 0;
        if (millis() - lastMsg > 10000) {
            Serial.printf("Running normally. IP: %s\n", WiFi.localIP().toString().c_str());
            lastMsg = millis();
        }
    }
    delay(1000);
}
