// #ifndef MQTT_MANAGER_H
// #define MQTT_MANAGER_H

// #include <PubSubClient.h>
// #include <WiFi.h>
// #include <vector>

// struct StoredMessage {
//     String topic;
//     String payload;
//     bool sent;
// };

// class MQTTManager {
// public:
//     MQTTManager(const char* broker, int port, const char* user, const char* password);
//     void initialize();
//     void loop();
//     bool publish(const char* topic, const char* payload);
//     bool publish(const char* topic, const String& payload1, const String& payload2);
//     void saveLocally(const String& topic, const String& payload);
//     void sendSavedData();

// private:
//     WiFiClient wifiClient;
//     PubSubClient client;
//     std::vector<StoredMessage> storedMessages;
// };

// #endif
