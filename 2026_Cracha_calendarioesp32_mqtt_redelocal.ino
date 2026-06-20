// ESP32 MQTT Calendar Puzzle - Com Correção de Espelhamento
// Requer: PubSubClient, FastLED

#include <WiFi.h>
#include <PubSubClient.h>
#include <FastLED.h>

#define kMatrixWidth  16
#define kMatrixHeight 16
#define LED_PIN 13
#define NUM_LEDS (kMatrixWidth * kMatrixHeight)

// ========== CONFIGURAÇÃO FÍSICA DO PAINEL (CORREÇÃO DE ESPELHAMENTO) ==========
// Se o desenho estiver invertido da esquerda para a direita, mude para true
#define MAP_MIRROR_HORIZONTAL true 
// Se o desenho estiver de ponta cabeça, mude para true
#define MAP_MIRROR_VERTICAL   false
// ==============================================================================

CRGB leds[NUM_LEDS];

WiFiClient espClient;
PubSubClient mqtt(espClient);

// ========== CONFIGURAÇÃO DE REDE ==========
const char* ssid = "moto g(7) 4767";
const char* password = "arduino351";

const char* mqtt_server = "broker.mqtt-dashboard.com";
const char* mqtt_topic = "puzzle/control/meu_painel_123"; 
// ==========================================

struct Piece {
  int8_t x; int8_t y;
  uint8_t shape[4][4];
  CRGB color;
};

Piece pieces[8];
uint8_t selectedPiece = 0;
uint8_t currentMonth = 0;
uint8_t currentDay = 0;
unsigned long lastDrawTime = 0;

// ========== FUNÇÕES DE MATRIZ MODIFICADA PARA ESPELHAMENTO ==========

uint16_t getIndex(uint16_t x, uint16_t y) {
  // Variáveis para armazenar as coordenadas físicas finais após o mapeamento
  uint16_t physicalX = x;
  uint16_t physicalY = y;

  // 1. Aplica espelhamento horizontal lógico (Inverte X)
  if (MAP_MIRROR_HORIZONTAL) {
    physicalX = (kMatrixWidth - 1) - x;
  }

  // 2. Aplica espelhamento vertical lógico (Inverte Y)
  if (MAP_MIRROR_VERTICAL) {
    physicalY = (kMatrixHeight - 1) - y;
  }

  // 3. Calcula o índice baseado na fiação tipo SERPENTINA (Zig-Zag)
  // Assumindo que a fiação física começa na linha 0 (seja topo ou fundo após o mapeamento)
  if (physicalY % 2 == 0) {
    // Linha par: Esquerda -> Direita
    return (physicalY * kMatrixWidth) + physicalX;
  } else {
    // Linha ímpar: Direita -> Esquerda (Serpentina)
    return (physicalY * kMatrixWidth) + (kMatrixWidth - 1) - physicalX;
  }
}

// ====================================================================

// [As funções abaixo permanecem iguais ao código anterior, pois usam o getIndex atualizado]

void alignPiece(uint8_t idx) {
  int minX = 4, minY = 4;
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (pieces[idx].shape[r][c] == 1) {
        if (c < minX) minX = c;
        if (r < minY) minY = r;
      }
    }
  }
  uint8_t temp[4][4] = {0};
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      if (pieces[idx].shape[r][c] == 1) temp[r - minY][c - minX] = 1;
    }
  }
  memcpy(pieces[idx].shape, temp, sizeof(temp));
}

void rotatePiece(uint8_t idx) {
  uint8_t temp[4][4] = {0};
  for (int r=0; r<4; r++) for (int c=0; c<4; c++) temp[c][3 - r] = pieces[idx].shape[r][c];
  memcpy(pieces[idx].shape, temp, sizeof(temp));
  alignPiece(idx);
}

void flipHPiece(uint8_t idx) {
  uint8_t temp[4][4] = {0};
  for (int r=0; r<4; r++) for (int c=0; c<4; c++) temp[r][3 - c] = pieces[idx].shape[r][c];
  memcpy(pieces[idx].shape, temp, sizeof(temp));
  alignPiece(idx);
}

void flipVPiece(uint8_t idx) {
  uint8_t temp[4][4] = {0};
  for (int r=0; r<4; r++) for (int c=0; c<4; c++) temp[3 - r][c] = pieces[idx].shape[r][c];
  memcpy(pieces[idx].shape, temp, sizeof(temp));
  alignPiece(idx);
}

