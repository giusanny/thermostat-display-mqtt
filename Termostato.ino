#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Encoder.h>
#include <Bounce2.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <WebServer.h>
#include <WiFi.h>

// Definizione delle dimensioni in EEPROM:
// 64 byte per SSID + 64 byte per password + 64 per topic = 256 byte totali
#define EEPROM_SIZE 256
#define ADDR_SSID 0     // Indirizzo iniziale per l'SSID (64 byte)
#define ADDR_PASS 64    // Indirizzo iniziale per la password (64 byte)
#define ADDR_TOPIC 128  // Indirizzo iniziale per il topic (64 byte)

// Valori di default (usate se l'EEPROM non è stata inizializzata)
char defaultSSID[] = "mySSID";
char defaultPass[] = "myPassword";
char defaultTopic[] = "myThermostatHA"; // che va a sostituire il value del topic "homeassistant/thermostat/{value}/mode"

// Credenziali  MQTT
const char *mqtt_server = "mosquittoServerIp";
const char *mqtt_user = "userMqtt";
const char *mqtt_password = "pwdMqtt";

// Topic MQTT
String topic_mode = "homeassistant/thermostat/{value}/mode";
String topic_set_mode = "homeassistant/thermostat/{value}/set_mode";
String topic_temp = "homeassistant/thermostat/{value}/current_temp";
String topic_setpoint = "homeassistant/thermostat/{value}/target_temp";
String topic_set_setpoint = "homeassistant/thermostat/{value}/set_target_temp";
String topic_preset_mode = "homeassistant/thermostat/{value}/preset_mode";
String topic_set_preset_mode = "homeassistant/thermostat/{value}/set_preset_mode";

// Buffer per le credenziali lette/salvate (aggiungiamo spazio per il terminatore)
char savedSSID[65];  // 64 byte + terminatore
char savedPassword[65];
char savedTopic[65];
// Flag per indicare che le nuove credenziali sono state configurate
volatile bool wifiConfigured = false;

// Istanza del WebServer sulla porta 80
WebServer server(80);

// Funzione per il reset della EEPROM: scrive 0xFF in ogni byte
void resetEEPROM() {
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0xFF);
  }
  EEPROM.commit();
  Serial.println("EEPROM resettata");
}

void handleRoot() {
  String page = "<html><head><title>Termostat Config</title>";
  page += "<script>";
  page += "function setSSID(ssid) { document.getElementById('ssid').value = ssid; }";
  page += "</script></head><body>";
  page += "<h2>Seleziona una rete WiFi</h2>";

  // Scansiona le reti WiFi disponibili
  int n = WiFi.scanNetworks();
  if (n == 0) {
    page += "<p>Nessuna rete trovata.</p>";
  } else {
    page += "<ul>";
    for (int i = 0; i < n; ++i) {
      page += "<li><a href='#' onclick='setSSID(\"" + WiFi.SSID(i) + "\")'>" + WiFi.SSID(i) + "</a></li>";
    }
    page += "</ul>";
  }

  // Form per inserire le credenziali WiFi
  page += "<form action='/save' method='POST'>";
  page += "SSID: <input type='text' id='ssid' name='ssid'><br>";
  page += "Password: <input type='password' name='password'><br>";
  page += "Topic: homeassistant/thermostat/<input type='topic' name='topic'><br>";
  page += "<input type='submit' value='Connetti'>";
  page += "</form><br>";
  // Aggiungiamo anche un pulsante per resettare l'EEPROM
  page += "<form action='/reset' method='POST'>";
  page += "<input type='submit' value='Reset EEPROM'>";
  page += "</form></body></html>";

  server.send(200, "text/html", page);
}
// Gestione del salvataggio delle nuove credenziali
void handleSave() {
  if (server.hasArg("ssid") && server.hasArg("password") && server.hasArg("topic")) {
    String newSSID = server.arg("ssid");
    String newPass = server.arg("password");
    String newTopic = server.arg("topic");

    // Copia nelle variabili globali (incluso il terminatore)
    newSSID.toCharArray(savedSSID, sizeof(savedSSID));
    newPass.toCharArray(savedPassword, sizeof(savedPassword));
    newTopic.toCharArray(savedTopic, sizeof(savedTopic));

    // Scrittura in EEPROM: 64 byte per ciascuna stringa
    for (int i = 0; i < 64; i++) {
      EEPROM.write(ADDR_SSID + i, savedSSID[i]);
    }
    for (int i = 0; i < 64; i++) {
      EEPROM.write(ADDR_PASS + i, savedPassword[i]);
    }

    for (int i = 0; i < 64; i++) {
      EEPROM.write(ADDR_TOPIC + i, savedTopic[i]);
    }
    EEPROM.commit();

    String page = "<html><body>Configurazioni salvate!</body></html>";
    server.send(200, "text/html", page);
    wifiConfigured = true;
  } else {
    server.send(400, "text/plain", "Parametro mancante");
  }
}

