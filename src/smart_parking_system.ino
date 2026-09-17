/*
  smart-parking-system
  ---------------------
  ESP32 + HC-SR04 ultrasonic sensors, one per parking spot.
  Each spot gets a status LED (red = occupied, green = free) and the
  board serves a small JSON API + auto-refreshing HTML dashboard over
  WiFi so spot status can be checked from any browser on the network.

  Hardware per spot:
    HC-SR04   TRIG -> ESP32 GPIO (output)
    HC-SR04   ECHO -> ESP32 GPIO (input, via a voltage divider —
                       HC-SR04 echo is 5V, ESP32 GPIOs are 3.3V-only)
    LED (red)    -> GPIO with a ~220ohm series resistor -> GND
    LED (green)  -> GPIO with a ~220ohm series resistor -> GND

  Detection logic:
    Mount the sensor facing down into the spot (e.g. on a ceiling
    bracket or a pole at the back of the space). A car parked in the
    spot shortens the measured distance well below the empty-spot
    baseline -> spot reported OCCUPIED.

  Board: any ESP32 dev board (tested pin numbers below assume a
  generic ESP32-WROOM-32 dev board — adjust PIN_* if yours differs).
*/

#include <WiFi.h>
#include <WebServer.h>

// ---------------------------------------------------------------
// Configuration — edit these for your network and hardware layout
// ---------------------------------------------------------------
const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const int NUM_SPOTS = 3;

// One TRIG/ECHO pair and one status LED pair per spot.
const int TRIG_PINS[NUM_SPOTS]      = {5, 18, 21};
const int ECHO_PINS[NUM_SPOTS]      = {19, 22, 23};
const int LED_RED_PINS[NUM_SPOTS]   = {25, 27, 32};
const int LED_GREEN_PINS[NUM_SPOTS] = {26, 14, 33};

// A spot is "occupied" once the measured distance drops below this,
// in centimeters. Calibrate for your mounting height: measure the
// empty-spot distance once and set this to roughly 60-70% of it.
const float OCCUPIED_THRESHOLD_CM = 60.0;

// Consecutive same-state readings required before flipping status,
// so a single noisy echo doesn't flap the reported state.
const int DEBOUNCE_READINGS = 3;

// ---------------------------------------------------------------

struct SpotState {
  bool occupied = false;
  int consecutiveMatches = 0;
  bool pendingState = false;
  float lastDistanceCm = -1;
};

SpotState spots[NUM_SPOTS];
WebServer server(80);

float measureDistanceCm(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // 30ms timeout ~= max range well beyond any parking-spot mounting
  // height, so a missing echo (no car, sensor pointed at open air
  // beyond range) resolves as "far away" rather than hanging.
  unsigned long durationUs = pulseIn(echoPin, HIGH, 30000UL);
  if (durationUs == 0) {
    return -1.0; // no echo received
  }
  // speed of sound ~343 m/s -> 0.0343 cm/us, round trip so /2
  return (durationUs * 0.0343f) / 2.0f;
}

void updateSpot(int i) {
  float d = measureDistanceCm(TRIG_PINS[i], ECHO_PINS[i]);
  spots[i].lastDistanceCm = d;

  bool readingOccupied = (d > 0 && d < OCCUPIED_THRESHOLD_CM);

  if (readingOccupied == spots[i].pendingState) {
    spots[i].consecutiveMatches++;
  } else {
    spots[i].pendingState = readingOccupied;
    spots[i].consecutiveMatches = 1;
  }

  if (spots[i].consecutiveMatches >= DEBOUNCE_READINGS) {
    spots[i].occupied = spots[i].pendingState;
  }

  digitalWrite(LED_RED_PINS[i], spots[i].occupied ? HIGH : LOW);
  digitalWrite(LED_GREEN_PINS[i], spots[i].occupied ? LOW : HIGH);
}

String buildStatusJson() {
  String json = "{\"spots\":[";
  for (int i = 0; i < NUM_SPOTS; i++) {
    if (i > 0) json += ",";
    json += "{\"id\":" + String(i);
    json += ",\"occupied\":" + String(spots[i].occupied ? "true" : "false");
    json += ",\"distance_cm\":" + String(spots[i].lastDistanceCm, 1);
    json += "}";
  }
  json += "]}";
  return json;
}

void handleRoot() {
  String html =
      "<!DOCTYPE html><html><head><meta charset='utf-8'>"
      "<meta http-equiv='refresh' content='3'>"
      "<title>Smart Parking Status</title>"
      "<style>"
      "body{font-family:sans-serif;background:#111;color:#eee;padding:24px}"
      "h1{margin-bottom:16px}"
      ".spot{display:inline-block;width:140px;margin:8px;padding:16px;"
      "border-radius:8px;text-align:center}"
      ".free{background:#1b5e20}.occupied{background:#7a1f1f}"
      "</style></head><body>"
      "<h1>Smart Parking — Live Status</h1>";

  for (int i = 0; i < NUM_SPOTS; i++) {
    html += "<div class='spot " + String(spots[i].occupied ? "occupied" : "free") + "'>";
    html += "<div>Spot " + String(i + 1) + "</div>";
    html += "<div>" + String(spots[i].occupied ? "OCCUPIED" : "FREE") + "</div>";
    html += "<div style='font-size:12px;opacity:0.7'>" +
             String(spots[i].lastDistanceCm, 1) + " cm</div>";
    html += "</div>";
  }
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleStatusJson() {
  server.send(200, "application/json", buildStatusJson());
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < NUM_SPOTS; i++) {
    pinMode(TRIG_PINS[i], OUTPUT);
    pinMode(ECHO_PINS[i], INPUT);
    pinMode(LED_RED_PINS[i], OUTPUT);
    pinMode(LED_GREEN_PINS[i], OUTPUT);
    digitalWrite(LED_GREEN_PINS[i], HIGH); // assume free until first reading
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected. IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/status", handleStatusJson);
  server.begin();
}

void loop() {
  for (int i = 0; i < NUM_SPOTS; i++) {
    updateSpot(i);
  }
  server.handleClient();
  delay(150); // ~6-7 sensor sweeps per second across all spots
}
