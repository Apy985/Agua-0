FUENTES.md — Declaración de Fuentes e IA
TEI201 — Taller de Diseño en Ingeniería · UAI · Junio 2026
Proyecto: Agua-0 — Sistema de Monitoreo de Humedad Urbana

## Librerías utilizadas
Librería            Versión      Uso en el proyecto                                                   Fuente
ESP8266WiFi         built-in     Conexión WiFi del nodo gateway ESP8266                               https://github.com/esp8266/Arduino
ESP8266HTTPClient   built-in     Envío de datos HTTP POST a Supabase desde ESP8266                    https://github.com/esp8266/Arduino
espnow              built-in     Comunicación inalámbrica entre ESP32-S3 y ESP8266                    https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html
ArduinoJson         6.x          Serialización de datos en formato JSON para el POST a Supabase       https://arduinojson.org
Chart.js            4.4.1        Renderizado de gráficos de línea, barra y dona en el dashboard web   https://cdnjs.cloudflare.com/ajax/libs/Chart.js/4.4.1/chart.umd.js

## Código externo adaptado
2. Código externo adaptado
Conexión WiFi con reintentos (gateway ESP8266)
Fuente: https://randomnerdtutorials.com/esp32-useful-wifi-functions/
Adaptación: Se modificó el tiempo de espera entre reintentos de 500ms a 1000ms y se agregó límite de 20 intentos para evitar bucles infinitos en zonas de señal débil.
HTTP POST a Supabase REST API (gateway ESP8266)
Fuente: Documentación oficial de Supabase REST API — https://supabase.com/docs/guides/api
Adaptación: Se implementaron los headers requeridos (apikey, Authorization: Bearer, Prefer: return=minimal) y se construyó el body JSON con los campos específicos del proyecto (sector, humedad_pct, llovio, precip_mm).
Recepción de datos ESP-NOW (gateway ESP8266)
Fuente: Ejemplo oficial de Espressif ESP-NOW — https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html
Adaptación: Se adaptó el callback de recepción para extraer el valor de humedad del struct personalizado y acumularlo para calcular el promedio cada 15 minutos antes de enviarlo al servidor.

## Uso de Inteligencia Artificial
Dashboard web completo (firmware/dashboard/index.html)
Herramienta: Claude Sonnet 4.6 (Anthropic, junio 2026)
Prompt utilizado: "Crea un dashboard HTML completo conectado a Supabase para monitoreo de humedad urbana. Debe mostrar: métricas por sector (humedad actual, alertas, días con lluvia), gráfico histórico con selector 24h/7d/30d/año, pronóstico climático desde Open-Meteo para Santiago Chile, calendario de lluvias del mes actual, gráfico comparativo anual y distribución de lecturas. Incluir cinta de alertas animada que muestre sectores sobre 80% de humedad. Usar diseño oscuro. Conectar a URL: https://ippzliasjmyhiulqbsgf.supabase.co/rest/v1 con API key provista."
Adaptación: Se ajustaron los umbrales de alerta (80% desbordamiento, 65% elevado) según el contexto del proyecto. Se modificó el intervalo de actualización a 30 segundos para mejor experiencia en demo. Se reescribió usando funciones tradicionales en lugar de async/await para compatibilidad. Se integraron las credenciales reales del proyecto.
Comprensión: El equipo comprende que el dashboard usa fetch() para consultar la API REST de Supabase, filtrando por timestamp según el rango seleccionado. Chart.js recibe arrays de labels y datasets para renderizar los gráficos. La cinta de alertas usa CSS animation con translateX para el scroll continuo.
Estructura de base de datos y queries SQL (Supabase)
Herramienta: Claude Sonnet 4.6 (Anthropic, junio 2026)
Prompt utilizado: "Dame el SQL para crear una tabla llamada lecturas en Supabase para almacenar datos de sensores de humedad por sector, con campos sector, timestamp, humedad_pct, llovio y precip_mm. También dame un INSERT masivo con generate_series para inyectar datos históricos de prueba de los últimos 6 meses con valores realistas para Chile."
Adaptación: Se ejecutó el SQL directamente en el editor de Supabase. Se corrigió el constraint de clave primaria que impedía insertar múltiples registros por sector. Se ajustaron los valores de humedad por rango estacional (meses 6-8 más húmedos para simular invierno chileno).
Comprensión: El equipo entiende que generate_series produce una serie temporal de timestamps cada 15 minutos, y el CASE WHEN asigna valores de humedad distintos por sector con variación aleatoria mediante random().
Arquitectura general del sistema IoT
Herramienta: Claude Sonnet 4.6 (Anthropic, junio 2026)
Uso: Definición de la topología ESP32-S3 → ESP-NOW → ESP8266 → WiFi → Supabase, selección del stack tecnológico gratuito (Supabase + Vercel), y cálculo de autonomía energética con batería LiPo AHB623450.
Adaptación: El equipo validó la arquitectura contra los componentes físicos disponibles (ESP32-S3 Super Mini, ESP8266 MOD) y ajustó el flujo según las restricciones reales del hardware.
Reorganización y comentarios del firmware ESP32-S3 (firmware/main.ino)
- Herramienta: Claude Sonnet 4.6 (Anthropic, junio 2026)
- Uso: Revisión de estructura del código, sugerencias de organización por secciones 
  (pines, umbrales, struct, callbacks, setup, loop) y mejora de comentarios explicativos.
- Adaptación: Los valores de umbrales (SENSOR_MAX 2500, UMBRAL_25 1100, UMBRAL_75 1900) 
  fueron determinados por el equipo mediante calibración física del sensor. La dirección 
  MAC del ESP8266 fue obtenida directamente del hardware real.
- Comprensión: El equipo entiende que ESP-NOW requiere modo WIFI_STA activo aunque no 
  haya conexión a red, que el callback onSent es asíncrono y que el struct DatosSensor 
  se serializa como bytes raw al enviarse.


##Servicios externos utilizados
Servicio     Uso                                                   URL
Supabase     Base de datos PostgreSQL en la nube (plan gratuito)   https://supabase.com
Vercel       Hosting del dashboard web (plan gratuito)             https://vercel.com
Open-Meteo   API de pronóstico climático gratuita para Santiago    https://open-meteo.com
Wokwi        Simulador y esquemático del circuito                  https://wokwi.com/projects/462681394967646209
