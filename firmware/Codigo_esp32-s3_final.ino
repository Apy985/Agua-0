/*
  ============================================================
  EMISOR ESP32-S3 - Sensor de Humedad + LED de estado
  ============================================================
  Función:
   1. NO se conecta a ningún router (ESP-NOW puro).
   2. Escanea los canales WiFi 1-13 escuchando el anuncio de canal
      que manda el ESP8266 receptor (ver código hermano).
   3. Al recibirlo, se fija en ese mismo canal y agrega al 8266 como peer.
   4. Lee el sensor de humedad cada 0.5s y lo envía por ESP-NOW.
   5. Si deja de recibir anuncios por un tiempo (el 8266 se reinició,
      o el router cambió de canal), vuelve a escanear automáticamente.
   6. Controla un LED RGB según el nivel de humedad.
   7. Si recibe una orden de deep sleep del ESP8266 (probabilidad de
      lluvia < 60%), entra en deep sleep por 12 horas para ahorrar batería.

  IMPORTANTE: este código y el del ESP8266 forman un par. El protocolo
  de mensajes (MSG_ANUNCIO_CANAL / MSG_LECTURA / MSG_ORDEN_SLEEP) debe
  coincidir EXACTO entre ambos archivos.
  ============================================================
*/

#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_sleep.h>

// ============================================================
// CONFIGURACIÓN DE PINES
// ============================================================
const int sensorPin = 4;
const int pinR = 2;
const int pinG = 3;
const int pinB = 5;

// ============================================================
// UMBRALES DE CLASIFICACIÓN
// ============================================================
const int umbralBajo = 800;
const int umbralIntermedio = 1000;

// ============================================================
// TIEMPOS
// ============================================================
const unsigned long intervaloEnvio       = 500;    // lectura+envío cada 0.5s
const unsigned long delayLED             = 3000;   // min. entre cambios de color
const unsigned long timeoutSinAnuncio    = 15000;  // 15s sin novedades -> re-escanear
const unsigned long tiempoPorCanalEscaneo = 400;   // ms de espera por canal al escanear

// Deep sleep de 12 horas cuando no hay lluvia esperada
// (12 * 60 * 60 * 1000000 microsegundos)
#define DEEP_SLEEP_US  (12ULL * 60 * 60 * 1000000)

unsigned long ultimoEnvio           = 0;
unsigned long ultimoCambioLED       = 0;
unsigned long ultimoAnuncioRecibido = 0;

String nivelAnterior = "";

// ============================================================
// PROTOCOLO INTERNO (debe coincidir EXACTO con el del 8266)
// ============================================================
typedef enum : uint8_t {
  MSG_ANUNCIO_CANAL = 0xA1,  // 8266 -> 32-S3 : "este es mi canal"
  MSG_LECTURA       = 0xA2,  // 32-S3 -> 8266 : datos de sensor
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
  uint8_t tipo;            // = MSG_ORDEN_SLEEP
  uint32_t duracion_seg;   // duración del sleep en segundos (info para logs)
} PaqueteOrdenSleep;

uint8_t macBroadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// MAC del ESP8266 receptor. Reemplazar con la MAC real de tu placa.
uint8_t macESP8266[] = {0xBC, 0xDD, 0xC2, 0x7A, 0x3B, 0x97};

// ============================================================
// ESTADO DE SINCRONIZACIÓN DE CANAL
// ============================================================
bool sincronizado       = false;
int  canalActual        = 1;
int  canalEscaneoActual = 1;
unsigned long ultimoSaltoEscaneo  = 0;
bool peerReceptorRegistrado       = false;

// ============================================================
// LED RGB
// ============================================================
void encenderColor(int r, int g, int b) {
  analogWrite(pinR, r);
  analogWrite(pinG, g);
  analogWrite(pinB, b);
}

void apagarLED() { encenderColor(0, 0, 0); }

// ============================================================
// Declaraciones adelantadas
// ============================================================
void fijarCanal(int canal);
void registrarPeerReceptor();
void avanzarEscaneo();
void leerYenviar(unsigned long ahora);
void entrarDeepSleep(uint32_t seg);

// ============================================================
// ESP-NOW: callbacks
// ============================================================
void onDataSent(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status) {
  // Diagnóstico opcional
}

void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (len < 1) return;
  uint8_t tipo = incomingData[0];

  if (tipo == MSG_ANUNCIO_CANAL && len == (int)sizeof(PaqueteAnuncioCanal)) {
    PaqueteAnuncioCanal pkt;
    memcpy(&pkt, incomingData, sizeof(pkt));
    ultimoAnuncioRecibido = millis();

    if (!sincronizado || pkt.canal != canalActual) {
      Serial.print("[Canal] Anuncio recibido. Canal: ");
      Serial.println(pkt.canal);
      fijarCanal(pkt.canal);
      registrarPeerReceptor();
      sincronizado = true;
    }
  }

  // ── NUEVO: orden de deep sleep enviada por el ESP8266 ──
  // El 8266 la manda si Open-Meteo indica < 60% de probabilidad de lluvia.
  if (tipo == MSG_ORDEN_SLEEP && len == (int)sizeof(PaqueteOrdenSleep)) {
    PaqueteOrdenSleep pkt;
    memcpy(&pkt, incomingData, sizeof(pkt));
    entrarDeepSleep(pkt.duracion_seg);
    // entrarDeepSleep() no retorna; el ESP32 se apaga.
  }
}

