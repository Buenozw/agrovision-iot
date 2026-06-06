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
 *   GET  /               Dashboard HTML
 *   GET  /api/status     JSON completo dos sensores
 *   GET  /api/ndvi       JSON do índice NDVI
 *   GET  /api/alerta     JSON do alerta atual
 *   POST /api/alerta/reset  Reseta o alerta
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

// ── CONFIGURAÇÕES ────────────────────────────────────────────
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

// ── OBJETOS ──────────────────────────────────────────────────
DHT               dht(PINO_DHT, TIPO_DHT);
LiquidCrystal_I2C lcd(0x27, 16, 2);
WebServer         server(80);

// ── DADOS DOS SENSORES ───────────────────────────────────────
float   gTemperatura    = 0.0f;
float   gUmidadeAr      = 0.0f;
float   gUmidadeSolo    = 0.0f;
float   gNdvi           = 0.0f;
String  gStatus         = "Iniciando";
String  gNivelAlerta    = "Nenhum";
bool    gAlertaAtivo    = false;

// ── CONTROLE LCD ─────────────────────────────────────────────
unsigned long tLcd  = 0;
int           tela  = 0;

// ── FUNÇÕES AUXILIARES ───────────────────────────────────────
float calcularSolo(int adc) {
  return (adc / 4095.0f) * 100.0f;
}

float calcularNdvi(float solo, float temp) {
  float fator = (temp > 35.0f) ? 0.70f : (temp > 30.0f) ? 0.85f : 1.0f;
  float n = (solo / 100.0f) * fator;
  return constrain(n, 0.0f, 1.0f);
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

  if      (!gAlertaAtivo)           gNivelAlerta = "Nenhum";
  else if (gNdvi < NDVI_CRITICO)    gNivelAlerta = "Critico";
  else                              gNivelAlerta = "Alto";
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
      digitalWrite(PINO_LED_VERDE,    LOW);
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
      lcd.setCursor(0, 1); lcd.print(gAlertaAtivo ? "!! ALERTA !!    " : "Campo OK        ");
      break;
  }
  tela = (tela + 1) % 3;
}

// ── CORS ─────────────────────────────────────────────────────
void cors() {
  server.sendHeader("Access-Control-Allow-Origin",  "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ── HANDLERS HTTP ─────────────────────────────────────────────
void handleRoot() {
  cors();
  server.send_P(200, "text/html", DASHBOARD_HTML);
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
  if      (gNdvi >= NDVI_ATENCAO) { desc="Vegetacao saudavel";              rec="Nenhuma acao necessaria"; }
  else if (gNdvi >= NDVI_CRITICO) { desc="Vegetacao em estresse moderado";  rec="Monitorar proximo ciclo satelital"; }
  else                             { desc="Possivel infestacao de praga";    rec="Acionar missao de drone imediatamente"; }

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
  if (gNdvi         < NDVI_CRITICO)    c.add("ndvi_baixo");
  if (gTemperatura  > TEMPERATURA_MAX) c.add("temperatura_alta");
  if (gUmidadeSolo  < UMIDADE_MIN)     c.add("solo_seco");
  if (c.size() == 0)                   c.add("nenhuma");
  String r; serializeJson(doc, r);
  server.send(200, "application/json", r);
}

void handleReset() {
  cors();
  gAlertaAtivo = false;
  gNivelAlerta = "Nenhum";
  gStatus      = "Saudavel";
  server.send(200, "application/json", "{\"sucesso\":true}");
}

void handleOptions() { cors(); server.send(204); }

void handleNotFound() {
  cors();
  server.send(404, "application/json", "{\"erro\":\"Rota nao encontrada\"}");
}

// ── SETUP ─────────────────────────────────────────────────────
void setup() {
  // Serial — primeira coisa a fazer
  Serial.begin(115200);
  delay(500);  // aguarda estabilizar
  Serial.println();
  Serial.println("========================================");
  Serial.println("  AgroVision IoT — ESP32 iniciando...");
  Serial.println("========================================");

  // LEDs
  pinMode(PINO_LED_VERDE,    OUTPUT);
  pinMode(PINO_LED_VERMELHO, OUTPUT);
  digitalWrite(PINO_LED_VERDE,    HIGH);  // acende verde no boot
  digitalWrite(PINO_LED_VERMELHO, LOW);
  Serial.println("[OK] LEDs configurados");

  // DHT
  dht.begin();
  Serial.println("[OK] Sensor DHT22 iniciado");

  // LCD
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("  AgroVision IoT ");
  lcd.setCursor(0, 1); lcd.print("  Iniciando...  ");
  Serial.println("[OK] LCD 16x2 I2C iniciado");

  // WiFi
  Serial.print("[..] Conectando WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long tInicio = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - tInicio > 15000) {
      Serial.println("\n[ERR] Timeout WiFi — rodando sem rede");
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("WiFi: TIMEOUT");
      lcd.setCursor(0, 1); lcd.print("Sem dashboard");
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("[OK] WiFi conectado!");
    Serial.print("[OK] Endereco IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();
    Serial.println("--------- ACESSE NO NAVEGADOR ---------");
    Serial.print("  http://");
    Serial.println(WiFi.localIP());
    Serial.println("---------------------------------------");

    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("WiFi OK!");
    lcd.setCursor(0, 1); lcd.print(WiFi.localIP());
    delay(2500);
  }

  // Rotas WebServer
  server.on("/",                  HTTP_GET,     handleRoot);
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

  Serial.println("[OK] WebServer iniciado na porta 80");
  Serial.println();
  Serial.println("ENDPOINTS DISPONÍVEIS:");
  Serial.println("  GET  /               -> Dashboard HTML");
  Serial.println("  GET  /api/status     -> Sensores JSON");
  Serial.println("  GET  /api/ndvi       -> NDVI JSON");
  Serial.println("  GET  /api/alerta     -> Alerta JSON");
  Serial.println("  POST /api/alerta/reset -> Reseta alerta");
  Serial.println();
  Serial.println("======= LEITURAS A CADA 2s =======");

  // Primeira leitura dos sensores
  lerSensores();
}

// ── LOOP ──────────────────────────────────────────────────────
void loop() {
  server.handleClient();

  // Lê sensores a cada 2 segundos
  static unsigned long tSensor = 0;
  if (millis() - tSensor > 2000) {
    lerSensores();
    tSensor = millis();

    // Log serial formatado
    Serial.println("----------------------------------");
    Serial.print("Temperatura : "); Serial.print(gTemperatura, 1); Serial.println(" C");
    Serial.print("Umidade Ar  : "); Serial.print(gUmidadeAr,   1); Serial.println(" %");
    Serial.print("Umidade Solo: "); Serial.print(gUmidadeSolo,  0); Serial.println(" %");
    Serial.print("NDVI Est.   : "); Serial.println(gNdvi, 3);
    Serial.print("Status      : "); Serial.println(gStatus);
    Serial.print("Alerta      : "); Serial.println(gAlertaAtivo ? "*** ATIVO ***" : "Nenhum");
    Serial.print("Uptime      : "); Serial.print(millis() / 1000); Serial.println(" s");
  }

  atualizarLeds();
  atualizarLcd();
}
