// #include "wifi_manager.h"
// #include <FS.h>
// #include <LittleFS.h> // Sørg for, at dette inkluderer ESP32-versionen

// String ssid;
// String password;
// AsyncWebServer server(80);

// void setupWiFi() {
//     if (!LittleFS.begin(true)) {
//         Serial.println("LittleFS fejlede. Starter AP-tilstand...");
//         startAccessPoint();
//         return;
//     }

//     ssid = "defaultSSID";
//     password = "defaultPassword";

//     WiFi.begin(ssid.c_str(), password.c_str());
//     Serial.println("Forbinder til Wi-Fi...");

//     if (WiFi.waitForConnectResult() != WL_CONNECTED) {
//         Serial.println("Wi-Fi fejlede. Starter AP-tilstand...");
//         startAccessPoint();
//     } else {
//         Serial.printf("Forbundet til Wi-Fi: %s\n", ssid.c_str());
//     }
// }

// void startAccessPoint() {
//     WiFi.softAP("ESP32_AP");

//     server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
//         request->send(LittleFS, "/wifimanager.html", "text/html");
//     });

//     server.on("/", HTTP_POST, [](AsyncWebServerRequest* request) {
//         int params = request->params();
//         for (int i = 0; i < params; i++) {
//             const AsyncWebParameter* p = request->getParam(i);
//             if (p->name() == "ssid") {
//                 ssid = p->value();
//             } else if (p->name() == "password") {
//                 password = p->value();
//             }
//         }
//         request->send(200, "text/plain", "Wi-Fi credentials gemt. Genstarter...");
//         delay(1000);
//         ESP.restart();
//     });

//     server.begin();
//     Serial.println("AP Mode aktiv.");
// }
