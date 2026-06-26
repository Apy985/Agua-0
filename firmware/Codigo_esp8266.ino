/*
  ============================================================
  RECEPTOR ESP8266 - Sensor de Humedad -> Supabase + Control de Sleep
  ============================================================
  Función:
   1. Se conecta al WiFi del router.
   2. Consulta Open-Meteo al arrancar (y cada 1 hora) para saber si
      hay >= 60% de probabilidad de lluvia hoy en Santiago.
   3. Si NO hay lluvia esperada: manda orden de deep sleep de 12h
      al ESP32-S3 por ESP-NOW y deja de procesar lecturas.
   4. Si SÍ hay lluvia esperada: opera con normalidad, recibiendo
      lecturas del ESP32-S3 y subiéndolas a Supabase.
   5. Anuncia su canal WiFi por broadcast ESP-NOW cada 2s para que
      el ESP32-S3 pueda sincronizarse automáticamente.

  IMPORTANTE: este código y el del ESP32-S3 forman un par.
  ============================================================
*/

#include <ESP8266WiFi.h>
#include <espnow.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

// ============================================================
// CONFIGURACIÓN - EDITA AQUÍ ANTES DE SUBIR
// ============================================================
const char* ssid     = "TU_RED_WIFI";     // <-- Reemplazar con tu red
const char* password = "TU_PASSWORD";     // <-- Reemplazar con tu contraseña

const int ID_SECTOR = 0; // Número de sector (0, 1, 2...) para Supabase

const char* supabase_url = "https://ippzliasjmyhiulqbsgf.supabase.co/rest/v1/lecturas";
const char* apiKey       = "sb_publishable_4XfTiCH3K866tq1PtQ5eqg_WxK-L_fy";

