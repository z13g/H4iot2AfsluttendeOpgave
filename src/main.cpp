#include "LittleFS.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include "driver/rtc_io.h" // For RTC_DATA_ATTR

// Definer pins til bevægelsessensor og LED
#define SENSOR1_PIN 33 // Pin til bevægelsessensor
#define LED_PIN 2 // Pin til LED

#define uS_TO_S_FACTOR 1000000ULL // Mikrosekunder til sekunder
#define SLEEP_DURATION 1 // Deep sleep-varighed i sekunder

// MQTT-parametre
const char *mqttBroker = "broker.hivemq.com"; // MQTT-brokeradresse
const int mqttPort = 1883; // MQTT-port
const char* mqttTopic = "plates/detected"; // MQTT-emne
const char *mqtt_client_id = "ds18b20"; // MQTT-klient-id
bool mqttNotActive = false; // Indikator for MQTT-status

// Initialiser WiFi og MQTT-klient
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Debounce-parametre
volatile unsigned long lastInterruptTime1 = 0; // Tidspunkt for sidste interrupt
const unsigned long debounceDelay = 5000; // Debounce-tid i millisekunder

// Variabler til inaktivitet
#define INACTIVITY_TIMEOUT 10000 // Timeout i millisekunder
RTC_DATA_ATTR unsigned long lastMotionTime = 0; // Sidste bevægelsestid, gemt gennem deep sleep
unsigned long lastMQTTRetryTime = 0; // Tidspunkt for sidste MQTT-forsøg

// Webserver konfigureret til port 80
AsyncWebServer server(80);

// Filer i LittleFS til gemte data
const char *ssidPath = "/ssid.txt"; // Filsti til WiFi-SSID
const char *passPath = "/pass.txt"; // Filsti til WiFi-password
const char *dataPath = "/data.txt"; // Filsti til gemt data

// WiFi-parameternavne
const char *PARAM_INPUT_1 = "ssid"; // SSID-parameternavn
const char *PARAM_INPUT_2 = "password"; // Password-parameternavn

// Variabler til WiFi-data
String ssid; // Gemt SSID
String pass; // Gemt password
String data; // Gemt data

// Tidservere
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600; // Justér for din tidszone (GMT+1)
const int daylightOffset_sec = 3600; // Sommer-/vintertid

// Variabel til AP-mode status
RTC_DATA_ATTR bool triggerAPMode = false; // Indikator for AP-mode

// Funktionsprototyper
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
void setupRTC();

RTC_DATA_ATTR bool motionDetected = false; // Bevægelsesindikator gemt gennem deep sleep

// Setup-funktion initialiserer systemet
void setup() {
    Serial.begin(115200);
    Serial.println("Starter ESP32...");

    // Initialiser filsystemet
    initLittleFS();

    // Læs gemte WiFi-oplysninger fra LittleFS
    ssid = readFile(LittleFS, ssidPath);
    pass = readFile(LittleFS, passPath);
    data = readFile(LittleFS, dataPath);

    // Konfigurer bevægelsessensor og LED
    pinMode(SENSOR1_PIN, INPUT_PULLDOWN); // Sensor som input
    pinMode(LED_PIN, OUTPUT); // LED som output
    attachInterrupt(digitalPinToInterrupt(SENSOR1_PIN), handleMotionSensor1, RISING); // Interrupt for bevægelse

    // Tilslut WiFi eller start AP-mode
    if (!initWiFi()) {
        setupWiFi();
    }

    // Initialiser RTC
    setupRTC();

    // Konfigurer MQTT
    mqttClient.setServer(mqttBroker, mqttPort);

    // Check sensorstatus ved opstart
    if (digitalRead(SENSOR1_PIN) == HIGH) {
        Serial.println("Sensor aktiv ved opstart.");
        motionDetected = true; // Indstil bevægelsesflag
    }

    // Sæt sidste bevægelsestid hvis ikke allerede sat
    if (lastMotionTime == 0) {
        lastMotionTime = millis();
    }

    Serial.println("Setup færdig.");
}