// Endpoint per il reset della EEPROM
void handleReset() {
  resetEEPROM();
  String page = "<html><body>EEPROM resettata! Riavviare il dispositivo.</body></html>";
  server.send(200, "text/html", page);
}

// Carica le configurazioni salvate in EEPROM oppure usa quelle di default
void loadConfig() {
  EEPROM.begin(EEPROM_SIZE);
  // Se il primo byte dell'SSID è 0xFF si assume che l'EEPROM non sia inizializzata
  if (EEPROM.read(ADDR_SSID) == 0xFF) {
    strncpy(savedSSID, defaultSSID, sizeof(savedSSID));
    strncpy(savedPassword, defaultPass, sizeof(savedPassword));
    strncpy(savedTopic, defaultTopic, sizeof(savedTopic));
  } else {
    for (int i = 0; i < 64; i++) {
      savedSSID[i] = EEPROM.read(ADDR_SSID + i);
    }
    savedSSID[64] = '\0';
    for (int i = 0; i < 64; i++) {
      savedPassword[i] = EEPROM.read(ADDR_PASS + i);
    }
    savedPassword[64] = '\0';
    for (int i = 0; i < 64; i++) {
      savedTopic[i] = EEPROM.read(ADDR_TOPIC + i);
    }
    savedTopic[64] = '\0';
  }
}

// Definizione dei colori se non già presenti
#ifndef LV_COLOR_BLUE
#define LV_COLOR_BLUE lv_color_make(0x87, 0xCE, 0xFA)  // Celeste (SkyBlue)
#endif

#ifndef LV_COLOR_ORANGE
#define LV_COLOR_ORANGE lv_color_make(0xFF, 0x85, 0x00)  // Arancione
#endif

#ifndef LV_COLOR_GRAY
#define LV_COLOR_GRAY lv_color_make(150, 150, 150)  // Grigio
#endif



WiFiClient espClient;
PubSubClient client(espClient);

// Variabili di stato
float currentTemp = 30.0;
float setPoint = 10.0;
String mode = "Mode";
String preset_mode = "Preset";

// Variabili per standby e gestione encoder
bool displayOn = true;
unsigned long lastActivityTime = 0;
int32_t last_encoder_value = 0;  // per tracciare le variazioni del rotary encoder

#define GFX_BL 8

Arduino_DataBus *bus = new Arduino_ESP32SPI(4 /* DC */, 10 /* CS */, 1 /* SCK */, 0 /* MOSI */, GFX_NOT_DEFINED /* MISO */);
Arduino_GFX *gfx = new Arduino_GC9A01(bus, GFX_NOT_DEFINED /* RST */, 1 /* rotation */, true /* IPS */);

#define ROTARY_ENCODER_A_PIN 7
#define ROTARY_ENCODER_B_PIN 6
#define ROTARY_ENCODER_BUTTON_PIN 9

Encoder myEnc(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN);
Bounce2::Button button = Bounce2::Button();

// Elementi UI
lv_obj_t *label_temp, *label_setpoint, *label_mode, *label_preset, *arcSP_widget, *arcT_widget;

/* Change to your screen resolution */
static uint32_t screenWidth = 320;
static uint32_t screenHeight = 240;
lv_disp_draw_buf_t draw_buf;
lv_color_t *disp_draw_buf1;
lv_color_t *disp_draw_buf2;
lv_disp_drv_t disp_drv;

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

  lv_disp_flush_ready(disp);
}


static void my_button_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
}


static void my_encoder_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
}

