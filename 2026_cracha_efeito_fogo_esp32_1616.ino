#include <FastLED.h>

// ─── Configurações do Painel de LEDs ──────────────────────
#define CONTROL_PIN 13
#define WIDTH 16
#define HEIGHT 16
#define NUM_LEDS (WIDTH * HEIGHT)
#define BRIGHTNESS 90

// ─── Configurações do Touch ───────────────────────────────
#define TOUCH_T5 T5 // Fogo "Apagado" / Suave
#define TOUCH_T6 T6 // Azul
#define TOUCH_T7 T7 // Vermelho (Novo e intenso)
#define TOUCH_THRESH 40

CRGB leds[NUM_LEDS];
CRGBPalette16 gPal;

// ─── Definição de Paletas Customizadas ────────────────────

// NOVA PALETA: Vermelho e Laranja intensos (Sem Branco)
DEFINE_GRADIENT_PALETTE( ChamaVermelha_p ) {
  0,     0,   0,   0,     // Preto (topo frio)
  90,  255,   0,   0,     // Vermelho intenso
  180, 255,  80,   0,     // Laranja
  255, 255, 180,   0      // Amarelo no núcleo extremo (sem branco)
};

// Paleta Fogo Azul (Ajustada para ficar sem branco também)
DEFINE_GRADIENT_PALETTE( ChamaAzul_p ) {
  0,     0,   0,   0,     // Preto
  100,   0,   0, 255,     // Azul escuro
  180,   0, 120, 255,     // Azul claro
  255,   0, 200, 255      // Ciano brilhante no núcleo
};

// Paleta extra (só brasa suave)
DEFINE_GRADIENT_PALETTE( ChamaBrasa_p ) {
  0,     0,   0,   0,     
  120,  50,   0,   0,     
  255, 150,  30,   0      
};

CRGBPalette16 paletaVermelha = ChamaVermelha_p;
CRGBPalette16 paletaAzul = ChamaAzul_p;
CRGBPalette16 paletaBrasa = ChamaBrasa_p;

// ─── Mapeamento Serpentina ────────────────────────────────
uint16_t XY(uint8_t x, uint8_t y) {
  if (y % 2 == 0) return y * WIDTH + x;
  else return y * WIDTH + (WIDTH - 1 - x);
}

// ─── Configurações do Fogo ────────────────────────────────
#define FPS 30
#define FPS_DELAY 1000/FPS
#define COOLING 40    // DIMINUÍDO: De 55 para 40 para a base ficar quente por mais tempo
#define HOT 180
#define MAXHOT (HOT * HEIGHT)

void setup() {
  Serial.begin(115200);
  
  FastLED.addLeds<WS2812B, CONTROL_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  
  // Começa com a nova paleta vermelha sem branco
  gPal = paletaVermelha; 
  
  FastLED.clear();
  FastLED.show();
  delay(500); 
}

void loop() {
  random16_add_entropy(random()); 
  
  verificaTouch();
  Fireplace();

  FastLED.show();
  FastLED.delay(FPS_DELAY);
}

// ══════════════════════════════════════════════════════════
// LÓGICA DE TROCA DE COR (TOUCH)
// ══════════════════════════════════════════════════════════
void verificaTouch() {
  static unsigned long lastTouch = 0;
  
  if (millis() - lastTouch < 200) return;

  if (touchRead(TOUCH_T7) < TOUCH_THRESH) {
    gPal = paletaVermelha; // Vermelho Intenso
    lastTouch = millis();
  } 
  else if (touchRead(TOUCH_T6) < TOUCH_THRESH) {
    gPal = paletaAzul;     // Azul
    lastTouch = millis();
  } 
  else if (touchRead(TOUCH_T5) < TOUCH_THRESH) {
    gPal = paletaBrasa;    // Brasa baixa
    lastTouch = millis();
  }
}

// ══════════════════════════════════════════════════════════
// ALGORITMO DO FOGO (Otimizado para Labaredas Altas 16x16)
// ══════════════════════════════════════════════════════════
void Fireplace() {
  static unsigned int spark[WIDTH]; 
  CRGB stack[WIDTH][HEIGHT];        
 
  // 1. Gera as faíscas na base
  for(int i = 0; i < WIDTH; i++) {
    if (spark[i] < HOT) {
      int base = HOT * 2;
      spark[i] = random16(base, MAXHOT);
    }
  }
  
  // 2. Esfria as faíscas gradativamente
  for(int i = 0; i < WIDTH; i++) {
    spark[i] = qsub8(spark[i], random8(0, COOLING));
  }
  
  // 3. Constrói a pilha vertical de calor
  for(int i = 0; i < WIDTH; i++) {
    unsigned int heat = constrain(spark[i], HOT/2, MAXHOT);
    
    for(int j = HEIGHT - 1; j >= 0; j--) {
      byte index = constrain(heat, 0, HOT);
      stack[i][j] = ColorFromPalette(gPal, index);
      
      // A MÁGICA ESTÁ AQUI: Antes o drop era random8(0, HOT). 
      // Isso matava a chama em 3 ou 4 pixels. 
      // Agora o limite é 35, permitindo que o calor suba até o topo da tela!
      unsigned int drop = random8(0, 35); 
      
      if (drop > heat) heat = 0; 
      else heat -= drop;
 
      heat = constrain(heat, 0, MAXHOT);
    }
  }
  
  // 4. Mapeia a pilha virtual para a fita física
  for(int i = 0; i < WIDTH; i++) {
    for(int j = 0; j < HEIGHT; j++) {
      int16_t xEspelhado = (WIDTH - 1) - i;
      leds[XY(xEspelhado, j)] = stack[i][j];
    }
  }
}