// Loop-funktionen kører konstant
void loop() {
    // Håndter AP-mode
    if (triggerAPMode) {
        resetAP();
    }

    // Håndter bevægelsesdetektion
    if (motionDetected) {
        motionDetected = false; // Nulstil bevægelsesflag
        processPlate(); // Processer pladedata
        lastMotionTime = millis(); // Opdater sidste bevægelsestid
    }

    // Sæt ESP'en i deep sleep efter inaktivitet
    if ((millis() - lastMotionTime) > INACTIVITY_TIMEOUT) {
        goToSleep();
    }

    // Håndter MQTT-forbindelse
    if (mqttNotActive && (millis() - lastMQTTRetryTime) > 60000) {
        Serial.println("MQTT er ikke aktiv. Forsøger igen...");
        reconnectMQTT();
        handleMQTTConnectionStatus();
    } else if (!mqttClient.connected()) {
        reconnectMQTT();
        handleMQTTConnectionStatus();
    }

    // Tænd/sluk LED baseret på sensorstatus
    int sensorValue = digitalRead(SENSOR1_PIN);
    digitalWrite(LED_PIN, sensorValue ? LOW : HIGH);

    delay(100);
}

void setupRTC() {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        Serial.println("RTC initialiseret med NTP-tid:");
        Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    } else {
        Serial.println("Fejl under initialisering af RTC med NTP.");
    }
}

// Håndter MQTT-status
void handleMQTTConnectionStatus() {
    if (mqttNotActive) {
        Serial.println("MQTT er ikke aktiv.");
    } else {
        Serial.println("MQTT er aktiv.");
        sendSavedData(); // Send gemt data til MQTT
        mqttClient.loop(); // Hold MQTT-forbindelse aktiv
    }
}

// Interrupt-handler for bevægelsessensor
void IRAM_ATTR handleMotionSensor1() {
    unsigned long currentTime = millis();
    if (currentTime - lastInterruptTime1 > debounceDelay) {
        lastInterruptTime1 = currentTime;
        motionDetected = true; // Sæt bevægelsesflag
    }
}

// Processer pladedata
void processPlate() {
    Serial.println("Bevægelse registreret. Henter data...");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
        Serial.printf("Pladen blev registreret på: %s\n", timeStr);
    } else {
        Serial.println("Fejl under hentning af tid.");
    }

    String plate, timestamp;
    if (queryPlateScanner(plate, timestamp)) {
        Serial.printf("Gyldig plade: %s kl. %s\n", plate.c_str(), timestamp.c_str());
        sendToMQTT(plate, timestamp); // Send til MQTT
    } else {
        Serial.println("Ingen gyldig plade fundet.");
    }
}

// Forespørg pladescanner for data
bool queryPlateScanner(String &plate, String &timestamp) {
    const char *serverUrl = "http://192.168.0.185:5000/get_plate"; // Flask-serverens IP

    WiFiClient client;
    HTTPClient http;

    if (http.begin(client, serverUrl)) {
        int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) { // Tjek om HTTP-status er OK
            String payload = http.getString();
            Serial.println("Svar fra server: " + payload);

            int commaIndex = payload.indexOf(',');
            if (commaIndex != -1) {
                plate = payload.substring(0, commaIndex);
                timestamp = payload.substring(commaIndex + 1);
                http.end();
                return true;
            } else {
                Serial.println("Modtaget format er ugyldigt.");
            }
        } else {
            Serial.printf("HTTP GET fejlede med fejl: %d\n", httpCode);
        }

        http.end();
    } else {
        Serial.println("Kunne ikke forbinde til server.");
    }

    return false;
}

// Send data til MQTT
void sendToMQTT(const String &plate, const String &timestamp) {
    String message = "{\"plate\":\"" + plate + "\",\"timestamp\":\"" + timestamp + "\"}";


    if (mqttClient.publish(mqttTopic, message.c_str())) {
        Serial.println("Data sendt til MQTT: " + message);
    } else {
        Serial.println("Fejl ved at sende data til MQTT.");

        if (mqttNotActive) {
            String messageWithNewline = message + "\n";
            appendFile(LittleFS, dataPath, messageWithNewline.c_str());
            Serial.println("MQTT er ikke aktiv. Data gemt lokalt.");
            return;
        }
    }
}

