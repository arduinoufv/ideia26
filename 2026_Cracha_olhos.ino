#include <FastLED.h>

// ─── Configurações do Painel de LEDs ──────────────────────
#define CONTROL_PIN 13
#define WIDTH 16
#define HEIGHT 16
#define NUM_LEDS (WIDTH * HEIGHT)
#define BRIGHTNESS 60

// ─── Configurações do Touch ───────────────────────────────
#define TOUCH_T5 T5 // Modo: Dois Olhos Longos (Contorno, Íris premium - PERFEITO)
#define TOUCH_T6 T6 // Modo: Dois Olhos Redondos (Novo: Com contorno e 1 pixel de íris)
#define TOUCH_T7 T7 // Modo: Um Olho Só (Mais rápido e com movimentos diversos)
#define TOUCH_THRESH 40

CRGB leds[NUM_LEDS];

// ─── Variáveis de Controle de Animação (Sem piscadas) ─────
uint8_t modoAtual = 0;     // 0 = T7 (Um olho), 1 = T6 (Redondos), 2 = T5 (Longos)
float pupilaX = 0.0;       // Posições atuais interpoladas
float pupilaY = 0.0;
float destinoX = 0.0;      // Alvos de coordenadas do olhar
float destinoY = 0.0;

unsigned long ultimoTempoMudanca = 0;

// ─── Mapeamento Serpentina com Inversão Física ────────────
uint16_t XY(uint8_t x, uint8_t y) {
  if (y % 2 == 0) return y * WIDTH + x;
  else return y * WIDTH + (WIDTH - 1 - x);
}

// Desenha na tela aplicando a orientação correta do seu painel
void plot(int8_t x, int8_t y, CRGB cor) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
  uint8_t xEspelhado = (WIDTH - 1) - x; 
  leds[XY(xEspelhado, y)] = cor;
}

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<WS2812B, CONTROL_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
}

void loop() {
  verificaTouch();
  atualizaMovimentos();

  FastLED.clear();
  
  // Renderiza o modo selecionado
  if (modoAtual == 0) {
    desenhaCiclopeEsperto(pupilaX, pupilaY);
  } else if (modoAtual == 1) {
    desenhaOlhosRedondosContorno(pupilaX, pupilaY);
  } else if (modoAtual == 2) {
    desenhaOlhosLongosContorno(pupilaX, pupilaY);
  }

  FastLED.show();
  delay(20); // ~50 FPS estável para movimentos orgânicos
}

// ══════════════════════════════════════════════════════════
// LÓGICA DE MOVIMENTAÇÃO DOS OLHOS
// ══════════════════════════════════════════════════════════
void atualizaMovimentos() {
  unsigned long agora = millis();

  if (modoAtual == 0) {
    // ─── MODO T7: UM OLHO SÓ (Mais rápido e imprevisível) ───
    pupilaX += (destinoX - pupilaX) * 0.08; // Velocidade aumentada
    pupilaY += (destinoY - pupilaY) * 0.08;

    // Muda o foco mais rápido (1.6s) e ganha maior amplitude horizontal/vertical
    if (agora - ultimoTempoMudanca > 1600) {
      destinoX = random(-3, 4); // Olha bem para as quinas e extremidades
      destinoY = random(-2, 3);
      ultimoTempoMudanca = agora;
    }
  } 
  else if (modoAtual == 1) {
    // ─── MODO T6: DOIS OLHOS REDONDOS (X e Y fluidos) ──────
    pupilaX += (destinoX - pupilaX) * 0.10;
    pupilaY += (destinoY - pupilaY) * 0.10;

    // Sorteia direções completas (cima, baixo, lados) a cada 2 segundos
    if (agora - ultimoTempoMudanca > 2000) {
      destinoX = random(-1, 2); // Movimento ideal para o espaço interno de 3x3
      destinoY = random(-1, 2);
      ultimoTempoMudanca = agora;
    }
  } 
  else if (modoAtual == 2) {
    // ─── MODO T5: DOIS OLHOS LONGOS (Varredura Ampla) ──────
    pupilaX += (destinoX - pupilaX) * 0.09;
    pupilaY += (destinoY - pupilaY) * 0.09;

    if (agora - ultimoTempoMudanca > 2200) {
      destinoX = random(-2, 3);
      destinoY = random(-3, 4); 
      ultimoTempoMudanca = agora;
    }
  }
}

// ─── DETECÇÃO DOS PINOS TOUCH ─────────────────────────────
void verificaTouch() {
  static unsigned long ultimoToque = 0;
  if (millis() - ultimoToque < 400) return; // Debounce

  if (touchRead(TOUCH_T7) < TOUCH_THRESH) {
    modoAtual = 0;
    destinoX = 0; destinoY = 0;
    ultimoTempoMudanca = millis();
    ultimoToque = millis();
  } 
  else if (touchRead(TOUCH_T6) < TOUCH_THRESH) {
    modoAtual = 1;
    destinoX = 0; destinoY = 0;
    ultimoTempoMudanca = millis();
    ultimoToque = millis();
  } 
  else if (touchRead(TOUCH_T5) < TOUCH_THRESH) {
    modoAtual = 2;
    destinoX = 0; destinoY = 0;
    ultimoTempoMudanca = millis();
    ultimoToque = millis();
  }
}

