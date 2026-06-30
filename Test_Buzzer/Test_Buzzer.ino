/*
 * Test_Buzzer.ino - Chi test coi GPIO 25 (digitalWrite HIGH)
 * Serial Monitor: 9600
 */
#define BUZZER_PIN 25

static bool on = false;

void buzzerOn() {
  if (on) return;
#if defined(ESP32) && defined(SOC_DAC_SUPPORTED)
  dacDisable(BUZZER_PIN);
#endif
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
  on = true;
}

void buzzerOff() {
  if (!on) return;
  digitalWrite(BUZZER_PIN, LOW);
  on = false;
}

void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.println("\n=== TEST COI GPIO 25 ===");
  Serial.println("Bat 2s / tat 2s — lap lai");
}

void loop() {
  Serial.println("[TEST] ON");
  buzzerOn();
  delay(2000);
  Serial.println("[TEST] OFF");
  buzzerOff();
  delay(2000);
}
