
Fuentes · MD
# FUENTES.md — Declaración de Fuentes e IA
## Proyecto: Agua-0 — Sistema de Monitoreo de Humedad Urbana
### TEI201 — Taller de Diseño en Ingeniería · UAI · Junio 2026
 
---
 
## 1. Librerías utilizadas
 
| Librería | Versión | Uso en el proyecto | Fuente |
|---|---|---|---|
| ESP8266WiFi | built-in | Conexión WiFi del nodo gateway ESP8266 | https://github.com/esp8266/Arduino |
| ESP8266HTTPClient | built-in | Envío de datos HTTP POST a Supabase desde ESP8266 | https://github.com/esp8266/Arduino |
| espnow | built-in | Comunicación inalámbrica entre ESP32-S3 y ESP8266 | https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html |
| ArduinoJson | 6.x | Serialización de datos en formato JSON para el POST a Supabase | https://arduinojson.org |
| Chart.js | 4.4.1 | Renderizado de gráficos en el dashboard web | https://cdnjs.cloudflare.com/ajax/libs/Chart.js/4.4.1/chart.umd.js |
 
---
 
## 2. Código externo adaptado
 
### Conexión WiFi con reintentos (gateway ESP8266)
- **Fuente:** https://randomnerdtutorials.com/esp32-useful-wifi-functions/
- **Adaptación:** Se modificó el tiempo de espera entre reintentos de 500ms a 1000ms y se agregó límite de 20 intentos para evitar bucles infinitos en zonas de señal débil.
### HTTP POST a Supabase REST API (gateway ESP8266)
- **Fuente:** Documentación oficial de Supabase REST API — https://supabase.com/docs/guides/api
- **Adaptación:** Se implementaron los headers requeridos (`apikey`, `Authorization: Bearer`, `Prefer: return=minimal`) y se construyó el body JSON con los campos específicos del proyecto.
### Recepción de datos ESP-NOW (gateway ESP8266)
- **Fuente:** Ejemplo oficial de Espressif — https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html
- **Adaptación:** Se adaptó el callback de recepción para extraer el valor de humedad del struct personalizado y acumularlo antes de enviarlo al servidor.
---
 
## 3. Uso de Inteligencia Artificial
 
### Dashboard web completo (`firmware/dashboard/index.html`)
- **Herramienta:** Claude Sonnet 4.6 (Anthropic, junio 2026)
- **Prompt:** *"Crea un dashboard HTML completo conectado a Supabase para monitoreo de humedad urbana. Debe mostrar: métricas por sector, gráfico histórico con selector 24h/7d/30d/año, pronóstico climático desde Open-Meteo para Santiago Chile, calendario de lluvias, gráfico comparativo anual y distribución de lecturas. Incluir cinta de alertas animada para sectores sobre 80%. Diseño oscuro."*
- **Adaptación:** Se ajustaron umbrales (80% desbordamiento, 65% elevado), intervalo de actualización a 30 segundos, y se reescribió usando funciones tradicionales en lugar de async/await para compatibilidad con el entorno de Vercel.
- **Comprensión:** El equipo comprende que el dashboard usa `fetch()` para consultar la API REST de Supabase filtrando por timestamp. Chart.js recibe arrays de labels y datasets. La cinta de alertas usa CSS `animation` con `translateX` para el scroll continuo.
### Estructura de base de datos y queries SQL
- **Herramienta:** Claude Sonnet 4.6 (Anthropic, junio 2026)
- **Prompt:** *"Dame el SQL para crear una tabla lecturas en Supabase con campos sector, timestamp, humedad_pct, llovio y precip_mm. También un INSERT masivo con generate_series para datos históricos de prueba de los últimos 6 meses."*
- **Adaptación:** Se corrigió el constraint de clave primaria, se ajustaron valores de humedad por rango estacional (meses 6-8 más húmedos para simular invierno chileno).
- **Comprensión:** `generate_series` produce timestamps cada 15 minutos; `CASE WHEN` asigna humedad distinta por sector con variación aleatoria mediante `random()`.
### Arquitectura general del sistema IoT
- **Herramienta:** Claude Sonnet 4.6 (Anthropic, junio 2026)
- **Uso:** Definición de la topología ESP32-S3 → ESP-NOW → ESP8266 → WiFi → Supabase, selección del stack gratuito (Supabase + Vercel), y cálculo de autonomía energética.
- **Adaptación:** El equipo validó la arquitectura contra los componentes físicos disponibles y ajustó el flujo según restricciones reales del hardware.
### Reorganización y comentarios del firmware ESP32-S3
- **Herramienta:** Claude Sonnet 4.6 (Anthropic, junio 2026)
- **Uso:** Revisión de estructura del código y mejora de comentarios explicativos por secciones.
- **Adaptación:** Los umbrales (UMBRAL_25 1100, UMBRAL_75 1900) fueron determinados por el equipo mediante calibración física. La MAC del ESP8266 fue obtenida directamente del hardware real.
- **Comprensión:** ESP-NOW requiere modo `WIFI_STA` activo aunque no haya conexión a red. El callback `onSent` es asíncrono. El struct `DatosSensor` se serializa como bytes raw al enviarse.
---
 
## 4. Servicios externos utilizados
 
| Servicio | Uso | URL |
|---|---|---|
| Supabase | Base de datos PostgreSQL en la nube (plan gratuito) | https://supabase.com |
| Vercel | Hosting del dashboard web (plan gratuito) | https://vercel.com |
| Open-Meteo | API de pronóstico climático gratuita para Santiago | https://open-meteo.com |
| Wokwi | Simulador y esquemático del circuito | https://wokwi.com/projects/462681394967646209 |
