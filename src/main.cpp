#include "LittleFS.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include "driver/rtc_io.h" // For RTC_DATA_ATTR


// Motion sensor pins
#define SENSOR1_PIN 33
#define LED_PIN 2 

#define uS_TO_S_FACTOR 1000000ULL // Mikrosekunder til sekunder
#define SLEEP_DURATION 1 // Sleep varighed i sekunder (1 minut)

/// MQTT-broker parameters
const char *mqttBroker = "broker.hivemq.com";
const int mqttPort = 1883;
const char* mqttTopic = "plates/detected";
const char *mqtt_client_id = "ds18b20"; 
bool mqttNotActive = false;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Debounce timing
volatile unsigned long lastInterruptTime1 = 0;
const unsigned long debounceDelay = 5000; // 5 seconds

// Last motion time
#define INACTIVITY_TIMEOUT 30000 // 30 sekunder i millisekunder
unsigned long lastMotionTime = 0;
unsigned long lastMQTTRetryTime = 0;

// Web server on port 80
AsyncWebServer server(80);

// Parameters for Wi-Fi Credentials path in LittleFS
const char *ssidPath = "/ssid.txt";
const char *passPath = "/pass.txt";
const char *dataPath = "/data.txt";

// Parameters for Wi-Fi Credentials
const char *PARAM_INPUT_1 = "ssid";
const char *PARAM_INPUT_2 = "password";

String ssid;
String pass;
String data;

RTC_DATA_ATTR bool triggerAPMode = false;

// Function Prototypes
void IRAM_ATTR handleMotionSensor1();
void processPlate();
void initLittleFS();
bool initWiFi();
void setupWiFi();
void resetAP();
bool queryPlateScanner(String &plate, String &timestamp);
void writeFile(fs::FS &fs, const char *path, const char *message);
String readFile(fs::FS &fs, const char *path);
void sendToMQTT(const String &plate, const String &timestamp);
void reconnectMQTT();
void goToSleep();
void sendSavedData();
void appendFile(fs::FS &fs, const char *path, const char *message);
void handleMQTTConnectionStatus();


volatile bool motionDetected = false;


void setup() {
    Serial.begin(115200);
    Serial.println("Starter ESP32...");

    initLittleFS();

    ssid = readFile(LittleFS, ssidPath);
    pass = readFile(LittleFS, passPath);
    data = readFile(LittleFS, dataPath);

    // Initialize motion sensor
    pinMode(SENSOR1_PIN, INPUT_PULLDOWN);
    pinMode(LED_PIN, OUTPUT);
    attachInterrupt(digitalPinToInterrupt(SENSOR1_PIN), handleMotionSensor1, RISING);

    // Initialize WiFi
    if (!initWiFi()) {
        setupWiFi();
    }

    // Initialize MQTT
    mqttClient.setServer(mqttBroker, mqttPort);

    lastMotionTime = millis();

    Serial.println("Setup færdig.");
}

void loop() {
    if (triggerAPMode) {
        resetAP();
    }

    if (motionDetected) {
        motionDetected = false;
        processPlate();
        lastMotionTime = millis();
    }

    if ((millis() - lastMotionTime) > INACTIVITY_TIMEOUT) {
        goToSleep();
    }

    if (mqttNotActive && (millis() - lastMQTTRetryTime) > 60000) {
        reconnectMQTT();
        handleMQTTConnectionStatus();
    }
    else if (!mqttClient.connected()) {
        reconnectMQTT();
        handleMQTTConnectionStatus();
    }

    int sensorValue = digitalRead(SENSOR1_PIN);
    digitalWrite(LED_PIN, sensorValue ? LOW : HIGH);

    delay(100);
}

void  handleMQTTConnectionStatus() {
    if (mqttNotActive) {
        Serial.println("MQTT er ikke aktiv fra loop");
    }
    else {
        Serial.println("MQTT er aktiv fra loop");
        // Send saved data to MQTT
        sendSavedData();
        mqttClient.loop();
    }
}

void IRAM_ATTR handleMotionSensor1() {
    unsigned long currentTime = millis();
    if (currentTime - lastInterruptTime1 > debounceDelay) {
        lastInterruptTime1 = currentTime;
        motionDetected = true; // Set the flag for motion detection
    }
}

void processPlate() {
    Serial.println("Bevægelse registreret. Henter data...");
    String plate, timestamp;
    if (queryPlateScanner(plate, timestamp)) {
        Serial.printf("Valid plade: %s kl. %s\n", plate.c_str(), timestamp.c_str());
        sendToMQTT(plate, timestamp);
    } else {
        Serial.println("Ingen valid plade fundet.");
    }
}

bool queryPlateScanner(String &plate, String &timestamp) {
    const char *serverUrl = "http://192.168.1.142:5000/get_plate"; // Opdater med din Flask-servers IP

    WiFiClient client;
    HTTPClient http;

    if (http.begin(client, serverUrl)) {
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) { // HTTP status 200
            String payload = http.getString();
            Serial.println("Response from server: " + payload);

            int commaIndex = payload.indexOf(',');
            if (commaIndex != -1) {
                plate = payload.substring(0, commaIndex);
                timestamp = payload.substring(commaIndex + 1);
                http.end();
                return true;
            } else {
                Serial.println("Invalid format received.");
            }
        } else {
            Serial.printf("HTTP GET failed with error: %d\n", httpCode);
        }

        http.end();
    } else {
        Serial.println("Unable to connect to server.");
    }

    return false;
}

