# Tiny Beacon 

[![Live Demo](https://img.shields.io/badge/Live%20Demo-Try%20Tiny%20Beacon%20Now-06b6d4?style=for-the-badge&logo=google-cloud&logoColor=white)](https://tiny-beacon-145776547522.europe-west10.run.app/)

**Tiny Beacon** is a lightweight, full-stack IoT provisioning system designed to connect, provision, and register ESP32 devices over Bluetooth Low Energy (BLE) using a dynamic web client and a Go REST API backend. 

With Tiny Beacon, you can onboard new hardware onto your users' WiFi networks and deliver custom configurations (like room location, API keys, or operational intervals) in seconds.

---

## Tiny-Fast Integration (Only 3 Lines of Code!)

Integrating Tiny Beacon into your existing ESP32 firmware is extremely simple. You only need to declare the connector, register a success callback, and initialize it.

### Arduino Framework
Copy the `TinyBeacon` library folder to your Arduino libraries directory, then add this to your sketch:

```cpp
#include <TinyBeacon.h>

TinyBeacon connector;

void setup() {
    // 1. Define what happens when provisioning completes
    connector.onProvisioned([](const char* ssid, const char* password, const char* customFieldsJson) {
        Serial.printf("Connected to %s! Custom config: %s\n", ssid, customFieldsJson);
    });

    // 2. Start provisioning with your Publisher ID & App ID (generated in the backend dashboard)
    connector.begin("YOUR_PUBLISHER_ID", "YOUR_APP_ID"); 
}
```

### Native ESP-IDF Framework
```cpp
#include "TinyBeacon.h"

void app_main() {
    // Initialize NVS & network stack as usual...
    static TinyBeacon connector;

    // 1. Define provisioning callback
    connector.onProvisioned([](const char* ssid, const char* password, const char* customFieldsJson) {
        printf("Connected to %s! Config: %s\n", ssid, customFieldsJson);
    });

    // 2. Start advertising or connect using custom IDs from the backend dashboard
    connector.begin("YOUR_PUBLISHER_ID", "YOUR_APP_ID");
}
```

### How it Works:
1. **`connector.begin(PublisherID, AppID)`** checks the device's non-volatile storage (NVS) for saved WiFi credentials.
2. If credentials exist, it connects automatically.
3. If no credentials exist, it starts advertising a unique BLE name formatted as **`DEV:<PUBLISHER_ID>:<APP_ID>`** so the Web BLE companion client can pair with it, dynamically render custom inputs defined on your backend, and safely transmit the WiFi & custom parameters over GATT.

---

## Running the Included Examples

We provide two complete, runnable examples under the `firmware/TinyBeacon/examples/` folder.

> [!IMPORTANT]
> The pre-configured examples in this repository use the default example values: **Publisher ID: `7DFD`** and **App ID: `8CE6`**. Make sure to register an App in the Admin Dashboard and modify them with your own unique IDs.

### 1. Arduino / PlatformIO (`BasicProvisioning`)
* **PlatformIO (Recommended)**: Open `firmware/TinyBeacon/examples/BasicProvisioning` in VS Code with PlatformIO installed, and click **Upload**.
* **Arduino IDE**: Copy `firmware/TinyBeacon` into your Arduino libraries directory, open `BasicProvisioning.ino`, select your ESP32 board, and click **Upload**.

### 2. Native ESP-IDF (`BasicProvisioningIDF`)
* **PlatformIO (Recommended)**: Open `firmware/TinyBeacon/examples/BasicProvisioningIDF` in VS Code, which automatically manages the ESP-IDF toolchain. Click **Upload**.
* **ESP-IDF CLI**:
  ```bash
  cd firmware/TinyBeacon/examples/BasicProvisioningIDF
  idf.py build
  idf.py -p <PORT> flash monitor
  ```

---

## Setting Up the Backend

The Go backend serves the Web BLE Provisioner and the Admin Dashboard, storing app branding and device activation records in Google Cloud Firestore.

### 1. Prerequisites
- **Go 1.25.0+**
- **Firebase CLI** installed (`npm install -g firebase-tools`)

### 2. Start the Local Firestore Emulator & Go API Server

Start the Firestore emulator:
```bash
firebase emulators:start --only firestore
```
*Note: By default, the Firestore emulator binds to port `8080`.*

Launch the backend on a different port (e.g., `8081`) to avoid conflicts, pointing to the local emulator:
```bash
cd backend
export FIRESTORE_EMULATOR_HOST="127.0.0.1:8080"
export GOOGLE_CLOUD_PROJECT="your-gcp-project-id"
export FIRESTORE_DATABASE="(default)" # or your custom database ID
export JWT_SECRET="your-jwt-secret-key-here"
export PORT="8081"
go run main.go
```
The server will run on `http://localhost:8081`.

### 3. Create Your App Configuration
1. Open `http://localhost:8081` in your browser.
2. Register an administrator account and log in.
3. Click **Create New App** and fill out the details:
   - **App Name**: E.g., `Smart Thermostat`
   - **Branding Customization**: Upload a logo URL and pick an accent color (e.g., `#06b6d4`).
   - **Custom Schema**: Add fields your hardware expects, such as:
     - `sensor_interval` (Type: Number)
     - `device_location` (Type: Text)

- **App ID**: App ID will be generated for every new app you create.
- **Publisher ID**: Your Publisher ID is created once when you first sign up. It will be used for any app you create.


### 4. Deploying to Google Cloud Run

You can deploy the Go API server directly to Google Cloud Run from source. Cloud Run automatically compiles your Go project using the included `Dockerfile` and runs it securely:

```bash
gcloud run deploy tiny-beacon-backend \
    --source ./backend \
    --region us-central1 \
    --allow-unauthenticated \
    --set-env-vars="GOOGLE_CLOUD_PROJECT=your-gcp-project-id,FIRESTORE_DATABASE=your-database-id,JWT_SECRET=your-jwt-secret-key-here"
```

---

## Provisioning a Device

Once your ESP32 is flashing/running the firmware and the Go backend is running:

1. Open Chrome or Edge (browsers supporting Web Bluetooth) and go to your server address (e.g., `http://localhost:8081/web-ble` for local testing).
2. Click **Scan for Devices** and select your device matching your IDs: **`DEV:<PUBLISHER_ID>:<APP_ID>`** (e.g., `DEV:7DFD:8CE6` when testing the pre-configured examples).
3. The Web BLE application will fetch the branding and custom fields schema (SSID, Password, room location, interval, etc.) from your Go server and render a premium form.
4. Input your WiFi details and custom parameters, then click **Send Configuration**.
5. The browser will transmit the data over BLE GATT. Once the ESP32 connects, it replies with success, and the browser registers the activation on your Go backend!

---

## BLE Specifications & UUIDs

If you wish to build custom companion mobile apps (iOS/Android), you can write to the following characteristics on service `4a2a1100-c977-440f-90e8-07b194d262d0`:

| Characteristic UUID | Property | Format / Description |
|---|---|---|
| `4a2a1101-c977-440f-90e8-07b194d262d0` | Write-only | WiFi Config: `SSID\nPASSWORD` |
| `4a2a1102-c977-440f-90e8-07b194d262d0` | Write-only | Custom JSON payload strings |
| `4a2a1103-c977-440f-90e8-07b194d262d0` | Read & Notify | Connection Status: `1` (Connecting), `2` (Success), `3` (Invalid Credentials), `4` (Timeout) |
| `4a2a1104-c977-440f-90e8-07b194d262d0` | Read-only | Returns the device's MAC Address string |
