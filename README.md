# Agua-0 — Sistema de Monitoreo de Humedad Urbana

Sistema IoT de bajo costo para detectar zonas con riesgo de desbordamiento de aguas en la vía pública, mediante una red de sensores de nivel de agua distribuidos por sectores urbanos.

## Integrantes
- Martín Román *(Líder de proyecto)*
- José Tomás Said
- Matías González
- Sebastián Romero

**Curso:** TEI201 — Taller de Diseño en Ingeniería · UAI · 2026 Semestre 1

---

## Problema que resuelve

En sectores urbanos de Santiago, las lluvias intensas generan acumulación de agua en la vía pública sin que exista un sistema de alerta temprana. Agua-0 permite monitorear en tiempo real el nivel de humedad por sector, identificar zonas críticas y tomar decisiones preventivas antes de que ocurra el desbordamiento.

---

## Arquitectura del sistema

```
[ESP32-S3 Super Mini]  →  ESP-NOW  →  [ESP8266 MOD]  →  WiFi HTTPS  →  [Supabase]  →  [Dashboard Vercel]
     (sensor + LED)                      (gateway)                      (base de datos)     (visualización)
```

---

## Componentes de hardware

| Componente | Especificación | Cantidad |
|---|---|---|
| Microcontrolador sensor | ESP32-S3 Super Mini | 1 |
| Gateway WiFi | ESP8266 MOD | 1 |
| Sensor de nivel de agua | Sensor nivel de agua FZ0506 5-5V esp32 Arduino | 1 |
| LED RGB | Cátodo común | 1 |
| Batería | LiPo AHB623450 3.7V ~1200mAh | 1 |

---

## Estructura del repositorio

```
agua-0/
├── README.md
├── FUENTES.md
├── firmware/
│   ├── Codigo_esp32-s3_final.ino   # Firmware nodo sensor
│   ├── Codigo_esp8266.ino          # Firmware gateway WiFi
│   └── dashboard/
│       └── index.html              # Dashboard web
├── hardware/
│   ├── esquematico.png             # Diagrama de conexiones (Wokwi)
│   ├── BOM.md                      # Lista de materiales con costos
│   └── fotos/                      # Fotografías del prototipo
├── diseño-3d/
│   ├── encapsulado.f3d             # Modelo Fusion 360
│   ├── planos.pdf                  # Planos técnicos con cotas
│   └── renders/                    # Renders del encapsulado
├── testing/
│   └── protocolo_pruebas.md        # Protocolo y resultados de pruebas
└── docs/
    └── reporte_final.pdf
```

---

## Instrucciones para replicar el firmware

### Requisitos
- Arduino IDE 2.x
- Board: ESP32 by Espressif (versión 3.3.8) para el ESP32-S3
- Board: ESP8266 by ESP8266 Community para el ESP8266

### Librerías necesarias
- `esp_now.h` — incluida en el SDK ESP32
- `ESP8266HTTPClient.h` — incluida en el SDK ESP8266
- `WiFiClientSecureBearSSL` — incluida en el SDK ESP8266

### Pasos
1. Clonar este repositorio
2. Abrir `firmware/Codigo_esp32-s3_final.ino` en Arduino IDE
3. Seleccionar placa: **ESP32S3 Dev Module**
4. Cargar el firmware al ESP32-S3
5. Abrir `firmware/Codigo_esp8266.ino`
6. Reemplazar `TU_RED_WIFI` y `TU_PASSWORD` con las credenciales reales
7. Seleccionar placa: **Generic ESP8266 Module**
8. Cargar el firmware al ESP8266

### Conexiones ESP32-S3
| Componente | Pin ESP32-S3 |
|---|---|
| Sensor S (señal) | GPIO 4 |
| Sensor VCC | 3.3V |
| Sensor GND | GND |
| LED R | GPIO 2 |
| LED G | GPIO 3 |
| LED B | GPIO 5 |
| LED GND | GND |

---

## Dashboard

**URL:** https://agua-0.vercel.app/

El dashboard muestra en tiempo real:
- Humedad por sector con indicador de alerta
- Cinta de alertas animada para sectores sobre 80%
- Pronóstico climático de Santiago vía Open-Meteo
- Calendario de lluvias del mes actual
- Tendencia histórica con selector 24h / 7 días / 30 días / año
- Comparativa de días en alerta por año

Se actualiza automáticamente cada 30 segundos.

---

## Servicios utilizados

| Servicio | Uso | Plan |
|---|---|---|
| Supabase | Base de datos PostgreSQL en la nube | Gratuito |
| Vercel | Hosting del dashboard web | Gratuito |
| Open-Meteo | API de pronóstico climático | Gratuito |