void sendToMQTT(const String &plate, const String &timestamp) {
    String message = "{\"plate\":\"" + plate + "\",\"timestamp\":\"" + timestamp + "\"}";

    if (mqttNotActive) {
        appendFile(LittleFS, dataPath, message.c_str() + String("\n"));
        Serial.println("MQTT er ikke aktiv. Data gemt lokalt.");
        return;
    }

    if (mqttClient.publish(mqttTopic, message.c_str())) {
        Serial.println("Data sendt til MQTT: " + message);
    } else {
        Serial.println("Fejl ved at sende data til MQTT.");
    }
}

void reconnectMQTT() {
    int retries = 0;
    lastMQTTRetryTime = millis();

    while (!mqttClient.connected()) {
        Serial.print("Forbinder til MQTT-broker...");
        if (mqttClient.connect(mqtt_client_id)) {
            Serial.println("Forbundet til MQTT!");
            mqttNotActive = false;
            return; 
        } else {
            Serial.print("Fejl under forbindelse til MQTT-broker\nFejl (kode: ");
            Serial.print(mqttClient.state());
            Serial.println("). Forsøger igen om 5 sekunder...");
            retries++;
            if (retries > 3) {
                mqttNotActive = true;
                Serial.println("MQTT connection fejlede. Vi prøvede 3 gange.");
                break;
            }
            delay(5000);
        }
    }
}


void initLittleFS() {
    if (!LittleFS.begin(true)) {
        Serial.println("Failed to mount LittleFS");
        return;
    }
    Serial.println("LittleFS mounted successfully");

    // Create default files if they do not exist
    if (!LittleFS.exists(ssidPath)) {
        writeFile(LittleFS, ssidPath, "");
    }
    if (!LittleFS.exists(passPath)) {
        writeFile(LittleFS, passPath, "");
    }
    if (!LittleFS.exists(dataPath)) {
        writeFile(LittleFS, dataPath, "");
    }
}

String readFile(fs::FS &fs, const char *path) {
    Serial.printf("Reading file: %s\n", path);
    File file = fs.open(path, "r");
    if (!file) {
        Serial.println("Failed to open file for reading");
        return String();
    }
    String content = file.readStringUntil('\n');
    file.close();
    return content;
}

void writeFile(fs::FS &fs, const char *path, const char *message) {
    Serial.printf("Writing file: %s\n", path);
    File file = fs.open(path, "w");
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    file.print(message);
    file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message) {
    Serial.printf("Appending to file: %s\n", path);
    File file = fs.open(path, FILE_APPEND); // Åbn filen i append-tilstand
    if (!file) {
        Serial.println("Failed to open file for appending");
        return;
    }
    file.print(message); // Tilføj beskeden til slutningen af filen
    file.close();
    Serial.println("Message appended successfully");
}


bool initWiFi() {
    if (ssid.isEmpty()) {
        Serial.println("Undefined SSID");
        return false;
    }

    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.println("Connecting to Wi-Fi...");

    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startAttemptTime > 10000) {
            Serial.println("Failed to connect to Wi-Fi");
            return false;
        }
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nConnected to Wi-Fi");
    Serial.println(WiFi.localIP());
    return true;
}

void setupWiFi() {
    WiFi.softAP("ESP-WIFI-MANAGER");
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/wifimanager.html", "text/html");
    });

    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
        int params = request->params();
        for (int i = 0; i < params; i++) {
            AsyncWebParameter *p = request->getParam(i);
            if (p->name() == PARAM_INPUT_1) {
                ssid = p->value();
                writeFile(LittleFS, ssidPath, ssid.c_str());
            } else if (p->name() == PARAM_INPUT_2) {
                pass = p->value();
                writeFile(LittleFS, passPath, pass.c_str());
            }
        }
        request->send(200, "text/plain", "Saved. Restarting...");
        delay(1000);
        ESP.restart();
    });

    server.begin();
}

void sendSavedData() {
    Serial.println("Sending saved data to MQTT...");

    if (WiFi.status() == WL_CONNECTED && mqttClient.connected()) {
        File file = LittleFS.open(dataPath, FILE_READ);

        if (!file || file.size() == 0) {
            Serial.println("No saved data to send.");
            return;
        }

        String remainingData = ""; // Bruges til at gemme data, der ikke blev sendt korrekt

        while (file.available()) {
            String line = file.readStringUntil('\n'); // Læs en linje
            line.trim(); // Fjern evt. whitespaces

            if (line.isEmpty()) {
                continue; // Ignorer tomme linjer
            }

            if (mqttClient.publish(mqttTopic, line.c_str())) {
                Serial.println("Data sent to MQTT: " + line);
            } else {
                Serial.println("Failed to send data to MQTT. Storing for retry.");
                remainingData += line + "\n"; // Gem linjen til genforsøg
            }
        }

        file.close();

        // Overskriv filen med kun de linjer, der ikke blev sendt korrekt
        File writeFile = LittleFS.open(dataPath, FILE_WRITE);
        if (writeFile) {
            writeFile.print(remainingData);
            writeFile.close();
        } else {
            Serial.println("Failed to update saved data file.");
        }
    } else {
        Serial.println("MQTT or WiFi not connected. Cannot send saved data.");
    }
}


void resetAP() {
    triggerAPMode = false;
    LittleFS.remove(ssidPath);
    LittleFS.remove(passPath);
    ESP.restart();
}

void goToSleep() {
    if (digitalRead(SENSOR1_PIN) == HIGH) {
        Serial.println("Motion sensor aktiv. Afventer.");
        return; 
    }

    Serial.println("Ingen aktivitet. Går i deep sleep...");
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, 1); // Wake-up på HIGH signal
    esp_deep_sleep_start();
}
