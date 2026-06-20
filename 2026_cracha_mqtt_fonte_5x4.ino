#include <Arduino.h>
#include <FastLED.h>

typedef void (*DrawPixelFunc)(int16_t x, int16_t y, CRGB color);

class Matrix4x5Text {
private:
    DrawPixelFunc drawPixel;
    
    struct LineState {
        String text;
        CRGB color;
        bool active = false;
        bool scrolling = false;
        int scrollPos = 0;
        unsigned long lastUpdate = 0;
    };
    
    // Agora temos apenas 3 linhas disponíveis (0, 1 e 2)
    LineState lines[3];
    
    // Mapeamento da fonte 4x5 (A-Z, 0-9)
    // Cada caractere possui 5 linhas, e usamos 4 bits por linha (0b0000 a 0b1111)
    const uint8_t font[36][5] = {
        {0b0110, 0b1001, 0b1111, 0b1001, 0b1001}, // A (0)
        {0b1110, 0b1001, 0b1110, 0b1001, 0b1110}, // B (1)
        {0b0110, 0b1001, 0b1000, 0b1001, 0b0110}, // C (2)
        {0b1110, 0b1001, 0b1001, 0b1001, 0b1110}, // D (3)
        {0b1111, 0b1000, 0b1110, 0b1000, 0b1111}, // E (4)
        {0b1111, 0b1000, 0b1110, 0b1000, 0b1000}, // F (5)
        {0b0110, 0b1000, 0b1011, 0b1001, 0b0111}, // G (6)
        {0b1001, 0b1001, 0b1111, 0b1001, 0b1001}, // H (7)
        {0b0110, 0b0110, 0b0110, 0b0110, 0b0110}, // I (8)
        {0b0001, 0b0001, 0b0001, 0b1001, 0b0110}, // J (9)
        {0b1001, 0b1010, 0b1100, 0b1010, 0b1001}, // K (10)
        {0b1000, 0b1000, 0b1000, 0b1000, 0b1111}, // L (11)
        {0b1001, 0b1111, 0b1001, 0b1001, 0b1001}, // M (12)
        {0b1001, 0b1101, 0b1011, 0b1001, 0b1001}, // N (13)
        {0b0110, 0b1001, 0b1001, 0b1001, 0b0110}, // O (14)
        {0b1110, 0b1001, 0b1110, 0b1000, 0b1000}, // P (15)
        {0b0110, 0b1001, 0b1001, 0b0110, 0b0001}, // Q (16)
        {0b1110, 0b1001, 0b1110, 0b1010, 0b1001}, // R (17)
        {0b0111, 0b1000, 0b0110, 0b0001, 0b1110}, // S (18)
        {0b1111, 0b0110, 0b0110, 0b0110, 0b0110}, // T (19)
        {0b1001, 0b1001, 0b1001, 0b1001, 0b0110}, // U (20)
        {0b1001, 0b1001, 0b1001, 0b0110, 0b0110}, // V (21)
        {0b1001, 0b1001, 0b1001, 0b1111, 0b1001}, // W (22)
        {0b1001, 0b1001, 0b0110, 0b1001, 0b1001}, // X (23)
        {0b1001, 0b1001, 0b0110, 0b0110, 0b0110}, // Y (24)
        {0b1111, 0b0001, 0b0110, 0b1000, 0b1111}, // Z (25)
        {0b0110, 0b1001, 0b1001, 0b1001, 0b0110}, // 0 (26)
        {0b0010, 0b0110, 0b0010, 0b0010, 0b0111}, // 1 (27)
        {0b0110, 0b1001, 0b0010, 0b0100, 0b1111}, // 2 (28)
        {0b1110, 0b0001, 0b0110, 0b0001, 0b1110}, // 3 (29)
        {0b1001, 0b1001, 0b1111, 0b0001, 0b0001}, // 4 (30)
        {0b1111, 0b1000, 0b1110, 0b0001, 0b1110}, // 5 (31)
        {0b0110, 0b1000, 0b1110, 0b1001, 0b0110}, // 6 (32)
        {0b1111, 0b0001, 0b0010, 0b0100, 0b1000}, // 7 (33)
        {0b0110, 0b1001, 0b0110, 0b1001, 0b0110}, // 8 (34)
        {0b0110, 0b1001, 0b0111, 0b0001, 0b0110}  // 9 (35)
    };

