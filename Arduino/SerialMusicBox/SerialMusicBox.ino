
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

#define USMIN  600 // This is the rounded 'minimum' microsecond length based on the minimum pulse of 150
#define USMAX  2300 // This is the rounded 'maximum' microsecond length based on the maximum pulse of 600
#define SERVO_FREQ 50 // Analog servos run at ~50 Hz updates

// our servo # counter
uint8_t servo_num = 30;
uint8_t incomingByte = 0;
Adafruit_PWMServoDriver pwm[2];
uint16_t servo_pos[30];
bool increasing[30];
uint16_t short_pulse = 1000;
uint16_t long_pulse = 1800;
uint16_t servo_speed = 200;

int datapin = 12;
bool send_data = false;
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(datapin, OUTPUT); 
  
  Serial.begin(115200);
  Serial.println("30 channel Servo test!");

  pwm[0] = Adafruit_PWMServoDriver(0x40);
  pwm[1] = Adafruit_PWMServoDriver(0x41);
  for (int i = 0 ; i < 2; i++) {
    pwm[i].begin();
    pwm[i].setOscillatorFrequency(27000000);
    pwm[i].setPWMFreq(SERVO_FREQ);
  }
  delay(100);
  for (int i = 0 ; i < servo_num; i++) {
    servo_pos[i] = short_pulse;
    increasing[i] = false;
    setServoPosByID(i, short_pulse);
  }
}

void setServoPosByID(uint8_t id, uint16_t microsec) {
  uint8_t pwm_id = (id - id % 16) / 16;
  pwm[pwm_id].writeMicroseconds(id % 16, microsec);
}

void loop() {
  send_data = false;
  if (Serial.available() > 0) {
    while (Serial.available() > 0) {
      incomingByte = Serial.read()-97;
      if (incomingByte < servo_num && servo_pos[incomingByte] == short_pulse) {
        increasing[incomingByte] = true;
      }
    }
  }

  for (int i = 0 ; i < servo_num; i++) {
    if (increasing[i]) {
      servo_pos[i] += servo_speed;
      if (servo_pos[i] > long_pulse) {
        servo_pos[i] = long_pulse; increasing[i] = false;
      }
      send_data = true;
    } else {
      if (servo_pos[i] != short_pulse) {
        servo_pos[i] -= servo_speed;
        if (servo_pos[i] < short_pulse) servo_pos[i] = short_pulse;
      }
    }
    setServoPosByID(i, servo_pos[i]);
  }
  if (send_data) { 
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    digitalWrite(datapin, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
    digitalWrite(datapin, LOW);
  }
}
