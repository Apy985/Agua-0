#include <esp_now.h>
#include <WiFi.h>

// Pines
const int sensorPin = 4;
const int pinR = 2;
const int pinG = 3;
const int pinB = 5;

// Umbrales
const int umbralBajo = 1000;
const int umbralIntermedio = 2000;

// MAC Address del ESP8266
uint8_t macESP8266[] = {0xBC, 0xDD, 0xC2, 0x7A, 0x3B, 0x97};

// Estructura de datos a enviar
typedef struct {
  int valorSensor;
  char nivel[12];
} DatosSensor;

DatosSensor datos;

String nivelAnterior = "";
unsigned long ultimoCambioLED = 0;
const unsigned long delayLED = 3000;

// Prototipos de funciones
void encenderColor(int r, int g, int b);
void apagarLED();

void onDataSent(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Estado envio ESP-NOW: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Exitoso" : "Fallido");
}

void setup() {
  Serial.begin(115200);

  pinMode(pinR, OUTPUT);
  pinMode(pinG, OUTPUT);
  pinMode(pinB, OUTPUT);
  apagarLED();

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error iniciando ESP-NOW");
    return;
  }
  Serial.println("ESP-NOW iniciado correctamente");

  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, macESP8266, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Error registrando peer");
    return;
  }
  Serial.println("Peer registrado correctamente");
}

void loop() {
  int valorSensor = analogRead(sensorPin);
  Serial.print("Valor sensor: ");
  Serial.println(valorSensor);

  String nivelActual;
  if (valorSensor < umbralBajo) {
    nivelActual = "BAJO";
  } else if (valorSensor < umbralIntermedio) {
    nivelActual = "INTERMEDIO";
  } else {
    nivelActual = "ALTO";
  }

  Serial.println("Nivel: " + nivelActual);

  // Cambio de LED cada 3 segundos
  if (nivelActual != nivelAnterior && (millis() - ultimoCambioLED >= delayLED)) {
    apagarLED();
    delay(50);

    if (nivelActual == "BAJO") {
      encenderColor(0, 255, 0);
    } else if (nivelActual == "INTERMEDIO") {
      encenderColor(255, 255, 0);
    } else {
      encenderColor(255, 0, 0);
    }

    nivelAnterior = nivelActual;
    ultimoCambioLED = millis();
  }

  // Enviar datos por ESP-NOW al ESP8266
  datos.valorSensor = valorSensor;
  nivelActual.toCharArray(datos.nivel, 12);
  esp_err_t resultado = esp_now_send(macESP8266, (uint8_t *)&datos, sizeof(datos));

  if (resultado == ESP_OK) {
    Serial.println("ESP-NOW: Enviado correctamente");
  } else {
    Serial.println("ESP-NOW: Error al enviar - codigo: " + String(resultado));
  }

  delay(500);
}

void encenderColor(int r, int g, int b) {
  analogWrite(pinR, r);
  analogWrite(pinG, g);
  analogWrite(pinB, b);
}

void apagarLED() {
  analogWrite(pinR, 0);
  analogWrite(pinG, 0);
  analogWrite(pinB, 0);
}