// ============================================================
// Fijar canal WiFi explícitamente
// ============================================================
void fijarCanal(int canal) {
  canalActual = canal;
  esp_wifi_set_channel(canal, WIFI_SECOND_CHAN_NONE);
}

// ============================================================
// Registrar al ESP8266 como peer en el canal actual
// ============================================================
void registrarPeerReceptor() {
  if (peerReceptorRegistrado) {
    esp_now_del_peer(macESP8266);
    peerReceptorRegistrado = false;
  }
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, macESP8266, 6);
  peerInfo.channel = canalActual;
  peerInfo.encrypt = false;
  peerInfo.ifidx   = WIFI_IF_STA;

  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    peerReceptorRegistrado = true;
    Serial.print("[Peer] ESP8266 registrado en canal ");
    Serial.println(canalActual);
  } else {
    Serial.println("[Peer] Error registrando ESP8266.");
  }
}

// ============================================================
// Escaneo de canales
// ============================================================
void avanzarEscaneo() {
  unsigned long ahora = millis();
  if (ahora - ultimoSaltoEscaneo < tiempoPorCanalEscaneo) return;
  ultimoSaltoEscaneo = ahora;
  canalEscaneoActual++;
  if (canalEscaneoActual > 13) canalEscaneoActual = 1;
  esp_wifi_set_channel(canalEscaneoActual, WIFI_SECOND_CHAN_NONE);
}

// ============================================================
// Deep sleep — apaga el ESP32-S3 por la duración indicada
// ============================================================
void entrarDeepSleep(uint32_t seg) {
  Serial.print("[Sleep] Entrando en deep sleep por ");
  Serial.print(seg / 3600);
  Serial.println(" horas. Sin lluvia esperada hoy.");

  // Parpadeo azul rápido como indicador visual antes de dormir
  for (int i = 0; i < 3; i++) {
    encenderColor(0, 0, 255); delay(200);
    apagarLED();              delay(200);
  }

  esp_now_deinit(); // liberar recursos antes de dormir
  apagarLED();

  // Despertar automático después de `seg` segundos
  esp_sleep_enable_timer_wakeup((uint64_t)seg * 1000000ULL);
  esp_deep_sleep_start(); // no retorna
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Emisor ESP32-S3 iniciando ===");

  pinMode(pinR, OUTPUT);
  pinMode(pinG, OUTPUT);
  pinMode(pinB, OUTPUT);
  apagarLED();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.print("MAC del ESP32-S3: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error iniciando ESP-NOW. Reiniciando en 3s...");
    delay(3000);
    ESP.restart();
  }
  Serial.println("ESP-NOW iniciado.");

  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  Serial.println("Escaneando canales en busca del ESP8266...");
  ultimoSaltoEscaneo = millis();
  esp_wifi_set_channel(canalEscaneoActual, WIFI_SECOND_CHAN_NONE);
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  unsigned long ahora = millis();

  if (!sincronizado) {
    avanzarEscaneo();
    return;
  }

  if (ahora - ultimoAnuncioRecibido > timeoutSinAnuncio) {
    Serial.println("[Canal] Sin anuncios. Volviendo a escanear...");
    sincronizado          = false;
    canalEscaneoActual    = canalActual;
    ultimoSaltoEscaneo    = ahora;
    return;
  }

  if (ahora - ultimoEnvio >= intervaloEnvio) {
    ultimoEnvio = ahora;
    leerYenviar(ahora);
  }
}

// ============================================================
// Lectura de sensor, LED y envío por ESP-NOW
// ============================================================
void leerYenviar(unsigned long ahora) {
  PaqueteLectura datos;
  datos.tipo        = MSG_LECTURA;
  datos.valorSensor = analogRead(sensorPin);

  String nivelActual;
  if      (datos.valorSensor < umbralBajo)        nivelActual = "BAJO";
  else if (datos.valorSensor <= umbralIntermedio)  nivelActual = "INTERMEDIO";
  else                                             nivelActual = "ALTO";

  memset(datos.nivel, 0, sizeof(datos.nivel));
  nivelActual.toCharArray(datos.nivel, sizeof(datos.nivel));

  Serial.print("Sensor: "); Serial.print(datos.valorSensor);
  Serial.print(" | Nivel: "); Serial.println(datos.nivel);

  if (nivelActual != nivelAnterior && (ahora - ultimoCambioLED >= delayLED)) {
    if      (nivelActual == "BAJO")        encenderColor(0, 255, 0);
    else if (nivelActual == "INTERMEDIO")  encenderColor(255, 165, 0);
    else                                   encenderColor(255, 0, 0);
    nivelAnterior     = nivelActual;
    ultimoCambioLED   = ahora;
  }

  esp_err_t res = esp_now_send(macESP8266, (uint8_t*)&datos, sizeof(datos));
  if (res != ESP_OK) Serial.println("[ESP-NOW] Error al enviar.");
}
