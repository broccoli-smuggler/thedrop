#include <Arduino.h>
#include <ezButton.h>
#include <BluetoothA2DPSink.h>

#include "SPIFFS.h"
#include "BluetoothA2DPSource.h"

BluetoothA2DPSource a2dp_source;

// put function declarations here:
void update_ripems();
float revsPerMinute();
void RPMLED(float);

// RPM switch
const int input_RPM_pin = 14;
ezButton rpm_button(input_RPM_pin);

const int idle_time = 2500;
unsigned long last_falling_edge_time;
bool was_pressed = false;
float rpm = 0;
float last_rpm = 0.0;

// RPM light
const int output_RPM_led = 15;
unsigned long last_led_time;

// Audio
File heart;
float volume = 0.5;  // 0.0–1.0
float speed = 1.0;   // 0.5 = half speed, 2.0 = double
uint32_t dataStart = 0;
uint32_t dataSize = 0;
uint32_t sampleRate = 44100;
bool stereo = false;

void audio_data_generator(uint8_t *data, uint32_t len) {
  static float position = 0;

  for (uint32_t i = 0; i < len; i += 2) {
    if ((uint32_t)(dataStart + position * 2) >= (dataStart + dataSize)) {
      data[i] = data[i + 1] = 0;
      continue;
    }

    heart.seek(dataStart + (uint32_t)(position * 2));
    int16_t sample = 0;
    heart.read((uint8_t *)&sample, 2);

    sample = (int16_t)(sample * volume);  // Apply volume
    data[i] = sample & 0xFF;
    data[i + 1] = (sample >> 8) & 0xFF;

    position += speed;
  }
}

void read_wav_header() {
  heart = SPIFFS.open("/heartbeat1.wav", "r");

  uint8_t header[44];
  heart.read(header, 44);

  if (memcmp(&header[0], "RIFF", 4) != 0 || memcmp(&header[8], "WAVE", 4) != 0) {
    Serial.println("Invalid WAV file");
    return;
  }

  uint16_t channels = *(uint16_t*)&header[22];
  sampleRate = *(uint32_t*)&header[24];
  stereo = (channels == 2);
  dataSize = *(uint32_t*)&header[40];
  dataStart = 44;
  heart.seek(dataStart);
}

void setup() {
  // put your setup code here, to run once:
  SPIFFS.begin();
	Serial.begin(115200);
	pinMode(input_RPM_pin, INPUT_PULLUP);
  pinMode(output_RPM_led, OUTPUT);

	// attachInterrupt(input_RPM_pin, isr, RISING);
  rpm_button.setDebounceTime(10);
  rpm_button.resetCount();

  last_falling_edge_time = millis();
  last_led_time = millis();
  digitalWrite(output_RPM_led, HIGH);
  read_wav_header();
}

void loop() {
  update_ripems();

  if (! fabs(rpm - last_rpm) < 0.00001){
    Serial.printf("RPM: %f\n",rpm);
    last_rpm = rpm;
  }

  unsigned int sensor = analogRead(GPIO_NUM_36);
  // if (sensor < 1700 or sensor  > 2000)
      // Serial.printf("HB %u\n",sensor);
}

void update_ripems() {
  unsigned long revtime;
  unsigned long now = millis();
  rpm_button.loop(); // MUST call the loop() function first

  // if there is no button press in the idle time assume it is not rotating
  if (now - last_falling_edge_time > idle_time) {
    rpm = 0.0;
  }

  if (rpm_button.isPressed() && ! was_pressed) {
    was_pressed = true;
    digitalWrite(output_RPM_led, HIGH);

    if (a2dp_source.is_connected()) {
      a2dp_source.start("Avantree Lock RX");
      a2dp_source.reconnect();
    }

    a2dp_source.set_data_source(heart);
    Serial.print("playing?");
  }

  // on down edge.
  if (rpm_button.isReleased() && was_pressed){
    // Serial.printf("Off at %lu ms\n",now);
    revtime = now - last_falling_edge_time;
    // Serial.printf("revtime=%lu ms\n",revtime);
    rpm = 60000.0 / float(revtime) ;
    was_pressed = false;
    last_falling_edge_time = now;
    digitalWrite(output_RPM_led, LOW);
  }
}