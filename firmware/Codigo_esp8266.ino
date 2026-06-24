#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

const char* ssid     = "TU_RED_WIFI";
const char* password = "TU_PASSWORD";

const char* serverURL = "https://ippzliasjmyhiulqbsgf.supabase.co/rest/v1/lecturas";
const char* apiKey    = "sb_publishable_4XfTiCH3K866tq1PtQ5eqg_WxK-L_fy";
const int sector = 0;

typedef struct __attribute__((packed)) {
  int valorSensor;
  char nivel[12];
} DatosSensor;

DatosSensor datosRecibidos;
bool datosPendientes = false;

float convertirAPorcentaje(int valor) {
  float p = (valor / 4095.0) * 100.0;
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return p;
}

void enviarASupabase(int valorSensor) {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    int i = 0;
    while (WiFi.status() != WL_CONNECTED && i < 20) {
      delay(500); i++;
    }
    if (WiFi.status() != WL_CONNECTED) return;
  }

  // Deshabilitar ESP-NOW temporalmente para liberar memoria
  esp_now_deinit();

  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  client.setBufferSizes(512, 512); // Reducir buffer para ahorrar RAM

  HTTPClient http;
  http.begin(client, serverURL);
  http.setTimeout(15000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", apiKey);
  http.addHeader("Authorization", "Bearer " + String(apiKey));
  http.addHeader("Prefer", "return=minimal");

  float humedad = convertirAPorcentaje(valorSensor);
  String payload = "{\"sector\":" + String(sector) + ",\"humedad_pct\":" + String(humedad, 1) + "}";

  Serial.print("Enviando: ");
  Serial.println(payload);

  int httpCode = http.POST(payload);
  Serial.print("Respuesta: ");
  Serial.println(httpCode);

  if (httpCode == 201) {
    Serial.println("Guardado correctamente en Supabase");
  } else {
    Serial.println("Error: " + http.getString());
  }

  http.end();
  client.stop();
  delay(500);

  // Reiniciar ESP-NOW después del envío
  esp_now_init();
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(onDataRecv);
}

void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  memcpy(&datosRecibidos, incomingData, sizeof(datosRecibidos));
  Serial.print("Recibido - Sensor: ");
  Serial.print(datosRecibidos.valorSensor);
  Serial.print(" | Nivel: ");
  Serial.println(datosRecibidos.nivel);
  datosPendientes = true;
}

void setup() {
  Serial.begin(9600);
  delay(2000);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != 0) {
    Serial.println("Error iniciando ESP-NOW");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(onDataRecv);
  Serial.println("ESP-NOW listo, esperando datos...");
}

void loop() {
  if (datosPendientes) {
    datosPendientes = false;
    enviarASupabase(datosRecibidos.valorSensor);
  }
  delay(100);
}
