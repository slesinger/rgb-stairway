/*
To test Schody, just publish message over MQTT:
mosquitto_pub -h localhost -p 8883 -u hass -P <mqtt password> -t schody/cmd -m <see below>

0:10(120r0g0b)1000(124R125g125B)1000(120r0g0b)1000(0r120g0b)1000(0r0g120b)1000(0r0g0b)
1:10(120r0g0b)1000(124R125g125B)1000(120g0r0b)1000(0r120b0g)1000(0b0g120r)1000(0r0g0b)

Exaplanation of the formatr:
<schod>:period(<r>r<g>g<b>b)...

1 period = 5ms

Stairs are split to two part upper part (nahore) and bottom part (dole) 
*/

#include "secrets.h"
#include <Arduino.h>

#include <WiFi.h>
#include <NeoPixelBus.h>

//needed for library
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h> //https://pubsubclient.knolleary.net/api.html
#include "driver/rmt.h"


// builtin LED
//#define LED_BUILTIN 5
//#define DEBUG_PIN 27

#define RMT_TX_CHANNEL RMT_CHANNEL_0


bool was_falling_dole = false;
bool was_falling_nahore = false;

// which effect to display for start
RgbColor barva(10, 220, 150);
RgbColor black(0);
RgbColor c_null(0);
RgbColor red(128,0,0);
RgbColor green(0,128,0);
RgbColor blue(0,0,128);

NeoPixelBus<NeoGrbFeature, NeoEsp32I2s0800KbpsMethod> strip_dole(NUM_SCHODU/2*3, STRIP_DOLE_PIN);
NeoPixelBus<NeoGrbFeature, NeoEsp32I2s1800KbpsMethod> strip_nahore(NUM_SCHODU/2*3, STRIP_NAHORE_PIN);

WiFiManager wifiManager;
WiFiClient espClient;
PubSubClient client(espClient);

boolean irdole_publish = false;
boolean irnahore_publish = false;

void setSchod(int id, RgbColor moje_barva)
{
  // moje_barva.Darken(255 - intenzita);
  if (id <= 7) {
    int i = id * 3;
    strip_dole.SetPixelColor(i, moje_barva);
    strip_dole.SetPixelColor(i+1, moje_barva);
    strip_dole.SetPixelColor(i+2, moje_barva);
  }
  else {
    int i = (15 - id) * 3;
    strip_nahore.SetPixelColor(i, moje_barva);
    strip_nahore.SetPixelColor(i+1, moje_barva);
    strip_nahore.SetPixelColor(i+2, moje_barva);
  }
}

void showSchody(){
  strip_dole.Show();
  strip_nahore.Show();
}

/* #Program
   for each stair-step
    commands:
      - //set_color (period, color) - same as blend(0, color)
      - blend (period, color) - as this is only command, does not need to specify in program, is implicit
    command parameter
      - period uint_16
      - target color
*/
struct RgbColorFloat {
  float R;
  float G;
  float B;
  float deltaR;
  float deltaG;
  float deltaB;
};
struct Program_step {
  uint16_t period;
  RgbColor target_color;
};
Program_step program[NUM_SCHODU][NUM_PROG_STEPS];
unsigned int program_pointers[NUM_SCHODU];
RgbColorFloat program_current_color[NUM_SCHODU];

void program_clear() {}

