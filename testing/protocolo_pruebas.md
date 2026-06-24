# Protocolo de Pruebas — Agua-0
## TEI201 · UAI · Junio 2026

---

## 1. Objetivo
Validar que el sistema Agua-0 captura datos reales de nivel de agua, los transmite correctamente a la nube y los visualiza en el dashboard, respondiendo al problema de detección de zonas con riesgo de desbordamiento urbano.

---

## 2. Condiciones de prueba
- **Lugar:** Laboratorio DEC, UAI
- **Fecha:** 24-25 junio 2026Abril - Junio 2026
- ** Hotspot celular (2.4GHz)
- **Hardware:** ESP32-S3 Super Mini + ESP8266 MOD + sensor de nivel de agua
- **Software:** Arduino IDE 2.x, SDK ESP32 v3.3.8, SDK ESP8266 Community

---

## 3. Pruebas realizadas

### Prueba 1 — Lectura del sensor
**Objetivo:** Verificar que el sensor entrega valores analógicos correctos en los 3 niveles.

**Procedimiento:**
1. Conectar el sensor al ESP32-S3 según el esquemático
2. Abrir el Monitor Serie a 115200 baudios
3. Sumergir el sensor progresivamente en agua
4. Registrar el valor ADC en cada nivel

**Criterio de éxito:** Valores ADC distintos y estables para cada nivel físico.

**Resultados:**

| Nivel físico | Valor ADC medido | Umbral definido | Resultado |
|---|---|---|---|
| Sin agua | < 200 | < 1000 (BAJO) | ✅ Correcto |
| Agua parcial | ~1400 | 1000–2000 (INTERMEDIO) | ✅ Correcto |
| Agua completa | > 2200 | > 2000 (ALTO) | ✅ Correcto |

---

### Prueba 2 — Control del LED RGB
**Objetivo:** Verificar que el LED cambia de color según el umbral detectado.

**Procedimiento:**
1. Con el sistema activo, variar el nivel de agua manualmente
2. Observar el color del LED en cada transición
3. Verificar el delay de 3 segundos entre cambios

**Criterio de éxito:** Verde = BAJO, Amarillo = INTERMEDIO, Rojo = ALTO, con transición de mínimo 3 segundos.

**Resultados:**

| Estado | Color esperado | Color observado | Delay respetado | Resultado |
|---|---|---|---|---|
| BAJO | Verde | Verde | — | ✅ |
| INTERMEDIO | Amarillo | Amarillo | 3s | ✅ |
| ALTO | Rojo | Rojo | 3s | ✅ |

---

### Prueba 3 — Comunicación ESP-NOW
**Objetivo:** Verificar que el ESP32-S3 envía datos al ESP8266 correctamente.

**Procedimiento:**
1. Encender ambos dispositivos
2. Abrir Monitor Serie del ESP8266 a 9600 baudios
3. Variar el nivel de agua en el sensor
4. Verificar que los datos llegan al ESP8266

**Criterio de éxito:** El ESP8266 imprime en Monitor Serie los valores recibidos cada 500ms.

**Resultados:**
- Latencia de recepción: < 50ms
- Tasa de éxito de envío: ~95% (pérdida ocasional por interferencia)
- Los campos `valorSensor` y `nivel` llegan correctamente

---

### Prueba 4 — Envío a Supabase
**Objetivo:** Verificar que los datos se almacenan correctamente en la base de datos en la nube.

**Procedimiento:**
1. Con el sistema activo y WiFi conectado
2. Observar el Monitor Serie del ESP8266
3. Verificar código de respuesta HTTP 201
4. Confirmar en Supabase Table Editor que el registro apareció

**Criterio de éxito:** Código HTTP 201 y dato visible en Supabase.

**Resultados:**
- Código HTTP recibido: **201** ✅
- Tiempo de envío promedio: ~2-3 segundos (incluye handshake HTTPS)
- Dato visible en Supabase inmediatamente después del POST

---

### Prueba 5 — Dashboard en tiempo real
**Objetivo:** Verificar que el dashboard refleja los datos enviados por el sensor.

**Procedimiento:**
1. Abrir `agua-0.vercel.app/dashboard` en el navegador
2. Variar el nivel de agua físicamente
3. Esperar hasta 30 segundos (intervalo de actualización)
4. Verificar que el valor de humedad del Sector 0 cambia

**Criterio de éxito:** El dashboard muestra el nuevo valor en menos de 30 segundos.

**Resultados:**
- Tiempo hasta visualización: ~15-30 segundos ✅
- La barra de humedad refleja el nivel real del sensor
- La cinta de alertas aparece cuando el nivel supera 80%

---

## 4. Falla encontrada y solución

**Falla:** El ESP8266 reiniciaba (Watchdog reset) al intentar hacer el POST HTTP dentro del callback de ESP-NOW.

**Causa:** El callback de ESP-NOW se ejecuta en un contexto de interrupción; hacer operaciones de red dentro de él consume demasiado stack y provoca el reset.

**Solución:** Se implementó una bandera `datosPendientes = true` en el callback, y el envío HTTP se realiza en el `loop()` principal donde hay suficiente stack disponible. Adicionalmente se desinicializa ESP-NOW antes del POST y se reinicializa después para liberar memoria RAM en el ESP8266.

---

## 5. Validación contra el problema original

En el Avance 1 se identificó que **no existe información en tiempo real sobre zonas con acumulación de agua en la vía pública de Santiago**, lo que impide tomar decisiones preventivas ante lluvias intensas.

Los resultados de las pruebas demuestran que:

- El sensor detecta 3 niveles distintos de agua con valores ADC reproducibles
- Los datos se transmiten a la nube en menos de 3 segundos por medición
- El dashboard muestra alertas automáticas cuando el nivel supera el 80%
- El historial permite identificar sectores y horarios con mayor riesgo

**Conclusión:** El sistema Agua-0 detecta el fenómeno real de acumulación de agua y genera información útil para que operadores municipales identifiquen zonas críticas y tomen decisiones de intervención preventiva.

