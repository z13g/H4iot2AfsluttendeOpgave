// #include "mqtt_manager.h"

// MQTTManager::MQTTManager(const char* broker, int port, const char* user, const char* password)
//     : client(wifiClient) {
//     client.setServer(broker, port);
// }

// void MQTTManager::initialize() {
//     client.setCallback([](char* topic, byte* payload, unsigned int length) {
//         Serial.printf("Modtog besked: %s\n", topic);
//     });
// }

// void MQTTManager::loop() {
//     if (!client.connected()) {
//         if (client.connect("ESP32Client")) {
//             Serial.println("Forbundet til MQTT broker.");
//         } else {
//             Serial.println("MQTT forbindelse fejlede.");
//         }
//     }
//     client.loop();
// }

// bool MQTTManager::publish(const char* topic, const char* payload) {
//     if (client.connected()) {
//         return client.publish(topic, payload);
//     }
//     return false;
// }

// bool MQTTManager::publish(const char* topic, const String& payload1, const String& payload2) {
//     String message = payload1 + "," + payload2;
//     return publish(topic, message.c_str());
// }

// void MQTTManager::saveLocally(const String& topic, const String& payload) {
//     StoredMessage msg = {topic, payload, false};
//     storedMessages.push_back(msg);
//     Serial.println("Data gemt lokalt.");
// }

// void MQTTManager::sendSavedData() {
//     if (WiFi.status() == WL_CONNECTED && client.connected()) {
//         for (auto& msg : storedMessages) {
//             if (publish(msg.topic.c_str(), msg.payload.c_str())) {
//                 msg.sent = true;
//             }
//         }
//         storedMessages.erase(std::remove_if(storedMessages.begin(), storedMessages.end(),
//                                             [](const StoredMessage& msg) { return msg.sent; }),
//                              storedMessages.end());
//     }
// }
