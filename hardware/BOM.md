# BOM — Lista de Materiales
## Proyecto Agua-0 · TEI201 · UAI · 2026

| # | Componente | Especificación | Cantidad | Costo unitario (CLP) | Costo total (CLP) |
|---|---|---|---|---|---|
| 1 | Microcontrolador sensor | ESP32-S3 Super Mini, 16MB Flash, 8MB PSRAM | 1 | $8.500 | $8.500 |
| 2 | Gateway WiFi | ESP8266 MOD, 802.11 b/g/n, 3.3V | 1 | $3.500 | $3.500 |
| 3 | Sensor de nivel de agua | Sensor nivel de agua FZ0506 5-5V esp32 Arduino | 1 | $1.200 | $1.200 |
| 4 | LED RGB | Cátodo común, 5mm, 20mA máx por canal | 1 | $200 | $200 |
| 5 | Batería LiPo | AHB623450, 3.7V, ~1200mAh | 1 | $4.500 | $4.500 |
| 6 | Cables jumper | Macho-macho y macho-hembra, 20cm | 10 | $80 | $800 |
| 7 | Protoboard | 400 puntos | 1 | $1.500 | $1.500 |
| | | | | **TOTAL** | **$20.200** |

---

## Justificación de componentes

**ESP32-S3 Super Mini** — Seleccionado por su bajo consumo en deep sleep (~20µA), soporte nativo para ESP-NOW, ADC de 12 bits para lectura precisa del sensor, y factor de forma compacto compatible con el encapsulado diseñado.

**ESP8266 MOD** — Actúa como gateway WiFi dedicado, separando la responsabilidad de red del nodo sensor. Permite que el ESP32-S3 opere en ciclos de bajo consumo mientras el ESP8266 mantiene la conexión WiFi permanente.

**Sensor de nivel de agua** — Sensor resistivo analógico de bajo costo, suficiente para detectar los 3 umbrales requeridos (bajo/intermedio/alto). Rango de operación 3.3V compatible con el ESP32-S3.

**LED RGB cátodo común** — Indicador visual inmediato del nivel de agua sin necesidad de pantalla. Verde/Amarillo/Rojo mapeado directamente a los 3 umbrales del sistema.

**Batería LiPo AHB623450** — Formato compacto (62×34×50mm) diseñado para caber en el encapsulado. Con el ciclo de muestreo de 500ms, autonomía estimada de 10–12 días continuos.

