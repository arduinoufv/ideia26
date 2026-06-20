#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>

// WiFi and MQTT settings
const char* ssid = "moto g(7) 4767";
const char* password = "arduino351";
const char* mqtt_server = "broker.mqtt-dashboard.com";
#define MQTTPORT 1883

// LED strip settings
#define PIN_LED        D3
#define MAXLED         300
#define NPIXELS        MAXLED

// Race settings
#define LOOP_MAX       8
#define MAX_CARS       15

// MQTT topics
#define TOPIC_PREFIX   "trackXX/"
#define TOPIC_RESET   "trackXX/reset"

#define INI_RAMP 30
#define MED_RAMP 40
#define END_RAMP 50
#define HIGH_RAMP 16
bool ENABLE_RAMP=0;
bool VIEW_RAMP=0;

byte  gravity_map[MAXLED];  

void set_ramp(byte H,byte a,byte b,byte c)
{for(int i=0;i<(b-a);i++){gravity_map[a+i]=127-i*((float)H/(b-a));};
 gravity_map[b]=127; 
 for(int i=0;i<(c-b);i++){gravity_map[b+i+1]=127+H-i*((float)H/(c-b));};
}

void set_loop(byte H,byte a,byte b,byte c)
{for(int i=0;i<(b-a);i++){gravity_map[a+i]=127-i*((float)H/(b-a));};
 gravity_map[b]=255; 
 for(int i=0;i<(c-b);i++){gravity_map[b+i+1]=127+H-i*((float)H/(c-b));};
}

// Car struct to hold car-specific data
struct Car {
    float speed;
    float dist;
    byte loop;
    uint32_t color;
    char topic[25];
};

// Array of cars
Car cars[MAX_CARS];

// Other variables
Adafruit_NeoPixel track = Adafruit_NeoPixel(NPIXELS, PIN_LED, NEO_GRB + NEO_KHZ800);
WiFiClient espClient;
PubSubClient client(espClient);

float ACEL = 0.2;
float kf = 0.015;
byte leader = 0;
byte draworder = 0;
int tdelay = 5;
float kg=0.003; //gravity constant

unsigned long timestamp=0;

void setup() {
    Serial.begin(115200);
    track.begin();
    setup_wifi();
    client.setServer(mqtt_server, MQTTPORT);
    client.setCallback(callback);

    // Initialize cars
    initializeCars();
    set_ramp(HIGH_RAMP,INI_RAMP,MED_RAMP,END_RAMP); 

    start_race();
}

const char* carNames[] = {
    "vermelho",     // 0
    "verde",        // 1
    "azul",         // 2
    "amarelo",      // 3
    "branco",       // 4
    "rosa",         // 5
    "ciano",        // 6
    "laranja",      // 7
    "marrom",       // 8
    "bege",         // 9
    "verdeagua",    //10
    "roxo",         //11
    "verdemusgo",   //12
    "azulmarinho",  //13
    "verdeclaro"    //14
};
uint32_t carColors[] = {
    track.Color(255, 0, 0),     // Vermelho
    track.Color(0, 255, 0),     // Verde
    track.Color(0, 0, 255),     // Azul
    track.Color(255, 255, 0),   // Amarelo
    track.Color(255, 255, 255), // Branco
    track.Color(255, 0, 255),   // Rosa
    track.Color(0, 255, 255),   // Ciano
    track.Color(255, 128, 0),   // Laranja
    track.Color(62, 29, 22),    // Marrom
    track.Color(219, 173, 42),  // Bege
    track.Color(0, 255, 128),   // VerdeÁgua
    track.Color(128, 0, 255),   // Roxo
    track.Color(0, 95, 49),     // VerdeMusgo
    track.Color(0, 0, 79),      // AzulMarinho
    track.Color(128, 255, 128)  // VerdeClaro
};



void initializeCars() {
    
    for (int i = 0; i < MAX_CARS; i++) {
        cars[i].speed = 0;
        cars[i].dist = 0;
        cars[i].loop = 0;
        cars[i].color = carColors[i];
        sprintf(cars[i].topic, "%s%scar", TOPIC_PREFIX, carNames[i]);
        client.subscribe(cars[i].topic);
        //Serial.print(cars[i].topic);
    }
    client.subscribe(TOPIC_RESET);
}

