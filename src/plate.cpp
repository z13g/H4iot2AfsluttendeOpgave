// #include "plate.h"
// // #include <ArduinoHttpClient.h>
// // #include <WiFiClient.h>

// // WiFiClient wifiClient;
// // HttpClient httpClient(wifiClient, "localhost", 5000);

// // bool queryPlateScanner(String& plate, String& timestamp) {
// //     httpClient.get("/get_plate");

// //     int statusCode = httpClient.responseStatusCode();
// //     if (statusCode == 200) {
// //         String payload = httpClient.responseBody();
// //         int separatorIndex = payload.indexOf(',');
// //         plate = payload.substring(0, separatorIndex);
// //         timestamp = payload.substring(separatorIndex + 1);
// //         return true;
// //     } else {
// //         Serial.println("Fejl ved forespørgsel til nummerpladescanner.");
// //         return false;
// //     }
// // }

// bool queryPlateScanner(String& plate, String& timestamp) {
//     plate = "AB12345";
//     timestamp = "2024-12-12 12:00:00";
//     return true;
// }