void program_load(byte* prg, unsigned int length) {
    // char prg[] = "0:1200(124R125g127B)0(0R0g0b)\n1:1201(121R121g121B)1(1R1g1b)";
    
    //init zero values
    for (int schod = 0; schod < NUM_SCHODU; schod++) {
      program_pointers[schod] = NUM_PROG_STEPS;   // pause execution
      program_current_color[schod] = {.R = 0, .G = 0, .B = 0, .deltaR = 0, .deltaG = 0, .deltaB = 0};

      for (int step = 0; step < NUM_PROG_STEPS; step++) {
        program[schod][step] = { .period = LAST_PROG_STEP, .target_color = black};
      }
    }

    //load from string
    unsigned int num = 0;
    unsigned int schod = 0;
    unsigned int step = 0;
    uint16_t period = 0;
    unsigned int r = 0;
    unsigned int g = 0;
    unsigned int b = 0;
    unsigned char* curr_prg = prg;
    for (unsigned int i = 0; i < length; i++) {
        unsigned char ch = *curr_prg;
        
        if (ch >= '0' && ch <= '9') {
            num = num*10 + ch-48;
        }
        if (ch == ':') {
            if (schod != num) step = 0;
            schod = num;
            num = 0;
        }
        if (ch == '(') {
            period = num;
            num = 0;
        }
        if (ch == 'R' || ch == 'r') {
            r = num;
            num = 0;
        }
        if (ch == 'G' || ch == 'g') {
            g = num;
            num = 0;
        }
        if (ch == 'B' || ch == 'b') {
            b = num;
            num = 0;
        }
        if (ch == ')') {
            // program[schod][step] = { .period = period, .target_color = RgbColor(r,g,b)};
            program[schod][step] = { .period = period, .target_color = RgbColor(r%127,g%127,b%127)};
            step++;
        }
        curr_prg++;
    }

    for (int schod = 0; schod < NUM_SCHODU; schod++) {
      RgbColor targ_col = program[schod][0].target_color;
      program_current_color[schod].deltaR = ((float)targ_col.R - 0) / (float)program[schod][0].period;
      program_current_color[schod].deltaG = ((float)targ_col.G - 0) / (float)program[schod][0].period;
      program_current_color[schod].deltaB = ((float)targ_col.B - 0) / (float)program[schod][0].period;
      program_pointers[schod] = 0;   // exceutes from 0 to NUM_PROG_STEPS
    }

  client.publish("schody/sviti", "on", true);
  ma_se_poslat_konec_sviceni = true;

}

void task_led_strip(void *parameter) {
  while (true) {

    //Execute program for each stair
    for (int schod = 0; schod < NUM_SCHODU; schod++) {

      if (program_pointers[schod] == NUM_PROG_STEPS) {
        continue; //program executed completely, skip this stair
      }

      if (program[schod][program_pointers[schod]].period == 0) {
        program_pointers[schod]++; //period for given step passed, go to next program step
        //calculate blending deltas
        RgbColor targ_col = program[schod][program_pointers[schod]].target_color;
        program_current_color[schod].deltaR = ((float)targ_col.R - program_current_color[schod].R) / (float)program[schod][program_pointers[schod]].period;
        program_current_color[schod].deltaG = ((float)targ_col.G - program_current_color[schod].G) / (float)program[schod][program_pointers[schod]].period;
        program_current_color[schod].deltaB = ((float)targ_col.B - program_current_color[schod].B) / (float)program[schod][program_pointers[schod]].period;
      }
      else if (program[schod][program_pointers[schod]].period == LAST_PROG_STEP) { // period indicates end of program
        program_pointers[schod] = NUM_PROG_STEPS; //set pointer as last step
        if (ma_se_poslat_konec_sviceni == true) {
          ma_se_poslat_konec_sviceni = false;
          client.publish("schody/sviti", "off", true);
        }     
        continue;
      }
      else {
        program[schod][program_pointers[schod]].period--;
        program_current_color[schod].R += program_current_color[schod].deltaR;
        program_current_color[schod].G += program_current_color[schod].deltaG;
        program_current_color[schod].B += program_current_color[schod].deltaB;
        RgbColor new_col((uint8_t)program_current_color[schod].R, (uint8_t)program_current_color[schod].G, (uint8_t)program_current_color[schod].B);
        setSchod(schod, new_col); //TODO vylepsit setSchod API - odebrat intensity
      }
    }
    showSchody();
    vTaskDelay( 3 / portTICK_PERIOD_MS);
  }
}