    int getCharIndex(char c) {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a';
        if (c >= '0' && c <= '9') return c - '0' + 26;
        return -1; 
    }

    void drawCharacter(int x, int y, char c, CRGB color) {
        int idx = getCharIndex(c);
        if (idx < 0) return; 
        
        for (int row = 0; row < 5; row++) {
            uint8_t rowBits = font[idx][row];
            for (int col = 0; col < 4; col++) {
                // Lê os 4 bits da esquerda para a direita
                if (rowBits & (1 << (3 - col))) {
                    if (drawPixel) {
                        drawPixel(x + col, y + row, color);
                    }
                }
            }
        }
    }

public:
    Matrix4x5Text() : drawPixel(nullptr) {}

    void begin(DrawPixelFunc func) {
        drawPixel = func;
    }

    // Só temos 3 linhas disponíveis (0, 1 e 2)
    void printline(uint8_t line, CRGB color, String message) {
        if (line > 2) return;
        lines[line].text = message;
        lines[line].color = color;
        lines[line].active = true;
        lines[line].scrollPos = 0;
        lines[line].lastUpdate = millis();
        
        // Ativa o scroll se a mensagem tiver mais de 3 caracteres
        lines[line].scrolling = (message.length() > 3);
    }

    void clearLine(uint8_t line) {
        if (line <= 2) lines[line].active = false;
    }

    void draw() {
        for (int i = 0; i < 3; i++) {
            if (!lines[i].active) continue;

            // Linhas começam em Y = 0, 5, e 10
            int yOffset = i * 5; 
            int msgLen = lines[i].text.length();

            if (!lines[i].scrolling) {
                // Desenha texto estático (até 3 caracteres)
                for (int c = 0; c < msgLen && c < 3; c++) {
                    drawCharacter(c * 5, yOffset, lines[i].text[c], lines[i].color);
                }
            } else {
                // Desenha texto em rolagem
                int totalWidth = (msgLen + 3) * 5; 
                
                if (millis() - lines[i].lastUpdate > 150) {
                    lines[i].scrollPos++;
                    if (lines[i].scrollPos >= totalWidth) {
                        lines[i].scrollPos = 0;
                    }
                    lines[i].lastUpdate = millis();
                }
                
                for (int c = 0; c < msgLen; c++) {
                    int xPos = (c * 5) - lines[i].scrollPos;
                    // Limites da tela para não gastar processamento invisível
                    if (xPos > -5 && xPos < 16) {
                        drawCharacter(xPos, yOffset, lines[i].text[c], lines[i].color);
                    }
                }
            }
        }
    }
};


#include <WiFi.h>
#include <PubSubClient.h>

// ─── Configurações de Rede e MQTT ────────────────────────
const char* ssid     = "moto g(7) 4767";
const char* password = "arduino351";
const char* mqtt_broker = "broker.mqtt-dashboard.com";
const int   mqtt_port   = 1883;
const char* mqtt_topic  = "cracha/linha";

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastReconnectAttempt = 0;

// ─── Configurações do Painel de LEDs ──────────────────────
#define LED_PIN 13
#define MATRIX_W 16
#define MATRIX_H 16
#define NUM_LEDS (MATRIX_W * MATRIX_H)
#define BRIGHTNESS 60
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// ─── Configurações do Touch ───────────────────────────────
#define TOUCH_T5 T5 
#define TOUCH_T6 T6 
#define TOUCH_T7 T7 
#define TOUCH_THRESH 40

CRGB leds[NUM_LEDS];
Matrix4x5Text telaTextos; 

// ─── Mapeamento serpentina ────────────────────────────────
uint16_t XY(uint8_t x, uint8_t y) {
  if (y % 2 == 0) return y * MATRIX_W + x;
  else return y * MATRIX_W + (MATRIX_W - 1 - x);
}

// ─── Correção de Espelhamento Horizontal ──────────────────
void desenhaPixel(int16_t x, int16_t y, CRGB color) {
  if (x < 0 || x >= MATRIX_W || y < 0 || y >= MATRIX_H) return;
  int16_t xEspelhado = (MATRIX_W - 1) - x;
  leds[XY(xEspelhado, y)] = color;
}

