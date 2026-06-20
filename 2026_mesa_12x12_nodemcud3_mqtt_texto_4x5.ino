#include <Arduino.h>
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

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
    
    // Mantemos as 2 linhas (0 e 1)
    LineState lines[2];
    
    // Mapeamento da fonte 4x5 (A-Z, 0-9)
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

    void printline(uint8_t line, CRGB color, String message) {
        if (line > 1) return; // Limite de 2 linhas
        lines[line].text = message;
        lines[line].color = color;
        lines[line].active = true;
        lines[line].scrollPos = 0;
        lines[line].lastUpdate = millis();
        
        lines[line].scrolling = (message.length() > 2); // Cabem ~2 chars na tela de 12px
    }

    void clearLine(uint8_t line) {
        if (line <= 1) lines[line].active = false;
    }

    void draw() {
        for (int i = 0; i < 2; i++) {
            if (!lines[i].active) continue;

            // Linha 0 em Y=0; Linha 1 em Y=6
            int yOffset = i * 6; 
            int msgLen = lines[i].text.length();

            if (!lines[i].scrolling) {
                for (int c = 0; c < msgLen && c < 2; c++) { 
                    drawCharacter(c * 5, yOffset, lines[i].text[c], lines[i].color);
                }
            } else {
                int totalWidth = (msgLen + 2) * 5; 
                
                if (millis() - lines[i].lastUpdate > 150) {
                    lines[i].scrollPos++;
                    if (lines[i].scrollPos >= totalWidth) {
                        lines[i].scrollPos = 0;
                    }
                    lines[i].lastUpdate = millis();
                }
                
                for (int c = 0; c < msgLen; c++) {
                    int xPos = (c * 5) - lines[i].scrollPos;
                    // Limites horizontais atualizados para tela de 12px
                    if (xPos > -5 && xPos < 12) {
                        drawCharacter(xPos, yOffset, lines[i].text[c], lines[i].color);
                    }
                }
            }
        }
    }
};

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
#define LED_PIN D3
#define MATRIX_W 12
#define MATRIX_H 12
#define NUM_LEDS (MATRIX_W * MATRIX_H)
#define BRIGHTNESS 60
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];
Matrix4x5Text telaTextos; 

// ─── Mapeamento Serpentina Original (Baseado nas coordenadas físicas) ────
// Esta função assume que a linha 0 física começa em X=0, a linha 1 física começa em X=11, etc.
uint16_t XY_Serpentina(uint8_t x, uint8_t y) {
  if (y % 2 == 0) return y * MATRIX_W + x;
  else return y * MATRIX_W + (MATRIX_W - 1 - x);
}

// ─── Lógica de Transformação: Espelhamento Horizontal E Vertical ───────
// Esta função recebe a coordenada LOGICA (onde o texto acha que está desenhando)
// e a converte para a coordenada FISICA, aplicando o Double Flip.
void desenhaPixelTransformado(int16_t x_logico, int16_t y_logico, CRGB color) {
  // 1. Verificação de limites lógicos
  if (x_logico < 0 || x_logico >= MATRIX_W || y_logico < 0 || y_logico >= MATRIX_H) return;

  // 2. Aplica o Espelhamento Horizontal (Inverte X)
  int16_t x_fisico = (MATRIX_W - 1) - x_logico;

  // 3. Aplica o Espelhamento Vertical (Inverte Y)
  int16_t y_fisico = (MATRIX_H - 1) - y_logico;

  // 4. Envia as coordenadas físicas finais para o mapeamento da serpentina
  leds[XY_Serpentina(x_fisico, y_fisico)] = color;
}

// ─── Conversor de Cores ──────────────────────────────────
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
  
  return CRGB::White; 
}

// ─── Callback do MQTT ────────────────────────────────────
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msgCompleta = "";
  for (unsigned int i = 0; i < length; i++) {
    msgCompleta += (char)payload[i];
  }
  
  Serial.print("Mensagem recebida: ");
  Serial.println(msgCompleta);

  int primeiraVirgula = msgCompleta.indexOf(',');
  int segundaVirgula  = msgCompleta.indexOf(',', primeiraVirgula + 1);

  if (primeiraVirgula == -1 || segundaVirgula == -1) {
    Serial.println("Erro: Formato inválido! Use 'linha, cor, mensagem'");
    return;
  }

  String linhaStr = msgCompleta.substring(0, primeiraVirgula);
  linhaStr.trim();
  int linha = linhaStr.toInt();
  if (linha < 0 || linha > 1) return; // Apenas linhas 0 e 1

  String corStr = msgCompleta.substring(primeiraVirgula + 1, segundaVirgula);
  CRGB corFinal = converteCor(corStr);

  String textoStr = msgCompleta.substring(segundaVirgula + 1);
  textoStr.trim();
  String textoFinal = "    " + textoStr;

  telaTextos.printline(linha, corFinal, textoFinal);
}

// ─── Conectores de Rede ──────────────────────────────────
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
  String clientId = "ESP8266Client-" + String(random(0xffff), HEX);
  
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
  
  // IMPORTANTE: Agora passamos a nova função de desenho transformado
  telaTextos.begin(desenhaPixelTransformado);
  
  telaTextos.printline(0, CRGB::Green, "    WIFI");

  conectaWiFi();
  
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(mqttCallback);
}

void loop() {
  FastLED.clear();
  telaTextos.draw(); // O texto desenha logicamente, a função callback aplica o flip
  FastLED.show();

  if (!client.connected()) {
    unsigned long agora = millis();
    if (agora - lastReconnectAttempt > 5000) { 
      lastReconnectAttempt = agora;
      if (conectaMQTT()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    client.loop(); 
  }
}