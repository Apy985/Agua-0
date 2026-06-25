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

  IMPORTANTE: este código y el del ESP8266 forman un par. El protocolo
  de mensajes (MSG_ANUNCIO_CANAL / MSG_LECTURA) debe coincidir EXACTO
  entre ambos archivos.
  ============================================================
*/

#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

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
const unsigned long intervaloEnvio = 500;       // lectura+envío cada 0.5s
const unsigned long delayLED = 3000;            // min. entre cambios de color
const unsigned long timeoutSinAnuncio = 15000;  // 15s sin novedades del 8266 -> re-escanear
const unsigned long tiempoPorCanalEscaneo = 400; // ms de espera en cada canal al escanear

unsigned long ultimoEnvio = 0;
unsigned long ultimoCambioLED = 0;
unsigned long ultimoAnuncioRecibido = 0;

String nivelAnterior = "";

// ============================================================
// PROTOCOLO INTERNO (debe coincidir EXACTO con el del 8266)
// ============================================================
typedef enum : uint8_t {
  MSG_ANUNCIO_CANAL = 0xA1,
  MSG_LECTURA       = 0xA2
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

uint8_t macBroadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// MAC del ESP8266 receptor. Cámbiala si tu placa tiene otra.
uint8_t macESP8266[] = {0xBC, 0xDD, 0xC2, 0x7A, 0x3B, 0x97};

// ============================================================
// ESTADO DE SINCRONIZACIÓN DE CANAL
// ============================================================
bool sincronizado = false;
int canalActual = 1;
int canalEscaneoActual = 1;
unsigned long ultimoSaltoEscaneo = 0;
bool peerReceptorRegistrado = false;

// ============================================================
// LED RGB
// ============================================================
void encenderColor(int r, int g, int b) {
  analogWrite(pinR, r);
  analogWrite(pinG, g);
  analogWrite(pinB, b);
}

void apagarLED() {
  encenderColor(0, 0, 0);
}

// ============================================================
// Declaraciones adelantadas (orden de funciones)
// ============================================================
void fijarCanal(int canal);
void registrarPeerReceptor();
void avanzarEscaneo();
void leerYenviar(unsigned long ahora);

// ============================================================
// ESP-NOW: callbacks
// ============================================================
void onDataSent(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status) {
  // Diagnóstico opcional; no es crítico para la lógica.
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Envío OK" : "Envío fallido");
}

void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (len < 1) return;
  uint8_t tipo = incomingData[0];

  if (tipo == MSG_ANUNCIO_CANAL && len == (int)sizeof(PaqueteAnuncioCanal)) {
    PaqueteAnuncioCanal pkt;
    memcpy(&pkt, incomingData, sizeof(pkt));

    ultimoAnuncioRecibido = millis();

    if (!sincronizado || pkt.canal != canalActual) {
      Serial.print("Anuncio de canal recibido del 8266. Canal indicado: ");
      Serial.println(pkt.canal);
      fijarCanal(pkt.canal);
      registrarPeerReceptor();
      sincronizado = true;
    }
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
  peerInfo.ifidx = WIFI_IF_STA;

  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    peerReceptorRegistrado = true;
    Serial.print("Peer del ESP8266 registrado en canal ");
    Serial.println(canalActual);
  } else {
    Serial.println("Error registrando peer del ESP8266.");
  }
}

// ============================================================
// Escaneo de canales (cuando no estamos sincronizados)
// ============================================================
void avanzarEscaneo() {
  unsigned long ahora = millis();
  if (ahora - ultimoSaltoEscaneo < tiempoPorCanalEscaneo) return;

  ultimoSaltoEscaneo = ahora;
  canalEscaneoActual++;
  if (canalEscaneoActual > 13) canalEscaneoActual = 1;

  esp_wifi_set_channel(canalEscaneoActual, WIFI_SECOND_CHAN_NONE);
  // No se imprime cada salto para no inundar el Serial; solo al sincronizar.
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n\n=== Emisor ESP32-S3 iniciando ===");

  pinMode(pinR, OUTPUT);
  pinMode(pinG, OUTPUT);
  pinMode(pinB, OUTPUT);
  apagarLED();

  // WiFi en modo estación, SIN conectarse a ningún router:
  // esto es ESP-NOW puro entre placas.
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.print("MAC del ESP32-S3: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error iniciando ESP-NOW. Reiniciando en 3s...");
    delay(3000);
    ESP.restart();
  }
  Serial.println("ESP-NOW iniciado correctamente.");

  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  Serial.println("Escaneando canales en busca del anuncio del ESP8266...");
  ultimoSaltoEscaneo = millis();
  esp_wifi_set_channel(canalEscaneoActual, WIFI_SECOND_CHAN_NONE);
}

// ============================================================
// LOOP
// ============================================================
void loop() {
  unsigned long ahora = millis();

  // --- Si no estamos sincronizados, seguir escaneando canales ---
  if (!sincronizado) {
    avanzarEscaneo();
    return; // no leer sensor ni enviar nada hasta sincronizar
  }

  // --- Si nos sincronizamos pero hace mucho que no hay novedades, re-escanear ---
  if (ahora - ultimoAnuncioRecibido > timeoutSinAnuncio) {
    Serial.println("Sin anuncios del 8266 hace tiempo. Volviendo a escanear canal...");
    sincronizado = false;
    canalEscaneoActual = canalActual; // arrancar el escaneo cerca de donde estábamos
    ultimoSaltoEscaneo = ahora;
    return;
  }

  // --- Lectura y envío periódico ---
  if (ahora - ultimoEnvio >= intervaloEnvio) {
    ultimoEnvio = ahora;
    leerYenviar(ahora);
  }
}

// ============================================================
// Lectura de sensor, control de LED y envío ESP-NOW
// ============================================================
void leerYenviar(unsigned long ahora) {
  PaqueteLectura datos;
  datos.tipo = MSG_LECTURA;
  datos.valorSensor = analogRead(sensorPin);

  String nivelActual;
  if (datos.valorSensor < umbralBajo) {
    nivelActual = "BAJO";
  } else if (datos.valorSensor <= umbralIntermedio) {
    nivelActual = "INTERMEDIO";
  } else {
    nivelActual = "ALTO";
  }

  memset(datos.nivel, 0, sizeof(datos.nivel));
  nivelActual.toCharArray(datos.nivel, sizeof(datos.nivel));

  Serial.print("Valor Sensor: ");
  Serial.print(datos.valorSensor);
  Serial.print(" | Nivel: ");
  Serial.println(datos.nivel);

  // Control de LED (no bloqueante, respeta el delay mínimo entre cambios)
  if (nivelActual != nivelAnterior && (ahora - ultimoCambioLED >= delayLED)) {
    if (nivelActual == "BAJO") {
      encenderColor(0, 255, 0);
    } else if (nivelActual == "INTERMEDIO") {
      encenderColor(255, 165, 0);
    } else {
      encenderColor(255, 0, 0);
    }
    nivelAnterior = nivelActual;
    ultimoCambioLED = ahora;
  }

  esp_err_t resultado = esp_now_send(macESP8266, (uint8_t*)&datos, sizeof(datos));
  if (resultado != ESP_OK) {
    Serial.println("Error al solicitar envío por ESP-NOW.");
  }
}