void setup_wifi()
{
  wifiManager.autoConnect(WIFI_SETUP_AP_NAME, WIFI_SETUP_AP_PASS); //AP tohoto ESP
  wifiManager.setWiFiAutoReconnect(true);
  Serial.print("IP address: ");                        //kdyz se pripoji na stromecek
  Serial.println(WiFi.localIP());
}

struct Button
{
  const uint8_t PIN;
  uint32_t numberKeyPresses;
  bool pressed;
};
Button ir_rcvr_dole = {IR_RECV_DOLE_PIN, 0, false};
Button ir_rcvr_nahore = {IR_RECV_NAHORE_PIN, 0, false};
//Button debug_button = {15, 0, false};

//void IRAM_ATTR isr_debug()
//{
//  digitalWrite(DEBUG_PIN, LOW);
//}

void IRAM_ATTR isr_dole()
{
  was_falling_dole = true;
}
void IRAM_ATTR isr_nahore()
{
  was_falling_nahore = true;
}

void setup_ir() {

  //setup transmitter LED
  rmt_config_t config;
  config.rmt_mode = RMT_MODE_TX;
  config.channel = RMT_TX_CHANNEL;
  config.gpio_num = IR_LED_NAHORE_PIN;
  config.mem_block_num = 1;
  config.tx_config.loop_en = 0;
  config.tx_config.carrier_en = 1;
  config.tx_config.idle_output_en = 0;
  config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  config.tx_config.carrier_duty_percent = 50;
  config.tx_config.carrier_freq_hz = 36000;
  config.tx_config.carrier_level = RMT_CARRIER_LEVEL_HIGH;
  config.clk_div = 255; //maximum resolution

  ESP_ERROR_CHECK(rmt_config(&config));
  ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

  // setup receiver
  pinMode(ir_rcvr_dole.PIN, INPUT_PULLUP);
  attachInterrupt(ir_rcvr_dole.PIN, isr_dole, FALLING);

  pinMode(ir_rcvr_nahore.PIN, INPUT_PULLUP);
  attachInterrupt(ir_rcvr_nahore.PIN, isr_nahore, FALLING);
}

rmt_item32_t rmt_items[] = {
    {{{ 150, 1, 300, 0 }}}, // dot
};

#define TICKS_GUARDED_PERIOD 5000
#define TICKS_VALIDATION_PERIOD 50
#define TICKS_BLOCKED 45 //minimum ticks with blocked IR during validation period

void task_tx_ir(void *parameter)
{
  unsigned int ticks = 0;
  unsigned int ticks_guarded = 0;
  unsigned int countdown_publish_dole = 0;
  unsigned int countdown_publish_nahore = 0;
  while (true) {
    int number_of_items = sizeof(rmt_items) / sizeof(rmt_items[0]);
    rmt_write_items(RMT_TX_CHANNEL, rmt_items, number_of_items, true);
    vTaskDelay( 1 / portTICK_PERIOD_MS / 2);  //[ms]  500us

    if (ticks_guarded > 0) {  //Do not register IR blocking after mqtt IR block published
      ticks_guarded--;
    }
    else {
      if (ticks > 0) ticks--;

      if (ticks == 0 && countdown_publish_dole > TICKS_BLOCKED) {
        irdole_publish = true;
        countdown_publish_dole = 0;
        ticks_guarded = TICKS_GUARDED_PERIOD;
      }
      if (ticks == 0 && countdown_publish_nahore > TICKS_BLOCKED) {
        irnahore_publish = true;
        countdown_publish_nahore = 0;
        ticks_guarded = TICKS_GUARDED_PERIOD;
      }

      if ( (was_falling_dole != true) && (digitalRead(ir_rcvr_dole.PIN)) ) {
        if (ticks == 0) { // ticks count down needs to start
          ticks = TICKS_VALIDATION_PERIOD;
          countdown_publish_dole = 0;
        }
        countdown_publish_dole++;  // count ticks during IR bocked
      }
      was_falling_dole = false;

      if ( (was_falling_nahore != true) && (digitalRead(ir_rcvr_nahore.PIN)) ) {
        if (ticks == 0) {
          ticks = TICKS_VALIDATION_PERIOD;
          countdown_publish_nahore = 0;
        }
        countdown_publish_nahore++;
      }
      was_falling_nahore = false;
    }
    vTaskDelay( 3 / portTICK_PERIOD_MS);  //[ms]  3ms
  }
}