void initPieces() {
  for(int i=0; i<8; i++) memset(pieces[i].shape, 0, sizeof(pieces[i].shape));

  pieces[0].shape[0][0]=1; pieces[0].shape[1][0]=1; pieces[0].shape[1][1]=1; pieces[0].shape[1][2]=1; pieces[0].shape[1][3]=1;
  pieces[0].color = CRGB(255, 0, 0); pieces[0].x = 0; pieces[0].y = 0;

  pieces[1].shape[0][0]=1; pieces[1].shape[0][1]=1; pieces[1].shape[0][2]=1; pieces[1].shape[1][0]=1; pieces[1].shape[1][1]=1;
  pieces[1].color = CRGB(0, 0, 255); pieces[1].x = 5; pieces[1].y = 0;

  pieces[2].shape[0][0]=1; pieces[2].shape[1][0]=1; pieces[2].shape[2][0]=1; pieces[2].shape[2][1]=1; pieces[2].shape[2][2]=1;
  pieces[2].color = CRGB(0, 255, 0); pieces[2].x = 10; pieces[2].y = 0;

  pieces[3].shape[0][0]=1; pieces[3].shape[0][2]=1; pieces[3].shape[1][0]=1; pieces[3].shape[1][1]=1; pieces[3].shape[1][2]=1;
  pieces[3].color = CRGB(255, 255, 0); pieces[3].x = 0; pieces[3].y = 3;

  pieces[4].shape[0][0]=1; pieces[4].shape[0][1]=1; pieces[4].shape[0][2]=1; pieces[4].shape[1][0]=1; pieces[4].shape[1][1]=1; pieces[4].shape[1][2]=1;
  pieces[4].color = CRGB(128, 0, 128); pieces[4].x = 5; pieces[4].y = 3;

  pieces[5].shape[0][0]=1; pieces[5].shape[0][1]=1; pieces[5].shape[1][1]=1; pieces[5].shape[1][2]=1; pieces[5].shape[1][3]=1;
  pieces[5].color = CRGB(255, 165, 0); pieces[5].x = 10; pieces[5].y = 4;

  pieces[6].shape[0][0]=1; pieces[6].shape[0][1]=1; pieces[6].shape[1][1]=1; pieces[6].shape[2][1]=1; pieces[6].shape[2][2]=1;
  pieces[6].color = CRGB(0, 255, 255); pieces[6].x = 12; pieces[6].y = 8;

  pieces[7].shape[0][2]=1; pieces[7].shape[1][0]=1; pieces[7].shape[1][1]=1; pieces[7].shape[1][2]=1; pieces[7].shape[1][3]=1;
  pieces[7].color = CRGB(255, 0, 255); pieces[7].x = 11; pieces[7].y = 12;
}

void drawGame() {
  FastLED.clear(); 
  
  CRGB cWhite = CRGB(255, 255, 255);
  CRGB cGray  = CRGB(30, 30, 30);

  // Tabuleiro e Bloqueios
  for (int x = 0; x <= 7; x++) leds[getIndex(x, 8)] = cWhite;
  for (int y = 8; y <= 15; y++) leds[getIndex(7, y)] = cWhite;
  leds[getIndex(6, 9)] = cWhite; leds[getIndex(6, 10)] = cWhite;
  leds[getIndex(3, 15)] = cWhite; leds[getIndex(4, 15)] = cWhite;
  leds[getIndex(5, 15)] = cWhite; leds[getIndex(6, 15)] = cWhite;

  leds[getIndex(currentMonth % 6, 9 + (currentMonth / 6))] = cGray;
  leds[getIndex(currentDay % 7, 11 + (currentDay / 7))] = cGray;

  // Peças
  bool blinkState = (millis() / 250) % 2 == 0;
  for (int i = 0; i < 8; i++) {
    if (i == selectedPiece && !blinkState) continue;
    Piece p = pieces[i];
    for (int r = 0; r < 4; r++) {
      for (int c = 0; c < 4; c++) {
        if (p.shape[r][c] == 1) {
          int drawX = p.x + c; int drawY = p.y + r;
          if (drawX >= 0 && drawX < kMatrixWidth && drawY >= 0 && drawY < kMatrixHeight) {
            leds[getIndex(drawX, drawY)] = p.color;
          }
        }
      }
    }
  }
  FastLED.show();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.println("Comando: " + msg);

  if (msg == "up") pieces[selectedPiece].y--;
  else if (msg == "down") pieces[selectedPiece].y++;
  else if (msg == "left") pieces[selectedPiece].x--;
  else if (msg == "right") pieces[selectedPiece].x++;
  else if (msg == "rotate") rotatePiece(selectedPiece);
  else if (msg == "fliph") flipHPiece(selectedPiece);
  else if (msg == "flipv") flipVPiece(selectedPiece);
  else if (msg.startsWith("piece:")) {
    selectedPiece = msg.substring(6).toInt();
  }
  else if (msg.startsWith("date:")) {
    int firstColon = msg.indexOf(':');
    int secondColon = msg.indexOf(':', firstColon + 1);
    currentMonth = msg.substring(firstColon + 1, secondColon).toInt();
    currentDay = msg.substring(secondColon + 1).toInt();
  }
}

void reconnect() {
  while (!mqtt.connected()) {
    Serial.print("Conectando ao MQTT...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (mqtt.connect(clientId.c_str())) {
      Serial.println("conectado!");
      mqtt.subscribe(mqtt_topic);
    } else {
      Serial.print("falhou, rc=");
      Serial.print(mqtt.state());
      Serial.println(" tentando novamente em 5s");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // Setup FastLED - Pino 13, WS2812B, Ordem de cor GRB
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(40);
  FastLED.clear();
  FastLED.show();
  
  initPieces();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi Conectado!");

  mqtt.setServer(mqtt_server, 1883);
  mqtt.setCallback(mqttCallback);
}

void loop() {
  if (!mqtt.connected()) reconnect();
  mqtt.loop();

  if (millis() - lastDrawTime > 50) {
    drawGame();
    lastDrawTime = millis();
  }
}