/*
 *      
 * Modified from Simple Stepper Motor Control Example Code by Dejan Nedelkovski, www.HowToMechatronics.com
 *  
 */
// defines pins numbers
const int stepPin = 3; 
const int dirPin = 4; 

int curr_note = 60;
const float semitone = 1.05946; // 12th root of 2. Pitch difference on a semitone
float hz = 520.0; // Middle C frequence
int d; // delay after pressing a note in mircoseconds. The note stops after 1 second
unsigned long c = 0;
unsigned long myTime;

void setup() {
  // Sets the two pins as Outputs
  pinMode(stepPin,OUTPUT); 
  pinMode(dirPin,OUTPUT);
  d = 1000000/hz;
  Serial.begin(9600);
  digitalWrite(dirPin,HIGH); // Enables the motor to move in a particular direction
}
void loop() {
  if (Serial.available() > 0) {
    while (Serial.available() > 0) {
      int note = Serial.read();
      Serial.println(note);

      // if a note is detected, we shift the frequency
      if (note>curr_note) {
        for (int i = 0; i< note-curr_note; i++) {
            hz = hz * semitone;
        }
      }
      else {
        for (int i = 0; i< curr_note-note; i++) {
            hz = hz / semitone;
        }
      }
      // Calculate the delay in milliseconds
      curr_note = note;
      d = 1000000/hz;
      c = myTime;
    }
  }
  // play the note by delaying and stepping the motor
  myTime = millis();
  if (myTime - 1000 < c) {
      digitalWrite(stepPin,HIGH);
      delayMicroseconds(d);
      digitalWrite(stepPin,LOW);
      delayMicroseconds(d);
  }
}