// ══════════════════════════════════════════════════════════
// RENDERIZADORES GRAFICOS (PIXEL ART CORRIGIDO)
// ══════════════════════════════════════════════════════════

// [MODO T7] Um olho centralizado, fundo branco cheio, movimentos ágeis
void desenhaCiclopeEsperto(float pX, float pY) {
  for(int y = 4; y <= 11; y++) {
    int sX = 2, eX = 13;
    if (y == 4 || y == 11) { sX = 5; eX = 10; }
    else if (y == 5 || y == 10) { sX = 3; eX = 12; }
    
    for(int x = sX; x <= eX; x++) plot(x, y, CRGB::White);
  }

  int cx = 7 + round(pX);
  int cy = 7 + round(pY);

  for(int dy = -2; dy <= 2; dy++) {
    for(int dx = -2; dx <= 2; dx++) {
      if (abs(dx) <= 1 && abs(dy) <= 1) {
        plot(cx + dx, cy + dy, CRGB::Black); 
      } else if (abs(dx) + abs(dy) <= 3) {
        plot(cx + dx, cy + dy, CRGB::DeepSkyBlue); 
      }
    }
  }
  plot(cx + 1, cy - 1, CRGB::White); 
}

// [MODO T6] DOIS OLHOS REDONDOS - Contorno branco, dentro preto, íris de 1 pixel azul
void desenhaOlhosRedondosContorno(float pX, float pY) {
  CRGB corContorno = CRGB::White;

  // Desenha os dois contornos redondos (Formato circular de tamanho 5x5 pixels)
  // Olho Esquerdo (Centro em X=4, Y=7) e Olho Direito (Centro em X=11, Y=7)
  
  // Tampas Horizontais (Superior e Inferior)
  for (int x = 3; x <= 5; x++)  { plot(x, 4, corContorno); plot(x, 10, corContorno); }
  for (int x = 10; x <= 12; x++) { plot(x, 4, corContorno); plot(x, 10, corContorno); }

  // Paredes Laterais
  for (int y = 5; y <= 9; y++) {
    plot(2, y, corContorno); plot(6, y, corContorno);  // Olho Esquerdo
    plot(9, y, corContorno); plot(13, y, corContorno); // Olho Direito
  }

  // Posição calculada para a íris de pixel único dentro do espaço 3x3 interno
  int cxL = constrain(4 + round(pX), 3, 5);
  int cyL = constrain(7 + round(pY), 5, 9);
  
  int cxR = constrain(11 + round(pX), 10, 12);
  int cyR = constrain(7 + round(pY), 5, 9);

  // Íris: Apenas 1 pixel azul vibrante para cada olho
  plot(cxL, cyL, CRGB::DodgerBlue);
  plot(cxR, cyR, CRGB::DodgerBlue);
}

// [MODO T5] DOIS OLHOS LONGOS - Apenas contorno branco, dentro preto, íris premium degradê
void desenhaOlhosLongosContorno(float pX, float pY) {
  CRGB corContorno = CRGB::White;

  for(int y = 2; y <= 13; y++) {
    if (y == 2 || y == 13) {
      plot(3, y, corContorno); plot(4, y, corContorno);
      plot(11, y, corContorno); plot(12, y, corContorno);
    } 
    else if (y == 3 || y == 12) {
      plot(2, y, corContorno); plot(5, y, corContorno);
      plot(10, y, corContorno); plot(13, y, corContorno);
    } 
    else {
      plot(1, y, corContorno); plot(6, y, corContorno);
      plot(9, y, corContorno); plot(14, y, corContorno);
    }
  }

  int cxL = constrain(3 + round(pX), 2, 4);
  int cyL = constrain(7 + round(pY), 4, 10);
  
  int cxR = constrain(11 + round(pX), 10, 12);
  int cyR = constrain(7 + round(pY), 4, 10);

  // Íris Esquerda (Tons de azul com brilho em cima)
  plot(cxL,     cyL,     CRGB::Blue);       plot(cxL + 1, cyL,     CRGB::Cyan);
  plot(cxL,     cyL + 1, CRGB::DarkBlue);   plot(cxL + 1, cyL + 1, CRGB::Blue);
  plot(cxL,     cyL + 2, CRGB::Blue);       plot(cxL + 1, cyL + 2, CRGB::Cyan);
  plot(cxL + 1, cyL,     CRGB::White);      

  // Íris Direita
  plot(cxR,     cyR,     CRGB::Blue);       plot(cxR + 1, cyR,     CRGB::Cyan);
  plot(cxR,     cyR + 1, CRGB::DarkBlue);   plot(cxR + 1, cyR + 1, CRGB::Blue);
  plot(cxR,     cyR + 2, CRGB::Blue);       plot(cxR + 1, cyR + 2, CRGB::Cyan);
  plot(cxR + 1, cyR,     CRGB::White);      
}