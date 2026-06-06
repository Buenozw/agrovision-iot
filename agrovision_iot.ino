/**
 * AgroVision IoT — ESP32
 * Global Solution 2026/1 — FIAP
 *
 * ENTRADAS : DHT22 (GPIO 4) · Potenciômetro solo (GPIO 34)
 * SAÍDAS   : LED Verde (GPIO 26) · LED Vermelho (GPIO 27)
 * INTERFACE: LCD 16x2 I2C (SDA=21 / SCL=22)
 * REDE     : Wi-Fi + WebServer HTTP porta 80
 *
 * ENDPOINTS:
 *   GET  /api/status        JSON completo dos sensores
 *   GET  /api/ndvi          JSON do índice NDVI
 *   GET  /api/alerta        JSON do alerta atual
 *   POST /api/alerta/reset  Reseta o alerta
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <Wire.h>

// ── CONFIGURAÇÕES ─────────────────────────────────────────────
#define WIFI_SSID         "Wokwi-GUEST"
#define WIFI_PASSWORD     ""

#define PINO_DHT          4
#define PINO_SOLO         34
#define PINO_LED_VERDE    26
#define PINO_LED_VERMELHO 27
#define TIPO_DHT          DHT22

const float NDVI_CRITICO    = 0.35f;
const float NDVI_ATENCAO    = 0.50f;
const float TEMPERATURA_MAX = 38.0f;
const float UMIDADE_MIN     = 30.0f;

// ── OBJETOS ───────────────────────────────────────────────────
DHT               dht(PINO_DHT, TIPO_DHT);
LiquidCrystal_I2C lcd(0x27, 16, 2);
WebServer         server(80);

// ── DADOS DOS SENSORES ────────────────────────────────────────
float  gTemperatura  = 0.0f;
float  gUmidadeAr    = 0.0f;
float  gUmidadeSolo  = 0.0f;
float  gNdvi         = 0.0f;
String gStatus       = "Iniciando";
String gNivelAlerta  = "Nenhum";
bool   gAlertaAtivo  = false;

// ── CONTROLE LCD ──────────────────────────────────────────────
unsigned long tLcd = 0;
int           tela = 0;

// ── HELPERS SERIAL ────────────────────────────────────────────
void serialLinha() {
  Serial.println(F("================================================"));
}

void serialBanner() {
  serialLinha();
  Serial.println(F("   _    ग्रो Vision IoT  —  ESP32"));
  Serial.println(F("   AgroVision IoT  |  FIAP 2026/1"));
  serialLinha();
}

void serialStatus() {
  Serial.println(F("\n>>> LEITURA DOS SENSORES <<<"));
  Serial.print(F("  Temperatura : ")); Serial.print(gTemperatura, 1); Serial.println(F(" C"));
  Serial.print(F("  Umidade Ar  : ")); Serial.print(gUmidadeAr,   1); Serial.println(F(" %"));
  Serial.print(F("  Umidade Solo: ")); Serial.print(gUmidadeSolo,  0); Serial.println(F(" %"));
  Serial.print(F("  NDVI Est.   : ")); Serial.println(gNdvi, 3);
  Serial.print(F("  Status      : ")); Serial.println(gStatus);
  Serial.print(F("  Alerta      : "));
  if (gAlertaAtivo) {
    Serial.print(F("*** ATIVO — nivel: ")); Serial.print(gNivelAlerta); Serial.println(F(" ***"));
  } else {
    Serial.println(F("Nenhum"));
  }
  Serial.print(F("  Uptime      : ")); Serial.print(millis() / 1000); Serial.println(F(" s"));
  Serial.println();
}

// ── FUNÇÕES AUXILIARES ────────────────────────────────────────
float calcularSolo(int adc) {
  return (adc / 4095.0f) * 100.0f;
}

float calcularNdvi(float solo, float temp) {
  float fator = (temp > 35.0f) ? 0.70f : (temp > 30.0f) ? 0.85f : 1.0f;
  return constrain((solo / 100.0f) * fator, 0.0f, 1.0f);
}

String calcularStatus(float ndvi) {
  if (ndvi >= NDVI_ATENCAO) return "Saudavel";
  if (ndvi >= NDVI_CRITICO) return "Observacao";
  return "Critico";
}

void lerSensores() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) gTemperatura = t;
  if (!isnan(h)) gUmidadeAr   = h;

  gUmidadeSolo = calcularSolo(analogRead(PINO_SOLO));
  gNdvi        = calcularNdvi(gUmidadeSolo, gTemperatura);
  gStatus      = calcularStatus(gNdvi);
  gAlertaAtivo = (gStatus == "Critico") || (gTemperatura > TEMPERATURA_MAX);

  if      (!gAlertaAtivo)         gNivelAlerta = "Nenhum";
  else if (gNdvi < NDVI_CRITICO)  gNivelAlerta = "Critico";
  else                            gNivelAlerta = "Alto";
}

void atualizarLeds() {
  if (gStatus == "Saudavel") {
    digitalWrite(PINO_LED_VERDE,    HIGH);
    digitalWrite(PINO_LED_VERMELHO, LOW);
  } else if (gStatus == "Observacao") {
    static unsigned long tp = 0;
    static bool sp = false;
    if (millis() - tp > 800) {
      sp = !sp;
      digitalWrite(PINO_LED_VERMELHO, sp ? HIGH : LOW);
      digitalWrite(PINO_LED_VERDE, LOW);
      tp = millis();
    }
  } else {
    digitalWrite(PINO_LED_VERMELHO, HIGH);
    digitalWrite(PINO_LED_VERDE,    LOW);
  }
}

void atualizarLcd() {
  if (millis() - tLcd < 3000) return;
  tLcd = millis();
  lcd.clear();
  switch (tela) {
    case 0:
      lcd.setCursor(0, 0); lcd.print("Temp: "); lcd.print(gTemperatura, 1); lcd.print((char)223); lcd.print("C");
      lcd.setCursor(0, 1); lcd.print("UmAr: "); lcd.print(gUmidadeAr, 1); lcd.print("%");
      break;
    case 1:
      lcd.setCursor(0, 0); lcd.print("NDVI: "); lcd.print(gNdvi, 2);
      lcd.setCursor(0, 1); lcd.print(gStatus);
      break;
    case 2:
      lcd.setCursor(0, 0); lcd.print("Solo: "); lcd.print(gUmidadeSolo, 0); lcd.print("%");
      lcd.setCursor(0, 1); lcd.print(gAlertaAtivo ? "!! ALERTA !!" : "Campo OK");
      break;
  }
  tela = (tela + 1) % 3;
}
// ── CORS ──────────────────────────────────────────────────────
void cors() {
  server.sendHeader("Access-Control-Allow-Origin",  "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleStatus() {
  cors();
  StaticJsonDocument<256> doc;
  doc["dispositivo"]      = "AgroVision-ESP32";
  doc["talhao"]           = "Talhao-A1";
  doc["temperatura_c"]    = gTemperatura;
  doc["umidade_ar_pct"]   = gUmidadeAr;
  doc["umidade_solo_pct"] = gUmidadeSolo;
  doc["ndvi_estimado"]    = gNdvi;
  doc["status_campo"]     = gStatus;
  doc["alerta_ativo"]     = gAlertaAtivo;
  doc["nivel_alerta"]     = gNivelAlerta;
  doc["uptime_ms"]        = millis();
  String r; serializeJson(doc, r);
  server.send(200, "application/json", r);
}

void handleNdvi() {
  cors();
  String desc, rec;
  if      (gNdvi >= NDVI_ATENCAO) { desc = "Vegetacao saudavel";             rec = "Nenhuma acao necessaria"; }
  else if (gNdvi >= NDVI_CRITICO) { desc = "Vegetacao em estresse moderado"; rec = "Monitorar proximo ciclo satelital"; }
  else                             { desc = "Possivel infestacao de praga";   rec = "Acionar missao de drone imediatamente"; }

  StaticJsonDocument<256> doc;
  doc["talhao"]         = "Talhao-A1";
  doc["ndvi_estimado"]  = gNdvi;
  doc["classificacao"]  = gStatus;
  doc["descricao"]      = desc;
  doc["recomendacao"]   = rec;
  doc["limiar_critico"] = NDVI_CRITICO;
  doc["limiar_atencao"] = NDVI_ATENCAO;
  String r; serializeJson(doc, r);
  server.send(200, "application/json", r);
}

void handleAlerta() {
  cors();
  StaticJsonDocument<300> doc;
  doc["alerta_ativo"]      = gAlertaAtivo;
  doc["nivel"]             = gNivelAlerta;
  doc["ndvi_atual"]        = gNdvi;
  doc["temperatura_atual"] = gTemperatura;
  doc["timestamp_ms"]      = millis();
  JsonArray c = doc.createNestedArray("causas");
  if (gNdvi        < NDVI_CRITICO)    c.add("ndvi_baixo");
  if (gTemperatura > TEMPERATURA_MAX) c.add("temperatura_alta");
  if (gUmidadeSolo < UMIDADE_MIN)     c.add("solo_seco");
  if (c.size() == 0)                  c.add("nenhuma");
  String r; serializeJson(doc, r);
  server.send(200, "application/json", r);
}

void handleReset() {
  cors();
  gAlertaAtivo = false;
  gNivelAlerta = "Nenhum";
  gStatus      = "Saudavel";
  Serial.println(F("[WEB] Alerta resetado via POST /api/alerta/reset"));
  server.send(200, "application/json", "{\"sucesso\":true}");
}

void handleOptions() { cors(); server.send(204); }


void handleRoot() {
  cors();

  String html = "";
  html += "<!DOCTYPE html><html lang='pt-BR'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>AgroVision IoT</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;background:#0f172a;color:#e5e7eb;padding:24px;}";
  html += ".card{max-width:700px;margin:auto;background:#111827;padding:24px;border-radius:12px;}";
  html += "h1{color:#22c55e;} code{background:#020617;padding:3px 6px;border-radius:5px;}";
  html += "a{color:#38bdf8;}";
  html += "</style></head><body><div class='card'>";
  html += "<h1>AgroVision IoT - ESP32</h1>";
  html += "<p>Servidor HTTP ativo.</p>";
  html += "<p><strong>Endpoints:</strong></p>";
  html += "<ul>";
  html += "<li><a href='/api/status'>GET /api/status</a></li>";
  html += "<li><a href='/api/ndvi'>GET /api/ndvi</a></li>";
  html += "<li><a href='/api/alerta'>GET /api/alerta</a></li>";
  html += "<li><code>POST /api/alerta/reset</code></li>";
  html += "</ul>";
  html += "</div></body></html>";

  server.send(200, "text/html", html);
}

void handleNotFound() {
  cors();
  Serial.print(F("[WEB] 404 -> ")); Serial.println(server.uri());
  server.send(404, "application/json", "{\"erro\":\"Rota nao encontrada\"}");
}

// ── SETUP ─────────────────────────────────────────────────────
void setup() {
  // ① Serial deve ser a PRIMEIRA instrução — delay garante buffer pronto no Wokwi
  Serial.begin(115200);
  delay(1000);

  serialBanner();
  Serial.println(F("[BOOT] Iniciando sistema AgroVision..."));
  Serial.println();

  // LEDs
  pinMode(PINO_LED_VERDE,    OUTPUT);
  pinMode(PINO_LED_VERMELHO, OUTPUT);
  digitalWrite(PINO_LED_VERDE,    HIGH);
  digitalWrite(PINO_LED_VERMELHO, LOW);
  Serial.println(F("[OK] LEDs configurados (GPIO 26=verde, GPIO 27=vermelho)"));

  // DHT22
  dht.begin();
  Serial.println(F("[OK] Sensor DHT22 iniciado (GPIO 4)"));

  // LCD
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(" AgroVision IoT");
  lcd.setCursor(0, 1); lcd.print("  Iniciando...");
  Serial.println(F("[OK] LCD 16x2 I2C iniciado (SDA=21, SCL=22)"));
  Serial.println();

  // WiFi
  Serial.print(F("[WiFi] Conectando a: "));
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long tInicio = millis();
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tentativas++;
    if (tentativas % 2 == 0) {        // imprime a cada 1 segundo
      Serial.print(F("[WiFi] Aguardando... ("));
      Serial.print(tentativas / 2);
      Serial.println(F("s)"));
    }
    if (millis() - tInicio > 15000) {
      Serial.println(F("\n[ERRO] Timeout WiFi — sistema rodando SEM rede"));
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("WiFi: TIMEOUT");
      lcd.setCursor(0, 1); lcd.print("Sem dashboard");
      return;
    }
  }

  // WiFi OK
  Serial.println();
  Serial.println(F("[OK] WiFi conectado!"));

  String ip = WiFi.localIP().toString();
  Serial.println();
  serialLinha();
  Serial.print(F("  IP DO ESP32: "));
  Serial.println(ip);
  Serial.println(F("  Cole esse IP no dashboard.html"));
  Serial.println(F("  ou acesse no navegador:"));
  Serial.print(F("  http://"));
  Serial.println(ip);
  serialLinha();
  Serial.println();

  // Mostra IP no LCD por 4 segundos
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("WiFi OK!");
  lcd.setCursor(0, 1); lcd.print(ip);
  delay(4000);

  // Rotas WebServer
  server.on("/",                  HTTP_GET,     handleRoot);
  server.on("/",                  HTTP_OPTIONS, handleOptions);
  server.on("/api/status",        HTTP_GET,     handleStatus);
  server.on("/api/ndvi",          HTTP_GET,     handleNdvi);
  server.on("/api/alerta",        HTTP_GET,     handleAlerta);
  server.on("/api/alerta/reset",  HTTP_POST,    handleReset);
  server.on("/api/status",        HTTP_OPTIONS, handleOptions);
  server.on("/api/ndvi",          HTTP_OPTIONS, handleOptions);
  server.on("/api/alerta",        HTTP_OPTIONS, handleOptions);
  server.on("/api/alerta/reset",  HTTP_OPTIONS, handleOptions);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println(F("[OK] WebServer iniciado na porta 80"));
  Serial.println();
  Serial.println(F("ENDPOINTS DISPONIVEIS:"));
  Serial.println(F("  GET  /api/status        -> Sensores JSON"));
  Serial.println(F("  GET  /api/ndvi          -> NDVI JSON"));
  Serial.println(F("  GET  /api/alerta        -> Alerta JSON"));
  Serial.println(F("  POST /api/alerta/reset  -> Reseta alerta"));
  Serial.println();
  Serial.println(F("======= LEITURAS A CADA 2s — INICIANDO ======="));
  Serial.println();

  lerSensores();
}

// ── LOOP ──────────────────────────────────────────────────────
void loop() {
  server.handleClient();

  static unsigned long tSensor = 0;
  if (millis() - tSensor > 2000) {
    lerSensores();
    tSensor = millis();
    serialStatus();         // imprime leitura formatada no Serial Monitor
  }

  atualizarLeds();
  atualizarLcd();
}
