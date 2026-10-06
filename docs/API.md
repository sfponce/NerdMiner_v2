# 📖 Documentación de APIs: NerdMiner & Public-Pool

Este documento detalla la arquitectura de comunicación y las APIs disponibles para monitorear y gestionar los dispositivos **NerdMiner v2 (ESP32)**, tanto a nivel de la red local (HTTP REST API en el minero) como en la nube (Public-Pool API).

---

## 📑 Tabla de Contenidos
1. [NerdMiner Local REST API (Puerto 80)](#1-nerdminer-local-rest-api-puerto-80)
   - [Panel Web (`GET /`)](#get-)
   - [Obtener Configuración (`GET /api/config`)](#get-apiconfig)
   - [Actualizar Configuración (`POST /api/config`)](#post-apiconfig)
   - [Obtener Estado del Minero (`GET /api/status`)](#get-apistatus)
   - [Reiniciar Minero (`POST /api/restart`)](#post-apirestart)
2. [Public-Pool API (Puerto 40557)](#2-public-pool-api-puerto-40557)
   - [Endpoint del Cliente (`GET /api/client/:wallet`)](#get-apiclientwallet)
   - [Estructura del Payload](#estructura-del-payload)
   - [Cómo Interpreta Public-Pool los Datos](#c%C3%B3mo-interpreta-public-pool-los-datos)
3. [Script de Monitoreo Rápido](#3-script-de-monitoreo-r%C3%A1pido)

---

## 1. NerdMiner Local REST API (Puerto 80)

Cada dispositivo NerdMiner expone un servidor HTTP ligero en el puerto `80` accesible desde tu red local. Soporta **CORS** activado por defecto para facilitar dashboards e integraciones externas.

> **Direcciones IP de tus mineros:**
> - **Miner 1:** `http://192.168.1.75` (Worker: `master`)
> - **Miner 2:** `http://192.168.0.71` (Worker: `worker01`)

---

### `GET /`
- **Descripción:** Página HTML informativa de bienvenida. Muestra la wallet configurada, el pool actual y enlaces directos a los endpoints REST.
- **Ejemplo cURL:**
  ```bash
  curl -s http://192.168.1.75/
  ```

---

### `GET /api/config`
- **Descripción:** Devuelve la configuración actual almacenada en la memoria flash NVS del ESP32 en formato JSON.
- **Respuesta (`200 OK`):**
  ```json
  {
    "wallet": "bc1pw28ulnema2vv3p9wr6tsxk27lk3upk6kz8xdy2zthc5e5e33meas9ml3uh.master",
    "pool_url": "public-pool.io",
    "pool_port": 21496,
    "pool_password": "x",
    "timezone": -3,
    "save_stats": false,
    "invert_colors": true,
    "brightness": 250
  }
  ```
- **Campos:**
  - `wallet` (*string*): Dirección Bitcoin y nombre opcional del worker (`<address>.<worker>`).
  - `pool_url` (*string*): Hostname o IP del servidor Stratum.
  - `pool_port` (*integer*): Puerto del servidor Stratum (ej. `21496` para solo mining en public-pool).
  - `pool_password` (*string*): Contraseña de Stratum (habitualmente `"x"`).
  - `timezone` (*integer*): Desplazamiento horario respecto a UTC (-12 a +12).
  - `save_stats` (*boolean*): Guardado persistente de estadísticas de minado en NVS.
  - `invert_colors` (*boolean*): Inversión cromática del panel TFT (CYD).
  - `brightness` (*integer*): Ciclo de trabajo PWM de la retroiluminación LCD (0 - 255).

---

### `POST /api/config`
- **Descripción:** Modifica y persiste dinámicamente los parámetros de configuración en la flash sin necesidad de activar el Access Point WiFi ni usar la pantalla táctil. Admite tanto payloads `application/json` como formularios `application/x-www-form-urlencoded`.
- **Por defecto:** Guarda los cambios en la NVS y reinicia el dispositivo automáticamente para aplicar los nuevos parámetros de minado.
- **Payload JSON de ejemplo:**
  ```json
  {
    "wallet": "bc1pw28ulnema2vv3p9wr6tsxk27lk3upk6kz8xdy2zthc5e5e33meas9ml3uh.worker01",
    "pool_url": "public-pool.io",
    "pool_port": 21496,
    "pool_password": "x",
    "timezone": -3,
    "brightness": 250,
    "restart": true
  }
  ```
- **Ejemplo con cURL (JSON):**
  ```bash
  curl -X POST http://192.168.1.75/api/config \
       -H "Content-Type: application/json" \
       -d '{"wallet":"bc1pw28ulnema2vv3p9wr6tsxk27lk3upk6kz8xdy2zthc5e5e33meas9ml3uh.worker01"}'
  ```
- **Ejemplo con cURL (Form urlencoded):**
  ```bash
  curl -X POST http://192.168.1.75/api/config \
       -d "wallet=bc1pw28ulnema2vv3p9wr6tsxk27lk3upk6kz8xdy2zthc5e5e33meas9ml3uh.worker01"
  ```
- **Respuesta (`200 OK`):**
  ```json
  {
    "status": "ok",
    "message": "Configuration saved to flash. Rebooting miner now..."
  }
  ```

---

### `GET /api/status`
- **Descripción:** Devuelve el estado operativo en tiempo real, consumo de memoria y uptime del dispositivo.
- **Respuesta (`200 OK`):**
  ```json
  {
    "status": "mining",
    "wallet": "bc1pw28ulnema2vv3p9wr6tsxk27lk3upk6kz8xdy2zthc5e5e33meas9ml3uh.master",
    "pool": "public-pool.io",
    "free_heap": 128940,
    "uptime_ms": 5234870,
    "ip": "192.168.1.75"
  }
  ```
- **Campos:**
  - `status` (*string*): `"mining"` si el worker está ejecutando hashes o `"connecting"` si está negociando Stratum.
  - `wallet` (*string*): Wallet actualmente en uso.
  - `pool` (*string*): Pool conectado.
  - `free_heap` (*integer*): Memoria RAM libre en bytes en el ESP32.
  - `uptime_ms` (*integer*): Milisegundos transcurridos desde el último reinicio.
  - `ip` (*string*): Dirección IP asignada por DHCP.

---

### `POST /api/restart`
- **Descripción:** Fuerza un reinicio limpio del ESP32 por software (`ESP.restart()`).
- **Ejemplo cURL:**
  ```bash
  curl -X POST http://192.168.1.75/api/restart
  ```
- **Respuesta (`200 OK`):**
  ```json
  {
    "status": "ok",
    "message": "Rebooting ESP32..."
  }
  ```

---

## 2. Public-Pool API (Puerto 40557)

`public-pool.io` ofrece una API REST pública sobre HTTPS en el puerto `40557`. Es la misma API consumida por el frontend web oficial (`https://web.public-pool.io/#/app/<wallet>`).

---

### `GET /api/client/:wallet`
- **URL Base:** `https://public-pool.io:40557`
- **Ruta:** `/api/client/<TU_DIRECCION_BITCOIN>`
- **Ejemplo cURL:**
  ```bash
  curl -s -k "https://public-pool.io:40557/api/client/bc1pw28ulnema2vv3p9wr6tsxk27lk3upk6kz8xdy2zthc5e5e33meas9ml3uh"
  ```

---

### Estructura del Payload

```json
{
  "bestDifficulty": "4.4826134157616275",
  "workersCount": 0,
  "accounting": {
    "totalAcceptedShares": 0,
    "totalCreditedDifficulty": 0,
    "acceptedSharesLast10Minutes": 0,
    "creditedDifficultyLast10Minutes": 0,
    "acceptedSharesLastHour": 0,
    "creditedDifficultyLastHour": 0,
    "acceptedSharesLastDay": 0,
    "creditedDifficultyLastDay": 0,
    "hashRateLast10Minutes": 0,
    "hashRateLastHour": 0,
    "bestSubmissionDifficulty": 4.4826134157616275,
    "bestSubmissionDifficultyAt": null,
    "workSinceLastBlock": 0,
    "currentRoundAcceptedShares": 0,
    "currentRoundNetworkDifficulty": 0,
    "networkDifficultyPercent": 0,
    "blockCandidateCount": 0,
    "latestShareAt": null,
    "protocolBreakdown": []
  },
  "expectedPayout": null,
  "workers": []
}
```

### Explicación de Campos Clave:

| Campo | Tipo | Significado |
|---|---|---|
| `bestDifficulty` / `bestSubmissionDifficulty` | `float/string` | La dificultad más alta registrada históricamente para esta wallet en el pool. |
| `workersCount` | `int` | Número de workers activos en los **últimos 10 minutos**. |
| `acceptedSharesLast10Minutes` | `int` | Shares válidos con dificultad $\ge 1.0$ enviados en la última ventana de 10 min. |
| `hashRateLast10Minutes` | `float` | Hashrate estimado en tiempo real calculado por la fórmula: $\text{Hashrate} = \frac{\text{Shares} \times 2^{32}}{\text{Tiempo (seg)}}$. |
| `workers` | `array` | Lista de sub-workers conectados (`master`, `worker01`, etc.) y su hashrate individual. |

---

### Cómo Interpreta Public-Pool los Datos

1. **Dificultad Base de Stratum:**  
   `public-pool.io` impone una dificultad mínima de **1.0** para registrar shares.
2. **Cálculo del Hashrate en la Web:**  
   La web **no mide cuántos hashes hace físicamente el ESP32 por segundo**; infiere el hashrate basándose en la frecuencia con la que recibe shares $\ge 1.0$.
3. **Tiempo Esperado para Mostrar Actividad:**  
   - Con un hashrate de **~735 KH/s**, se tarda en promedio **~1.6 horas** en encontrar un hash que cumpla con dificultad $\ge 1.0$ ($4.29 \times 10^9$ hashes).
   - Por esta razón, cuando un worker recién arranca o no ha enviado un share en los últimos 10 minutos, `workersCount` figurará en `0` hasta que entre el siguiente share al pool.

---

## 3. Script de Monitoreo Rápido

Para consultar el estado de ambos mineros y el pool desde la terminal:

```bash
#!/bin/bash
WALLET="bc1pw28ulnema2vv3p9wr6tsxk27lk3upk6kz8xdy2zthc5e5e33meas9ml3uh"

echo "=== MINER 1 (192.168.1.75) ==="
curl -s --connect-timeout 2 http://192.168.1.75/api/status | jq . || echo "Miner 1 Offline"

echo -e "\n=== MINER 2 (192.168.0.71) ==="
curl -s --connect-timeout 2 http://192.168.0.71/api/status | jq . || echo "Miner 2 Offline"

echo -e "\n=== PUBLIC-POOL ESTADO GLOBAL ==="
curl -s -k --connect-timeout 5 "https://public-pool.io:40557/api/client/${WALLET}" | jq '{bestDifficulty, workersCount, accounting: {acceptedSharesLast10Minutes: .accounting.acceptedSharesLast10Minutes, hashRateLast10Minutes: .accounting.hashRateLast10Minutes}}'
```