// Genopret MQTT-forbindelse
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
                Serial.println("MQTT-tilslutning fejlede. Forsøg opgivet.");
                break;
            }
            delay(5000);
        }
    }
}

// Initialiser LittleFS
void initLittleFS() {
    if (!LittleFS.begin(true)) {
        Serial.println("Kunne ikke montere LittleFS");
        return;
    }
    Serial.println("LittleFS monteret succesfuldt.");

    // Opret standardfiler hvis de ikke findes
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

// Læs fil fra LittleFS
String readFile(fs::FS &fs, const char *path) {
    Serial.printf("Læser fil: %s\n", path);
    File file = fs.open(path, "r");
    if (!file) {
        Serial.println("Kunne ikke åbne fil til læsning.");
        return String();
    }
    String content = file.readStringUntil('\n');
    file.close();
    return content;
}

// Skriv til fil i LittleFS
void writeFile(fs::FS &fs, const char *path, const char *message) {
    Serial.printf("Skriver til fil: %s\n", path);
    File file = fs.open(path, "w");
    if (!file) {
        Serial.println("Kunne ikke åbne fil til skrivning.");
        return;
    }
    file.print(message);
    file.close();
}

// Tilføj til fil i LittleFS
void appendFile(fs::FS &fs, const char *path, const char *message) {
    Serial.printf("Tilføjer til fil: %s\n", path);
    File file = fs.open(path, FILE_APPEND);
    if (!file) {
        Serial.println("Kunne ikke åbne fil til tilføjelse.");
        return;
    }
    file.print(message);
    file.close();
    Serial.println("Data tilføjet til fil.");
}

// Initialiser WiFi
bool initWiFi() {
    if (ssid.isEmpty()) {
        Serial.println("SSID ikke defineret.");
        return false;
    }

    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.println("Forbinder til WiFi...");

    unsigned long startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startAttemptTime > 10000) {
            Serial.println("Kunne ikke oprette forbindelse til WiFi.");
            return false;
        }
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nForbundet til WiFi.");
    Serial.println(WiFi.localIP());
    return true;
}

// Konfigurer AP-mode
void setupWiFi() {
    WiFi.softAP("ESP-WIFI-MANAGER");
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP-adresse: ");
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
        request->send(200, "text/plain", "Gemte oplysninger. Genstarter...");
        delay(1000);
        ESP.restart();
    });

    server.begin();
}

// Send gemt data til MQTT
void sendSavedData() {
    Serial.println("Sender gemt data til MQTT...");

    if (WiFi.status() == WL_CONNECTED && mqttClient.connected()) {
        File file = LittleFS.open(dataPath, FILE_READ);

        if (!file || file.size() == 0) {
            Serial.println("Ingen gemt data at sende.");
            return;
        }

        String remainingData = "";

        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim();

            if (line.isEmpty()) {
                continue;
            }

            if (mqttClient.publish(mqttTopic, line.c_str())) {
                Serial.println("Data sendt til MQTT: " + line);
            } else {
                Serial.println("Fejl ved at sende data til MQTT. Gemmer til senere.");
                remainingData += line + "\n";
            }
        }

        file.close();

        File writeFile = LittleFS.open(dataPath, FILE_WRITE);
        if (writeFile) {
            writeFile.print(remainingData);
            writeFile.close();
        } else {
            Serial.println("Kunne ikke opdatere gemt data.");
        }
    } else {
        Serial.println("WiFi eller MQTT ikke forbundet. Kunne ikke sende data.");
    }
}

// Gendan til AP-mode
void resetAP() {
    triggerAPMode = false;
    LittleFS.remove(ssidPath);
    LittleFS.remove(passPath);
    ESP.restart();
}

// Sæt ESP'en i deep sleep
void goToSleep() {
    if (digitalRead(SENSOR1_PIN) == HIGH) {
        Serial.println("Bevægelsessensor aktiv. Afventer.");
        return;
    }

    Serial.println("Ingen aktivitet. Går i deep sleep...");
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, 1); // Wake-up ved HIGH signal
    esp_deep_sleep_start();
}