// FUNZIONE: Aggiornamento colore di sfondo del SP
void update_background_colorSP() {
  lv_color_t bgColor;
  if (setPoint <= 5) {
    bgColor = LV_COLOR_BLUE;  // Celeste
  } else if (setPoint >= 35) {
    bgColor = LV_COLOR_ORANGE;  // Arancione
  } else {
    // Calcola una sfumatura in base al setpoint
    uint8_t blend = map(setPoint, 5, 35, 0, 255);
    bgColor = lv_color_mix(LV_COLOR_ORANGE, LV_COLOR_BLUE, blend);
  }
  //lv_obj_set_style_bg_color(lv_scr_act(), bgColor, LV_PART_MAIN);
  lv_obj_set_style_arc_color(arcSP_widget, bgColor, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(arcSP_widget, bgColor, LV_PART_KNOB);
}

// FUNZIONE: Aggiornamento colore di sfondo del SP
void update_background_colorT() {
  lv_color_t bgColor;
  if (currentTemp <= 5) {
    bgColor = LV_COLOR_BLUE;  // Celeste
  } else if (currentTemp >= 35) {
    bgColor = LV_COLOR_ORANGE;  // Arancione
  } else {
    // Calcola una sfumatura in base al setpoint
    uint8_t blend = map(currentTemp, 5, 35, 0, 255);
    bgColor = lv_color_mix(LV_COLOR_ORANGE, LV_COLOR_BLUE, blend);
  }
  //lv_obj_set_style_bg_color(lv_scr_act(), bgColor, LV_PART_MAIN);
  lv_obj_set_style_arc_color(arcT_widget, bgColor, LV_PART_INDICATOR);
}

// CALLBACK MQTT: Aggiorna i valori in UI e lo sfondo (quando arriva il topic setpoint)
void mqtt_callback(char *topic, byte *payload, unsigned int length) {
  payload[length] = '\0';
  String message = String((char *)payload);

  Serial.print("MQTT: ");
  Serial.print(topic);
  Serial.print(" -> ");
  Serial.println(message);

  if (strcmp(topic, topic_mode.c_str()) == 0) {
    if (strcmp(message.c_str(), "heat") == 0)
      mode = "Caldo";
    else if (strcmp(message.c_str(), "cool") == 0)
      mode = "Freddo";
    else if (strcmp(message.c_str(), "off") == 0)
      mode = "OFF";

    lv_label_set_text(label_mode, (mode).c_str());
  } else if (strcmp(topic, topic_temp.c_str()) == 0) {
    currentTemp = message.toFloat();
    lv_label_set_text(label_temp, (String(currentTemp) + "°C").c_str());
    update_background_colorT();

    // Aggiorna il cursore della temperatura
    if (arcT_widget != NULL) {
      lv_arc_set_value(arcT_widget, (int)currentTemp);
    }

  } else if (strcmp(topic, topic_setpoint.c_str()) == 0) {
    setPoint = message.toFloat();
    lv_label_set_text(label_setpoint, (String(setPoint) + "°C").c_str());
    update_background_colorSP();
    if (arcSP_widget != NULL) {
      lv_arc_set_value(arcSP_widget, (int)setPoint);
    }
  } else if (strcmp(topic, topic_preset_mode.c_str()) == 0) {
    if (strcmp(message.c_str(), "eco") == 0)
      preset_mode = "Eco";
    if (strcmp(message.c_str(), "boost") == 0)
      preset_mode = "Boost";

    lv_label_set_text(label_preset, (preset_mode).c_str());
  }
}

// FUNZIONE: Creazione dell'interfaccia grafica
void create_ui() {

  // Pulisce lo schermo attuale
  lv_obj_clean(lv_scr_act());

  lv_obj_t *new_scr = lv_obj_create(NULL);

  lv_obj_t *container = lv_obj_create(new_scr);
  //lv_obj_t *container = lv_obj_create(lv_scr_act());
  lv_obj_set_size(container, 240, 240);
  lv_obj_center(container);
  lv_obj_align(container, LV_TEXT_ALIGN_CENTER, -40, 0);

  // Label per mostrare le temperature e altri dati
  label_temp = lv_label_create(container);
  lv_label_set_text(label_temp, "30°C");
  lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_34, 0);
  lv_obj_set_style_text_align(label_temp, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(label_temp);

  label_setpoint = lv_label_create(container);
  lv_label_set_text(label_setpoint, "10°C");
  lv_obj_set_style_text_font(label_setpoint, &lv_font_montserrat_22, 0);
  lv_obj_set_style_text_align(label_setpoint, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align_to(label_setpoint, label_temp, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);

  label_mode = lv_label_create(container);
  lv_label_set_text(label_mode, "Stagione");
  lv_obj_set_style_text_font(label_mode, &lv_font_montserrat_18, 0);
  lv_obj_align(label_mode, LV_ALIGN_CENTER, 0, -45);

  label_preset = lv_label_create(container);
  lv_label_set_text(label_preset, "OFF");
  lv_obj_set_style_text_font(label_preset, &lv_font_montserrat_14, 0);
  lv_obj_align(label_preset, LV_ALIGN_CENTER, 0, 70);

  // Creiamo l'arco della temperatura
  lv_obj_t *arcT = lv_arc_create(container);
  lv_obj_set_size(arcT, 200, 200);
  lv_arc_set_rotation(arcT, 135);
  lv_arc_set_bg_angles(arcT, 0, 270);

  // Imposta il range dell'arco da 5 a 35 (corrispondente al setPoint)
  lv_arc_set_range(arcT, 5, 35);

  // Imposta il valore iniziale (usando la temperatura iniziale)
  lv_arc_set_value(arcT, currentTemp);
  lv_obj_center(arcT);

  // Imposta il colore dell'arco attivo (valore corrente)
  lv_obj_set_style_arc_color(arcT, LV_COLOR_ORANGE, LV_PART_INDICATOR);

  // Imposta il colore dello sfondo dell'arco
  lv_obj_set_style_arc_color(arcT, LV_COLOR_GRAY, LV_PART_MAIN);
  lv_obj_set_style_opa(arcT, LV_OPA_TRANSP, LV_PART_KNOB);

  // Aumenta lo spessore dell'arco attivo
  lv_obj_set_style_arc_width(arcT, 27, LV_PART_INDICATOR);

  // Diminuisci lo spessore dello sfondo dell'arco
  lv_obj_set_style_arc_width(arcT, 0, LV_PART_MAIN);

  lv_obj_set_style_pad_all(arcT, -7, LV_PART_INDICATOR);
  arcT_widget = arcT;

  // Creiamo l'arco del setpoint
  lv_obj_t *arc = lv_arc_create(container);
  lv_obj_set_size(arc, 200, 200);
  lv_arc_set_rotation(arc, 135);
  lv_arc_set_bg_angles(arc, 0, 270);

  // Imposta il range dell'arco da 5 a 35 (corrispondente al setPoint)
  lv_arc_set_range(arc, 5, 35);
  lv_obj_set_style_arc_width(arc, 13, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(arc, 13, LV_PART_MAIN);

  // Imposta il valore iniziale (usando il setPoint iniziale)
  lv_arc_set_value(arc, setPoint);
  lv_obj_center(arc);

  // Salva il riferimento globale all'arco per poterlo aggiornare altrove
  arcSP_widget = arc;

  //lv_scr_load(lv_scr_act());
  lv_scr_load(new_scr);
}

// FUNZIONE: Connessione a MQTT con sottoscrizioni
void connect_mqtt() {
  while (!client.connected()) {
    if (client.connect("AtHome_Thermostat", mqtt_user, mqtt_password)) {
      client.subscribe(topic_mode.c_str());
      client.subscribe(topic_temp.c_str());
      client.subscribe(topic_setpoint.c_str());
      client.subscribe(topic_preset_mode.c_str());
      Serial.println("MQTT connesso e sottoscritto");
    } else {
      Serial.println("MQTT non connesso, riprovo tra 5 secondi");
      delay(5000);
    }
  }
}

void setup() {
  //resetEEPROM();
  Serial.begin(115200);
  Serial.println("Setup started");

  // Configura il backlight: LOW = acceso, HIGH = spento
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, LOW);
  displayOn = true;
  lastActivityTime = millis();

  //initializing gfx
  gfx->begin(80000000);
  Serial.println("GFX initialized");

  lv_init();
  delay(10);
  Serial.println("LVGL initialized");

  disp_draw_buf1 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * screenWidth * screenHeight / 8, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  disp_draw_buf2 = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * screenWidth * screenHeight / 8, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  if (!disp_draw_buf1 && !disp_draw_buf2) {
    Serial.println("LVGL disp_draw_buf allocate failed!");
    return;
  } else {
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf1, disp_draw_buf2, screenWidth * screenHeight / 8);

    /* Initialize the display */
    lv_disp_drv_init(&disp_drv);
    /* Change the following line to your display resolution */
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    Serial.println("Display initialized");

    // Carica le configurazioni da EEPROM (o usa quelle di default)
    loadConfig();
    Serial.print("Tentativo di connessione a: ");
    Serial.println(savedSSID);

    // Prova a connettersi al WiFi in modalità STA
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSSID, savedPassword);

    unsigned long startAttemptTime = millis();
    // Attendi fino a 5 secondi per il collegamento
    while (WiFi.status() != WL_CONNECTED && (millis() - startAttemptTime < 5000)) {
      delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi connesso");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    } else {
      // Se non si connette entro 5 secondi, avvia la modalità Access Point per la configurazione
      Serial.println("Connessione WiFi fallita. Avvio modalità Access Point per configurazione.");
      WiFi.disconnect(true, true);
      WiFi.mode(WIFI_AP);
      // Avvia un hotspot denominato "Termostat_Config" con password "12345678"
      bool result = WiFi.softAP("Thermostat_Config", "12345678");

      if (result) {
        Serial.println("Hotspot creato con successo!");
        Serial.print("SSID: ");
        Serial.println(WiFi.softAPSSID());
        Serial.print("IP: ");
        Serial.println(WiFi.softAPIP());
      } else {
        Serial.println("Errore nella creazione dell'Hotspot!");
      }

      // Configura il web server con gli endpoint per configurazione e reset
      server.on("/", HTTP_GET, handleRoot);
      server.on("/save", HTTP_POST, handleSave);
      server.on("/reset", HTTP_POST, handleReset);
      server.begin();
      Serial.println("Server di configurazione avviato");

      // Rimani in questo loop finché non vengono configurate le credenziali
      while (!wifiConfigured) {
        server.handleClient();
        delay(10);
      }
      // Riavvia il dispositivo per tentare la connessione con le nuove credenziali
      ESP.restart();
      return;
    }

    // Configura MQTT
    topic_mode.replace("{value}", savedTopic);
    topic_set_mode.replace("{value}", savedTopic);
    topic_temp.replace("{value}", savedTopic);
    topic_setpoint.replace("{value}", savedTopic);
    topic_set_setpoint.replace("{value}", savedTopic);
    topic_preset_mode.replace("{value}", savedTopic);
    topic_set_preset_mode.replace("{value}", savedTopic);

    client.setServer(mqtt_server, 1883);
    client.setCallback(mqtt_callback);

    // Initializing buttons
    button.attach(ROTARY_ENCODER_BUTTON_PIN, INPUT);
    button.interval(5);
    Serial.println("Bottone inizializzato");

    // Initialize the rotation encoder
    myEnc.write(0);  // Initialize the value of the encoder to 0

    // Crea l'interfaccia utente
    create_ui();

    Serial.println("Page caricata");
  }

  xTaskCreatePinnedToCore(
    lvglTask,    // Task function
    "lvglTask",  // Task name
    8192,        // Stack size
    NULL,        // Parameters
    2,           // Priority
    NULL,        // Task handle
    1            // Run on core 1
  );

  xTaskCreatePinnedToCore(
    inputTask,    // Task function
    "inputTask",  // Task name
    8192,         // Stack size
    NULL,         // Parameters
    2,            // Priority
    NULL,         // Task handle
    1             // Run on core 1
  );

  Serial.println("Setup done");
}