void callback(char* topic, byte* payload, unsigned int length) {
//Serial.print(topic);
    for (int i = 0; i < MAX_CARS; i++) {
        if (strcmp(topic, cars[i].topic) == 0) {
            cars[i].speed += ACEL;
            break;
        }
        if (strcmp(topic, TOPIC_RESET) == 0) {
            resetRace();
            break;
        }
    }
}

void setup_wifi() {
    // WiFi setup code (unchanged)

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

}

void reconnect() {
    // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
/*****
 * Add two pushbutton subscribe. You could add more controls....
 */
for (int i = 0; i < MAX_CARS; i++) {
        sprintf(cars[i].topic, "%s%scar", TOPIC_PREFIX, carNames[i]);
        client.subscribe(cars[i].topic);
    }
    client.subscribe(TOPIC_RESET);

/*******************************************************/      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void start_race() {
    // Race start animation code (unchanged)
    for(int i=0;i<NPIXELS;i++){track.setPixelColor(i, track.Color(0,0,0));};
                  track.show();
                  delay(2000);
                  track.setPixelColor(12, track.Color(0,255,0));
                  track.setPixelColor(11, track.Color(0,255,0));
                  track.show();
                  track.setPixelColor(12, track.Color(0,0,0));
                  track.setPixelColor(11, track.Color(0,0,0));
                  track.setPixelColor(10, track.Color(255,255,0));
                  track.setPixelColor(9, track.Color(255,255,0));
                  track.show();
                  track.setPixelColor(9, track.Color(0,0,0));
                  track.setPixelColor(10, track.Color(0,0,0));
                  track.setPixelColor(8, track.Color(255,0,0));
                  track.setPixelColor(7, track.Color(255,0,0));
                  track.show();
                  timestamp=0;              

}

void draw_car(int carIndex) {
    for (int i = 0; i <= cars[carIndex].loop; i++) {
        int r,g,b;
        if (((cars[carIndex].color >> 16) & 0xFF)!=0)
           r = ((cars[carIndex].color >> 16) & 0xFF) -i*2;
        else 
           r= 0;
        if (((cars[carIndex].color >> 8) & 0xFF)!=0)
           g = ((cars[carIndex].color >> 8) & 0xFF)-i*2;
        else 
           g= 0;
        if (((cars[carIndex].color) & 0xFF)!=0)
           b = ((cars[carIndex].color ) & 0xFF)-i*2;
        else 
           b= 0;
        uint32_t color = track.Color(
            r,
            g,
            b
        );
        track.setPixelColor(((word)cars[carIndex].dist % NPIXELS) + i, color);
    }
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    for (int i = 0; i < NPIXELS; i++) {
        track.setPixelColor(i, track.Color(0,0,0));
    }

    for (int i = 0; i < MAX_CARS; i++) {
      if ((int)cars[i].dist % NPIXELS > 15 && (int)cars[i].dist % NPIXELS < 25  ) 
        cars[i].speed-=cars[i].speed*15*kf;
      if ((int)cars[i].dist % NPIXELS > 25 && (int)cars[i].dist % NPIXELS < 30  ) 
        cars[i].speed+=cars[i].speed*5*kf;
      if ((int)cars[i].dist % NPIXELS > 180 && (int)cars[i].dist % NPIXELS < 190  ) 
        cars[i].speed-=cars[i].speed*15*kf;
      if ((int)cars[i].dist % NPIXELS > 190 && (int)cars[i].dist % NPIXELS < 200  ) 
        cars[i].speed+=cars[i].speed*5*kf;

        cars[i].speed -= cars[i].speed * kf;
        cars[i].dist += cars[i].speed;

        if (cars[i].dist > NPIXELS * cars[i].loop) {
            cars[i].loop++;
        }

        if (cars[i].loop > LOOP_MAX) {
            // End race and reset
            Serial.println(carNames[i]);
            for (int j = 0; j < NPIXELS/4; j++) {
                track.setPixelColor(j, cars[i].color);
            }
            track.show();
            delay(4000);
            resetRace();
            start_race();
            return;
        }
    }

    // Determine leader
    leader = 0;
    for (int i = 1; i < MAX_CARS; i++) {
        if (cars[i].dist > cars[leader].dist) {
            leader = i;
        }
    }

    // Draw cars
    for (int i = 0; i < MAX_CARS; i++) {
        draw_car(i);
    }

    track.show();
    delay(tdelay);
}

void resetRace() {
    for (int i = 0; i < MAX_CARS; i++) {
        cars[i].speed = 0;
        cars[i].dist = 0;
        cars[i].loop = 0;
    }
}