// ─── Conversor de Cores (Texto -> CRGB) ───────────────────
CRGB converteCor(String corStr) {
  corStr.toLowerCase();
  corStr.trim();
  
  if (corStr == "red"    || corStr == "vermelho") return CRGB::Red;
  if (corStr == "blue"   || corStr == "azul")     return CRGB::Blue;
  if (corStr == "orange" || corStr == "laranja")  return CRGB::Orange;
  if (corStr == "yellow" || corStr == "amarelo")  return CRGB::Yellow;
  if (corStr == "white"  || corStr == "branco")   return CRGB::White;
  if (corStr == "green"  || corStr == "verde")    return CRGB::Green;
  if (corStr == "purple" || corStr == "roxo")     return CRGB::Purple;
  
  return CRGB::White; // Cor padrão caso não reconheça
}

// ─── Callback do MQTT (Processamento da Mensagem) ────────
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Converte o payload recebido em String do C++
  String msgCompleta = "";
  for (unsigned int i = 0; i < length; i++) {
    msgCompleta += (char)payload[i];
  }
  
  Serial.print("Mensagem recebida: ");
  Serial.println(msgCompleta);

  // Encontra as posições das vírgulas para separar os dados
  int primeiraVirgula = msgCompleta.indexOf(',');
  int segundaVirgula  = msgCompleta.indexOf(',', primeiraVirgula + 1);

  // Validação básica do formato do protocolo (precisa ter duas vírgulas)
  if (primeiraVirgula == -1 || segundaVirgula == -1) {
    Serial.println("Erro: Formato inválido! Use 'linha, cor, mensagem'");
    return;
  }

  // 1. Extrai e converte a Linha
  String linhaStr = msgCompleta.substring(0, primeiraVirgula);
  linhaStr.trim();
  int linha = linhaStr.toInt();
  if (linha < 0 || linha > 2) return; // Linha inválida

  // 2. Extrai e converte a Cor
  String corStr = msgCompleta.substring(primeiraVirgula + 1, segundaVirgula);
  CRGB corFinal = converteCor(corStr);

  // 3. Extrai o Texto e adiciona os 4 espaços no início
  String textoStr = msgCompleta.substring(segundaVirgula + 1);
  textoStr.trim();
  String textoFinal = "    " + textoStr; // Prepende os 4 espaços solicitados

  // Atualiza a linha na tela (automaticamente apaga a anterior e reinicia o scroll)
  telaTextos.printline(linha, corFinal, textoFinal);
}

// ─── Conectores de Rede (Não-bloqueantes) ─────────────────
void conectaWiFi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando em: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

bool conectaMQTT() {
  Serial.print("Tentando conexão MQTT... ");
  // Gera um ID de cliente único usando o MAC do ESP32
  String clientId = "ESP32Client-" + String(random(0xffff), HEX);
  
  if (client.connect(clientId.c_str())) {
    Serial.println("Conectado ao Broker!");
    client.subscribe(mqtt_topic);
    return true;
  }
  Serial.print("Falhou, rc=");
  Serial.println(client.state());
  return false;
}

void setup() {
  Serial.begin(115200);
  
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  
  telaTextos.begin(desenhaPixel);
  
  // Mensagem inicial padrão
  telaTextos.printline(0, CRGB::Green, "    WIFI");

  conectaWiFi();
  
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(mqttCallback);
}

void loop() {
  // Mantém os LEDs rodando e atualizando
  FastLED.clear();
  telaTextos.draw();
  FastLED.show();

  // Gerenciamento inteligente de conexão MQTT (Sem travar o loop)
  if (!client.connected()) {
    unsigned long agora = millis();
    if (agora - lastReconnectAttempt > 5000) { // Tenta reconectar a cada 5 segundos
      lastReconnectAttempt = agora;
      if (conectaMQTT()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    client.loop(); // Processa mensagens MQTT recebidas
  }

  // ─── Verificação dos Botões Touch (Mantidos como fallback) ───
  static unsigned long lastTouch = 0;
  if (millis() - lastTouch < 300) return; 

  if (touchRead(TOUCH_T7) < TOUCH_THRESH) {
    lastTouch = millis();
    telaTextos.printline(1, CRGB::Blue, "    AZUL");
  }
  else if (touchRead(TOUCH_T6) < TOUCH_THRESH) {
    lastTouch = millis();
    telaTextos.printline(2, CRGB::Red, "    RED");
  }
  else if (touchRead(TOUCH_T5) < TOUCH_THRESH) {
    lastTouch = millis();
    telaTextos.printline(0, CRGB::Green, "    1234");
  }
}