// MAC del ESP32-S3 emisor.
// Sube primero el código del ESP32-S3, lee su MAC en el Monitor Serie, y ponla aquí.
uint8_t macESP32S3[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // <-- COMPLETAR

// ── Umbral de lluvia para activar el sistema ──────────────
// Si la probabilidad de lluvia es MENOR a este valor, se ordena deep sleep.
// Cambiar si se necesita un umbral distinto.
const int UMBRAL_LLUVIA_PCT = 60;

// Duración del deep sleep ordenado al ESP32-S3 (en segundos)
const uint32_t SLEEP_DURACION_SEG = 12 * 3600; // 12 horas

// ============================================================
// PROTOCOLO INTERNO (debe coincidir EXACTO con el del ESP32-S3)
// ============================================================
typedef enum : uint8_t {
  MSG_ANUNCIO_CANAL = 0xA1,  // 8266 -> 32-S3 : "este es mi canal WiFi"
  MSG_LECTURA       = 0xA2,  // 32-S3 -> 8266 : datos del sensor
  MSG_ORDEN_SLEEP   = 0xA3   // 8266 -> 32-S3 : "entra en deep sleep"
} TipoMensaje;

typedef struct __attribute__((packed)) {
  uint8_t tipo;
  uint8_t canal;
} PaqueteAnuncioCanal;

typedef struct __attribute__((packed)) {
  uint8_t tipo;
  int valorSensor;
  char nivel[12];
} PaqueteLectura;

typedef struct __attribute__((packed)) {
  uint8_t tipo;           // = MSG_ORDEN_SLEEP
  uint32_t duracion_seg;  // duración del sleep en segundos
} PaqueteOrdenSleep;

uint8_t macBroadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ============================================================
// VARIABLES DE ESTADO
// ============================================================
volatile bool datosPendientes = false;
PaqueteLectura lecturaRecibida;

unsigned long ultimoAnuncio     = 0;
const unsigned long intervaloAnuncio = 2000; // re-anunciar canal cada 2s

bool peerEmisorRegistrado = false;

// Control del pronóstico
bool  lluviaEsperada          = true;  // asume lluvia al arrancar; se actualiza con API
unsigned long ultimoChequeoClima = 0;
const unsigned long intervaloChequeoClima = 3600000UL; // revisar cada 1 hora

// ============================================================
// PRONÓSTICO CLIMÁTICO — Open-Meteo
// Consulta la probabilidad máxima de lluvia para hoy en Santiago.
// Retorna el porcentaje (0-100), o -1 si falla la conexión.
// ============================================================
int consultarProbabilidadLluvia() {
  const char* openMeteoUrl =
    "http://api.open-meteo.com/v1/forecast"
    "?latitude=-33.45&longitude=-70.67"
    "&daily=precipitation_probability_max"
    "&forecast_days=1"
    "&timezone=America%2FSantiago";

  WiFiClient client;
  HTTPClient http;

  if (!http.begin(client, openMeteoUrl)) {
    Serial.println("[Clima] No se pudo conectar a Open-Meteo.");
    return -1;
  }

  int code = http.GET();
  if (code != 200) {
    Serial.print("[Clima] HTTP error: "); Serial.println(code);
    http.end();
    return -1;
  }

  String payload = http.getString();
  http.end();

  // Parseo manual del JSON: busca "precipitation_probability_max":[NN]
  int idx = payload.indexOf("precipitation_probability_max\":[");
  if (idx < 0) {
    Serial.println("[Clima] No se encontró el campo de lluvia en la respuesta.");
    return -1;
  }
  idx += strlen("precipitation_probability_max\":[");
  int fin = payload.indexOf("]", idx);
  if (fin < 0) return -1;

  String valStr = payload.substring(idx, fin);
  valStr.trim();
  int prob = valStr.toInt();

  Serial.print("[Clima] Probabilidad de lluvia hoy: ");
  Serial.print(prob);
  Serial.println("%");

  return prob;
}

// ============================================================
// Enviar orden de deep sleep al ESP32-S3 por ESP-NOW
// ============================================================
void ordenarSleepAlEmisor() {
  PaqueteOrdenSleep pkt;
  pkt.tipo         = MSG_ORDEN_SLEEP;
  pkt.duracion_seg = SLEEP_DURACION_SEG;

  // Enviar tanto a broadcast como directo si se conoce la MAC
  esp_now_send(macBroadcast, (uint8_t*)&pkt, sizeof(pkt));

  bool macOk = false;
  for (int i = 0; i < 6; i++) { if (macESP32S3[i] != 0x00) { macOk = true; break; } }
  if (macOk) esp_now_send(macESP32S3, (uint8_t*)&pkt, sizeof(pkt));

  Serial.print("[Sleep] Orden enviada al ESP32-S3: dormir ");
  Serial.print(SLEEP_DURACION_SEG / 3600);
  Serial.println(" horas.");
}

// ============================================================
// Chequear clima y decidir si el sistema opera o duerme
// ============================================================
void chequearClima() {
  int prob = consultarProbabilidadLluvia();

  if (prob < 0) {
    // Si falla la API, no cambiar el estado anterior (fail-safe: seguir operando)
    Serial.println("[Clima] Error al consultar. Manteniendo estado anterior.");
    return;
  }

  if (prob >= UMBRAL_LLUVIA_PCT) {
    lluviaEsperada = true;
    Serial.println("[Clima] Lluvia esperada. Sistema activo.");
  } else {
    lluviaEsperada = false;
    Serial.print("[Clima] Sin lluvia esperada (");
    Serial.print(prob);
    Serial.print("% < ");
    Serial.print(UMBRAL_LLUVIA_PCT);
    Serial.println("%). Ordenando sleep al ESP32-S3.");
    ordenarSleepAlEmisor();
  }
}

// ============================================================
// ESP-NOW: inicialización / reinicialización
// ============================================================
bool iniciarEspNow() {
  if (esp_now_init() != 0) {
    Serial.println("[ESP-NOW] Error al iniciar.");
    return false;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_recv_cb(onDataRecv);
  esp_now_register_send_cb(onDataSent);
  esp_now_add_peer(macBroadcast, ESP_NOW_ROLE_COMBO, WiFi.channel(), NULL, 0);
  peerEmisorRegistrado = false;
  Serial.println("[ESP-NOW] Inicializado.");
  return true;
}

void registrarPeerEmisorSiHaceFalta() {
  bool macOk = false;
  for (int i = 0; i < 6; i++) { if (macESP32S3[i] != 0x00) { macOk = true; break; } }
  if (!macOk) return;
  if (!peerEmisorRegistrado) {
    esp_now_add_peer(macESP32S3, ESP_NOW_ROLE_COMBO, WiFi.channel(), NULL, 0);
    peerEmisorRegistrado = true;
    Serial.println("[ESP-NOW] Peer del ESP32-S3 registrado.");
  }
}

void anunciarCanal() {
  PaqueteAnuncioCanal pkt;
  pkt.tipo  = MSG_ANUNCIO_CANAL;
  pkt.canal = (uint8_t)WiFi.channel();
  esp_now_send(macBroadcast, (uint8_t*)&pkt, sizeof(pkt));
  bool macOk = false;
  for (int i = 0; i < 6; i++) { if (macESP32S3[i] != 0x00) { macOk = true; break; } }
  if (macOk) esp_now_send(macESP32S3, (uint8_t*)&pkt, sizeof(pkt));
}

// ============================================================
// ESP-NOW: callbacks
// ============================================================
void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  if (len < 1) return;
  uint8_t tipo = incomingData[0];
  if (tipo == MSG_LECTURA && len == sizeof(PaqueteLectura)) {
    memcpy(&lecturaRecibida, incomingData, sizeof(PaqueteLectura));
    datosPendientes = true;
  }
}

void onDataSent(uint8_t *mac_addr, uint8_t status) { }

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(9600);
  Serial.println("\n=== Receptor ESP8266 iniciando ===");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nConectado.");
  Serial.print("MAC del ESP8266: "); Serial.println(WiFi.macAddress());
  Serial.print("Canal WiFi: ");      Serial.println(WiFi.channel());

  iniciarEspNow();
  registrarPeerEmisorSiHaceFalta();

  // Primer chequeo de clima al arrancar
  chequearClima();
  ultimoChequeoClima = millis();

  Serial.println("Sistema listo.");
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  unsigned long ahora = millis();

  registrarPeerEmisorSiHaceFalta();

  // Anunciar canal periódicamente (el ESP32-S3 lo necesita para sincronizarse)
  if (ahora - ultimoAnuncio >= intervaloAnuncio) {
    ultimoAnuncio = ahora;
    anunciarCanal();
  }

  // Revisar pronóstico cada 1 hora
  if (ahora - ultimoChequeoClima >= intervaloChequeoClima) {
    ultimoChequeoClima = ahora;
    chequearClima();
  }

  // Procesar lectura solo si hay lluvia esperada
  if (datosPendientes) {
    datosPendientes = false;
    if (lluviaEsperada) {
      procesarYsubirLectura();
    } else {
      // Ignorar lecturas si no hay lluvia; re-enviar orden de sleep por si acaso
      Serial.println("[Loop] Lectura ignorada — sin lluvia esperada. Re-enviando orden sleep.");
      ordenarSleepAlEmisor();
    }
  }
}

