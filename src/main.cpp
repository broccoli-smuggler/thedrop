#include <Arduino.h>

// put function declarations here:
int revsPerMinute(unsigned long);

const int input_RPM_pin = 25;
unsigned long last_switch_time;
int RPM;

// RPM interupt
void IRAM_ATTR isr() {
  last_switch_time = millis();
}

void setup() {
  // put your setup code here, to run once:
	Serial.begin(115200);
	pinMode(input_RPM_pin, INPUT_PULLUP);
	attachInterrupt(input_RPM_pin, isr, FALLING);
  last_switch_time = 0;
  RPM = 0;
}

void loop() {
  // put your main code here, to run repeatedly:
  if (last_switch_time != 0)
  {
    RPM = revsPerMinute(millis() - last_switch_time);
    Serial.print(RPM);
  }
}

int revsPerMinute(unsigned long time_diff) {
  return (int)time_diff;
}