void loop() {
  if (!client.connected()) {
    connect_mqtt();
  }
  client.loop();
  delay(50);
}

// Task handler function
void lvglTask(void *pvParameters) {
  Serial.println("lvglTask started");
  while (true) {
    lv_timer_handler();            // Handle LVGL tasks
    vTaskDelay(pdMS_TO_TICKS(5));  // delay 5ms
  }
}

void inputTask(void *pvParameters) {
  Serial.println("inputTask started");

  static unsigned long lastArcUpdateTime = 0;          // Aggiorna l'arco ogni 50ms
  static unsigned long lastSetPointChangeTime = 0;     // Tempo dell'ultima variazione del setPoint
  static float lastPublishedSetPoint = setPoint;       // Ultimo valore del setPoint pubblicato via MQTT
  static unsigned long buttonPressTime = 0;            // Tempo in cui il bottone è stato premuto
  const unsigned long longPressSeasonThreshold = 250;  // Soglia per il long press Stagione in ms
  const unsigned long longPressResetThreshold = 5000;  // Soglia per il long press reset in ms
  unsigned long currentTime;

  while (true) {
    currentTime = millis();

    // Gestione dello standby: spegni il display dopo 120 sec di inattività
    if (displayOn && (currentTime - lastActivityTime >= 120000)) {
      Serial.println("Display in standby per inattività");
      digitalWrite(GFX_BL, HIGH);  // Spegne il backlight
      displayOn = false;
    }

    if (displayOn) {
      // Aggiorna lo stato del bottone
      button.update();
      if (button.fell()) {
        buttonPressTime = currentTime;
      }
      if (button.rose()) {
        lastActivityTime = currentTime;  // Aggiorna il timer di attività
        unsigned long pressDuration = currentTime - buttonPressTime;
        if (pressDuration >= longPressResetThreshold) {
          Serial.println("Very long press detected");
          resetEEPROM();
          ESP.restart();
        } else if (pressDuration >= longPressSeasonThreshold) {
          Serial.println("Long press detected");
          // Inserisci qui la logica per il long press
          if (strcmp(mode.c_str(), "Caldo") == 0) {
            mode = "Freddo";
            client.publish(topic_set_mode.c_str(), "cool");
          } else if (strcmp(mode.c_str(), "Freddo") == 0) {
            mode = "OFF";
            client.publish(topic_set_mode.c_str(), "off");
          } else if (strcmp(mode.c_str(), "OFF") == 0) {
            mode = "Caldo";
            client.publish(topic_set_mode.c_str(), "heat");
          }
          lv_label_set_text(label_mode, (mode).c_str());
        } else {
          Serial.println("Short press detected");
          if (strcmp(preset_mode.c_str(), "Eco") == 0) {
            preset_mode = "Boost";
            client.publish(topic_set_mode.c_str(), "boost");
          } else if (strcmp(preset_mode.c_str(), "Boost") == 0) {
            preset_mode = "Eco";
            client.publish(topic_set_preset_mode.c_str(), "eco");
          } //implementare altri preset a piacimento
          lv_label_set_text(label_preset, (preset_mode).c_str());
        }
      }
      // Gestione dell'encoder
      int32_t current_encoder_value = myEnc.read() / 2;
      int32_t encoder_diff = current_encoder_value - last_encoder_value;

      if (encoder_diff != 0) {
        lastActivityTime = currentTime;  // Aggiorna il timer di attività

        // Aggiorna il setPoint in memoria immediatamente (ogni 10ms)
        float newSetPoint = setPoint + encoder_diff * 0.5;  // ogni step incrementa/decrementa di 0.5°C
        if (newSetPoint < 5) newSetPoint = 5;
        if (newSetPoint > 35) newSetPoint = 35;
        if (newSetPoint != setPoint) {
          setPoint = newSetPoint;
          // Aggiorna l'interfaccia utente
          lv_label_set_text(label_setpoint, (String(setPoint) + "°C").c_str());
          // Registra il momento dell'ultima modifica
          lastSetPointChangeTime = currentTime;
        }
        last_encoder_value = current_encoder_value;
      }

      // Aggiornamento grafico dell'arco ogni 50ms
      if (currentTime - lastArcUpdateTime >= 50) {
        if (arcSP_widget != NULL) {
          lv_arc_set_value(arcSP_widget, (int)setPoint);
          update_background_colorSP();
        }
        lastArcUpdateTime = currentTime;
      }

      // Invia il valore via MQTT solo se il setPoint è stabile da almeno 1 secondo
      if ((currentTime - lastSetPointChangeTime >= 1000) && (setPoint != lastPublishedSetPoint)) {
        client.publish(topic_set_setpoint.c_str(), String(setPoint).c_str());
        lastPublishedSetPoint = setPoint;
        Serial.print("MQTT publish: ");
        Serial.println(setPoint);
      }
    } else {  // Gestione in caso di display spento
      // Gestione dell'encoder
      int32_t current_encoder_value = myEnc.read() / 2;
      int32_t encoder_diff = current_encoder_value - last_encoder_value;
      last_encoder_value = current_encoder_value;
      button.update();
      if (button.rose() || encoder_diff != 0) {
        digitalWrite(GFX_BL, LOW);  // Riaccende il backlight
        displayOn = true;
        Serial.println("Display riacceso");
        lastActivityTime = currentTime;
      }
    }
    // Delay del task impostato a 10ms per aggiornare frequentemente il valore
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void app_main() {
  setup();  // initial setup

  // Creating tasks
  // xTaskCreate(lvglTask, "lvglTask", 8192, NULL, 2, NULL);
  // xTaskCreate(inputTask, "inputTask", 8192, NULL, 2, NULL);
  xTaskCreatePinnedToCore(lvglTask, "lvglTask", 8192 * 2, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(inputTask, "inputTask", 8192 * 2, NULL, 2, NULL, 1);
  Serial.println("Tasks created");
}