void mqtt_handler(const char* topic, byte* payload, unsigned int length) {
  // if (topic == 'schody/program')
  program_load(payload, length);
}

void mqtt_reconnect()
{
  if (!client.connected())
  {
    Serial.printf("Connection state %d. Connecting to MQTT...", client.state());

    if (client.connect(MQTT_TOPIC, MQTT_USER, MQTT_PASS), true)
    {
      Serial.println("connected");
      client.setBufferSize(4096); //to fit long program sent to schody/cmd
      client.setCallback(&mqtt_handler);
      client.subscribe("schody/cmd", 0);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
    }
  }
}
void setup_mqtt()
{
  client.setServer(MQTT_HOST, MQTT_PORT);
  mqtt_reconnect();
}

void task_mqtt_status_publish(void *parameter) {
  while (true) {
    if (!client.connected()) {
      mqtt_reconnect();
      client.publish("schody/status", String("RECONNECTED").c_str(), true);
    }
    client.publish("schody/status", String("OK").c_str(), true);

    vTaskDelay( 10000 / portTICK_PERIOD_MS);
  }
}

void task_mqtt_ir_publish(void *parameter) {
  while (true) {
    if (irdole_publish) {
      client.publish("schody/ir/dole", "on", true);
      irdole_publish = false;
      client.loop();
      vTaskDelay( 1000 / portTICK_PERIOD_MS);
      client.publish("schody/ir/dole", "off", true);
    }
    if (irnahore_publish) {
      client.publish("schody/ir/nahore", "on", true);
      irnahore_publish = false;
      client.loop();
      vTaskDelay( 1000 / portTICK_PERIOD_MS);
      client.publish("schody/ir/nahore", "off", true);
    }
    client.loop();
    vTaskDelay( 500 / portTICK_PERIOD_MS);
  }
}


void setup() {
  // sanity check delay - allows reprogramming if accidently blowing power w/leds
  delay(2000);

  Serial.begin(9600);
  strip_dole.Begin();
  strip_nahore.Begin();
  program_load((byte*)PROGRAM_INIT, strlen(PROGRAM_INIT));
  showSchody();
  setup_ir();
  setup_wifi();
  delay(1000);
  setup_mqtt();
  //pinMode(DEBUG_PIN, OUTPUT);
  //pinMode(LED_BUILTIN, OUTPUT);
  //digitalWrite(DEBUG_PIN, LOW);

  //pinMode(debug_button.PIN, INPUT_PULLUP);
  //attachInterrupt(debug_button.PIN, isr_debug, FALLING);


  /*
  &taskLedStrip,     Task function.
  "TaskLedStrip",    String with name of task.
  10000,             Stack size in words.
  NULL,              Parameter passed as input of the task
  1,                 Priority of the task. Higher number high prio up to 32
  NULL);             Task handle. */
  xTaskCreate(&task_led_strip, "TaskLedStrip", 10000, NULL, 16, NULL);
  xTaskCreate(&task_tx_ir, "TaskTxIr", 10000, NULL, 2, NULL);
  xTaskCreate(&task_mqtt_status_publish, "TaskMqttPublish", 10000, NULL, 1, NULL);
  xTaskCreate(&task_mqtt_ir_publish, "TaskMqttPublish", 10000, NULL, 1, NULL);
}

void loop()
{
  delay(100);
}