// ============================================================
// SUBIDA A SUPABASE
// ============================================================
void procesarYsubirLectura() {
  Serial.print("Recibido -> Sensor: "); Serial.print(lecturaRecibida.valorSensor);
  Serial.print(" | Nivel: ");           Serial.println(lecturaRecibida.nivel);

  int humedad_pct = map(lecturaRecibida.valorSensor, 0, 4095, 0, 100);
  humedad_pct = constrain(humedad_pct, 0, 100);

  // Pausar ESP-NOW para liberar RAM necesaria para el handshake TLS
  esp_now_unregister_recv_cb();
  esp_now_unregister_send_cb();
  esp_now_deinit();
  Serial.println("[HTTPS] ESP-NOW pausado para subida...");

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  client->setBufferSizes(512, 512);

  HTTPClient https;
  if (https.begin(*client, supabase_url)) {
    https.addHeader("Content-Type",  "application/json");
    https.addHeader("apikey",        apiKey);
    https.addHeader("Authorization", String("Bearer ") + apiKey);
    https.addHeader("Prefer",        "return=minimal");

    String body = "{\"sector\":" + String(ID_SECTOR) +
                  ",\"humedad_pct\":" + String(humedad_pct) + "}";

    Serial.print("[HTTPS] POST: "); Serial.println(body);
    int code = https.POST(body);

    if (code > 0) {
      Serial.print("[HTTPS] Respuesta: "); Serial.println(code);
      if (code != 201) Serial.println(https.getString());
    } else {
      Serial.print("[HTTPS] Error: "); Serial.println(https.errorToString(code));
    }
    https.end();
  } else {
    Serial.println("[HTTPS] No se pudo conectar al servidor.");
  }

  // Reactivar ESP-NOW
  iniciarEspNow();
  registrarPeerEmisorSiHaceFalta();
}
