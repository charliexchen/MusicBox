

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

uint16_t servo_reset[30] = {1818, 898, 1843, 1033, 1754, 1052, 1763, 1003, 1759, 1001, 1719, 1093, 1838, 967, 1793, 1038, 1795, 853, 1780, 962, 1827, 979, 1786, 1070, 1884, 1061, 1820, 1008, 1687, 1052};
uint16_t servo_idle_tight[30] = {1459, 1338, 1534, 1413, 1441, 1456, 1438, 1431, 1408, 1401, 1395, 1457, 1495, 1377, 1486, 1415, 1493, 1248, 1473, 1301, 1477, 1317, 1488, 1390, 1592, 1415, 1541, 1337, 1385, 1376};
uint16_t servo_idle[30] = {1541, 1258, 1594, 1370, 1527, 1383, 1498, 1299, 1518, 1337, 1468, 1385, 1559, 1315, 1571, 1322, 1538, 1191, 1555, 1200, 1552, 1269, 1580, 1317, 1674, 1369, 1635, 1255, 1466, 1361};
uint16_t servo_play[30] = {1322, 1500, 1321, 1537, 1283, 1564, 1239, 1548, 1271, 1509, 1191, 1635, 1328, 1507, 1265, 1585, 1349, 1397, 1315, 1456, 1299, 1461, 1321, 1564, 1397, 1568, 1370, 1466, 1226, 1521};


uint8_t reset_counter[30];
uint8_t reset_count_max = 4;

uint8_t relay_pin = 5;

int8_t full_note_counter[30];
uint8_t note_on[30];
uint8_t full_note_sustain = 3;//3



const int16_t disable_power_frames = 60 * 20;
int16_t disable_power_count;
bool shut_down = false;
#define USMIN  600 // This is the rounded 'minimum' microsecond length based on the minimum pulse of 150
#define USMAX  2300 // This is the rounded 'maximum' microsecond length based on the maximum pulse of 600
#define SERVO_FREQ 50 // Analog servos run at ~50 Hz updates

// our servo # counter
uint8_t servo_num = 30;
uint8_t incomingByte = 0;
Adafruit_PWMServoDriver pwm[2];
uint16_t servo_pos[30];
uint16_t short_pulse = 1000;
uint16_t long_pulse = 1800;
uint16_t servo_speed = 200;

int datapin = 12;
bool send_data = false;
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(datapin, OUTPUT);
  pinMode(relay_pin, OUTPUT);
  disable_power_count = disable_power_frames;
  Serial.begin(9600);
  Serial.println("Initalising Music Box...");
  digitalWrite(relay_pin, LOW);
  delay(500);
  pwm[0] = Adafruit_PWMServoDriver(0x40);
  pwm[1] = Adafruit_PWMServoDriver(0x41);
  for (int i = 0 ; i < 2; i++) {
    pwm[i].begin();
    pwm[i].setOscillatorFrequency(27000000);
    pwm[i].setPWMFreq(SERVO_FREQ);
    delay(1000);
  }
  digitalWrite(relay_pin, HIGH);
  delay(500);
  for (int i = 0 ; i < servo_num; i++) {
    reset_counter[i] = 0;
    full_note_counter[i] = -1;
    note_on[i] = false;
  }
  digitalWrite(datapin, HIGH);
  delay(100);
  digitalWrite(datapin, LOW);
  for (int i = 0 ; i < servo_num; i++) {
    setServoPosByID(i, servo_play[i]);
    delay(30);
  }
  // Send all servos to the "reset" position
  for (int i = 0 ; i < servo_num; i++) {
    setServoPosByID(i, servo_reset[i]);
    delay(30);
  }
  // Rotate the barrel to reset all the plates
  digitalWrite(datapin, HIGH);
  delay(100);
  digitalWrite(datapin, LOW);
  // Send all servos to the "idle" position
  for (int i = 0 ; i < servo_num; i++) {
    servo_pos[i] = servo_idle[i];
    setServoPosByID(i, servo_pos[i]);
    delay(30);
  }
  Serial.println("Startup Complete!");
}

void setServoPosByID(uint8_t id, uint16_t microsec) {
  uint8_t pwm_id = (id - id % 16) / 16;
  pwm[pwm_id].writeMicroseconds(id % 16, microsec);
}

void loop() {
  if (shut_down) return;
  send_data = false;
  if (Serial.available() > 0) {
    while (Serial.available() > 0) {
      incomingByte = Serial.read();
      disable_power_count = disable_power_frames;
      digitalWrite(relay_pin, HIGH);
      if (incomingByte >= 97) {
        if (servo_pos[incomingByte - 97] != servo_reset[incomingByte - 97]) {
          note_on[incomingByte - 97] = true;
          full_note_counter[incomingByte - 97] = full_note_sustain;
        }
      }
      else {
        note_on[incomingByte - 65] = false;
      }
      send_data = true;
    }
  }
  for (int i = 0 ; i < servo_num; i++) {
    if (servo_pos[i] == servo_idle[i] &&  note_on[i] == false && full_note_counter[i] < 0) {
      setServoPosByID(i, servo_pos[i]);
      continue;
    }

    if (full_note_counter[i] >= 0 || note_on[i]) {
      if (full_note_counter[i] >= 0)
      {
        full_note_counter[i] -= 1;
      }

      servo_pos[i] = servo_play[i];
    }
    else {
      servo_pos[i] = servo_reset[i];
      reset_counter[i] += 1;
      if (reset_counter[i] == reset_count_max) {
        servo_pos[i] = servo_idle[i];
        reset_counter[i] = 0;
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
  if (disable_power_count == 0) {
    digitalWrite(relay_pin, LOW);
    disable_power_count--;
  }
  else if (disable_power_count > 0) {
    disable_power_count--;
  }
}
