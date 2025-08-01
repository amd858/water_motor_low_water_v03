#include <EEPROM.h>
#include <CRC8.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 4);  // set the LCD address to 0x27 for a 16 chars and 2 line display

#define BLUE_TANK_LOW_UNDIPPED 2
#define BLUE_TANK_FULL_UNDIPPED 3
#define BLUE_TANK_MAX_UNDIPPED 4
#define CEMENT_TANK_LOW_UNDIPPED 5
#define CEMENT_TANK_FULL_UNDIPPED 6
#define CEMENT_TANK_MAX_UNDIPPED 7
#define UNDERGROUND_TANK_LOW_UNDIPPED 14
#define UNDERGROUND_TANK_FULL_UNDIPPED 15
#define UNDERGROUND_TANK_MAX_UNDIPPED 16
#define BUZZER 13
#define BLUE_TANK_LED 10
#define CEMENT_TANK_LED 8
#define UNDERGROUND_TANK_LED 9
#define BLUE_SSR 12
#define CEMENT_SSR 11

byte tank_sensor_error = 0;

enum TankStates {
  START,
  BOUNCE_BEFORE_FILLING,
  FILLING,
  BOUNCE_BEFORE_OFF,
  DO_NOTHING
};
enum BuzzerStates {
  STATE_0,
  STATE_1,
  STATE_2,
  STATE_3,
  STATE_4
};
enum SensorStates {
  DIPPED,
  BOUNCE_BEFORE_NOT_DIPPED,
  NOT_DIPPED,
  BOUNCE_BEFORE_DIPPED,
};
enum TankFaultStates {
  TANK_NO_FAULT,
  TANK_BOUNCE_BEFORE_FAULT,
  TANK_FAULT
};
void setup() {
  pinMode(BLUE_TANK_LOW_UNDIPPED, INPUT_PULLUP);
  pinMode(BLUE_TANK_FULL_UNDIPPED, INPUT_PULLUP);
  pinMode(BLUE_TANK_MAX_UNDIPPED, INPUT_PULLUP);
  pinMode(CEMENT_TANK_LOW_UNDIPPED, INPUT_PULLUP);
  pinMode(CEMENT_TANK_FULL_UNDIPPED, INPUT_PULLUP);
  pinMode(CEMENT_TANK_MAX_UNDIPPED, INPUT_PULLUP);
  pinMode(UNDERGROUND_TANK_LOW_UNDIPPED, INPUT_PULLUP);
  pinMode(UNDERGROUND_TANK_FULL_UNDIPPED, INPUT_PULLUP);
  pinMode(UNDERGROUND_TANK_MAX_UNDIPPED, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  pinMode(BLUE_TANK_LED, OUTPUT);
  pinMode(CEMENT_TANK_LED, OUTPUT);
  pinMode(UNDERGROUND_TANK_LED, OUTPUT);
  pinMode(BLUE_SSR, OUTPUT);
  pinMode(CEMENT_SSR, OUTPUT);
  Serial.begin(115200);
  for (int i = 0; i < 3; i++) {
    digitalWrite(BLUE_TANK_LED, HIGH);
    digitalWrite(CEMENT_TANK_LED, HIGH);
    digitalWrite(UNDERGROUND_TANK_LED, HIGH);
    delay(200);
    digitalWrite(BLUE_TANK_LED, LOW);
    digitalWrite(CEMENT_TANK_LED, LOW);
    digitalWrite(UNDERGROUND_TANK_LED, LOW);
    delay(200);
  }
  // check error condition for beeping.
  if (check_sensors_error_condition()) {
    Serial.println("CRC is valid. Data is intact.");
    if (tank_sensor_error) {
      if (check_all_sensors_disconnected()) {
        Serial.print("CRC tank_sensor_error =");
        Serial.println(tank_sensor_error);
        tank_sensor_error = 0;
        EEPROM.put(0, tank_sensor_error);
        EEPROM.put(1, calculateCRC(tank_sensor_error));
      }
    }
  } else {
    Serial.println("CRC is not valid");
  }

  lcd.init();
}

bool check_all_sensors_disconnected() {
  // Loop through all sensor pins
  int sensorPins[] = {
    BLUE_TANK_LOW_UNDIPPED,
    BLUE_TANK_FULL_UNDIPPED,
    BLUE_TANK_MAX_UNDIPPED,
    CEMENT_TANK_LOW_UNDIPPED,
    CEMENT_TANK_FULL_UNDIPPED,
    CEMENT_TANK_MAX_UNDIPPED,
    UNDERGROUND_TANK_LOW_UNDIPPED,
    UNDERGROUND_TANK_FULL_UNDIPPED,
    UNDERGROUND_TANK_MAX_UNDIPPED
  };
  for (int i = 0; i < sizeof(sensorPins) / sizeof(sensorPins[0]); i++) {
    if (digitalRead(sensorPins[i]) == LOW) {
      return false;  // If any sensor pin is LOW, return false
    }
  }
  Serial.println("end");
  return true;  // If all pins are HIGH, return true
}

void print_level(unsigned char sensor_state) {
  bool is_dipped;
  if (sensor_state == DIPPED || sensor_state == BOUNCE_BEFORE_NOT_DIPPED) {
    is_dipped = true;
  } else {
    is_dipped = false;
  }
  if (is_dipped) {
    lcd.print("***");
  } else {
    lcd.print("---");
  }
}

void print_status(unsigned char *sensor_states, unsigned char *tankFaultyStates) {

  lcd.setCursor(0, 0);
  lcd.print("CEMENT");
  //lcd.print("CEM!!!");
  lcd.setCursor(5, 0);
  lcd.print("|");
  lcd.print("BLUE");
  //lcd.print("BL!!");
  lcd.setCursor(10, 0);
  lcd.print("|");
  lcd.print("UNDER");
  //lcd.print("UND!!");


  lcd.setCursor(0, 1);  //cement tank max
  print_level(sensor_states[5]);
  lcd.setCursor(5, 1);  //blue tank max
  lcd.print("|");
  print_level(sensor_states[2]);
  lcd.setCursor(10, 1);  //under tank max
  lcd.print("|");
  print_level(sensor_states[8]);

  //
  lcd.setCursor(16, 0);  //cement tank full
  print_level(sensor_states[4]);
  lcd.setCursor(21, 0);  //blue tank full
  lcd.print("|");
  print_level(sensor_states[1]);
  lcd.setCursor(26, 0);  //under tank full
  lcd.print("|");
  print_level(sensor_states[7]);

  //
  lcd.setCursor(16, 1);  // cement tank low
  print_level(sensor_states[3]);
  lcd.setCursor(21, 1);  //blue tank low
  lcd.print("|");
  print_level(sensor_states[0]);
  lcd.setCursor(26, 1);  // under tank low
  lcd.print("|");
  print_level(sensor_states[6]);
}

void loop() {
  static unsigned char blue_tank_current_state = START;
  static unsigned char cement_tank_current_state = START;
  static unsigned char buzzer_current_state = STATE_0;
  static int blue_tank_debounce_counter = 0;
  static int cement_tank_debouncec_counter = 0;

  static unsigned char sensorStates[9] = { DIPPED, DIPPED, DIPPED, DIPPED, DIPPED, DIPPED, DIPPED, DIPPED, DIPPED };
  static int sensorPins[] = {
    BLUE_TANK_LOW_UNDIPPED,
    BLUE_TANK_FULL_UNDIPPED,
    BLUE_TANK_MAX_UNDIPPED,
    CEMENT_TANK_LOW_UNDIPPED,
    CEMENT_TANK_FULL_UNDIPPED,
    CEMENT_TANK_MAX_UNDIPPED,
    UNDERGROUND_TANK_LOW_UNDIPPED,
    UNDERGROUND_TANK_FULL_UNDIPPED,
    UNDERGROUND_TANK_MAX_UNDIPPED
  };
  static int sensorDebouncecounters[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };

  static int tankFaultyDebouncecounters[3] = { 0, 0, 0 };
  static unsigned char tankFaultyStates[3] = { TANK_NO_FAULT, TANK_NO_FAULT, TANK_NO_FAULT };





  sensor_debounce_proccesing(sensorStates, sensorPins, sensorDebouncecounters);
  print_status(sensorStates, tankFaultyStates);
  bool blue_tank_filled = tankFilled(sensorStates[0], sensorStates[1], sensorStates[2], BLUE_TANK_LED);
  bool cement_tank_filled = tankFilled(sensorStates[3], sensorStates[4], sensorStates[5], CEMENT_TANK_LED);
  bool underground_tank_filled = tankFilled(sensorStates[6], sensorStates[7], sensorStates[8], UNDERGROUND_TANK_LED);

  bool blue_tank_empty = tankEmpty(sensorStates[0], sensorStates[1], sensorStates[2], BLUE_TANK_LED);
  bool cement_tank_empty = tankEmpty(sensorStates[3], sensorStates[4], sensorStates[5], CEMENT_TANK_LED);
  bool underground_tank_empty = tankEmpty(sensorStates[6], sensorStates[7], sensorStates[8], UNDERGROUND_TANK_LED);

  bool blue_tank_faulty = tankFaulty(sensorStates[0], sensorStates[1], sensorStates[2], BLUE_TANK_LED);
  bool cement_tank_faulty = tankFaulty(sensorStates[3], sensorStates[4], sensorStates[5], CEMENT_TANK_LED);
  bool underground_tank_faulty = tankFaulty(sensorStates[6], sensorStates[7], sensorStates[8], UNDERGROUND_TANK_LED);

  tankFaultyStates[0] = faulty_tank_processing(blue_tank_faulty, tankFaultyStates[0], tankFaultyDebouncecounters[0]);
  tankFaultyStates[1] = faulty_tank_processing(cement_tank_faulty, tankFaultyStates[1], tankFaultyDebouncecounters[1]);
  tankFaultyStates[2] = faulty_tank_processing(underground_tank_faulty, tankFaultyStates[2], tankFaultyDebouncecounters[2]);


  blue_tank_current_state = upper_tank_processing(blue_tank_current_state, blue_tank_filled, underground_tank_filled, BLUE_SSR, blue_tank_debounce_counter);
  cement_tank_current_state = upper_tank_processing(cement_tank_current_state, cement_tank_filled, underground_tank_filled, CEMENT_SSR, cement_tank_debouncec_counter);
  buzzer_current_state = buzzer_processing(buzzer_current_state);

  delay(50);
}
unsigned char faulty_tank_processing(bool tank_faulty, unsigned char faulty_tank_state, int &faulty_tank_debounce_counter) {
  static const int MAX_BOUNCE = 200;
  switch (faulty_tank_state) {
    case TANK_NO_FAULT:
      if (tank_faulty) {
        faulty_tank_state = TANK_BOUNCE_BEFORE_FAULT;
        faulty_tank_debounce_counter = 0;
      }
      break;
    case TANK_BOUNCE_BEFORE_FAULT:
      if (tank_faulty) {
        if (faulty_tank_debounce_counter >= MAX_BOUNCE) {
          setSensorError();
          faulty_tank_state = TANK_FAULT;
          faulty_tank_debounce_counter = 0;
        } else {
          faulty_tank_debounce_counter++;
        }
      } else {
        faulty_tank_state = TANK_NO_FAULT;
      }
      break;
    case TANK_FAULT:
      break;
  }
  return faulty_tank_state;
}

void sensor_debounce_proccesing(unsigned char *sensor_states, int *sensor_pins, int *sensor_debounce_counters) {
  static const int MAX_BOUNCE = 20;
  bool sensor_i_is_dipped = false;
  for (int i = 0; i < 9; i++) {
    sensor_i_is_dipped = !digitalRead(sensor_pins[i]);
    switch (sensor_states[i]) {
      case DIPPED:
        if (!sensor_i_is_dipped) {
          sensor_states[i] = BOUNCE_BEFORE_NOT_DIPPED;
          sensor_debounce_counters[i] = 0;
        }
        break;
      case BOUNCE_BEFORE_NOT_DIPPED:
        if (!sensor_i_is_dipped) {
          if (sensor_debounce_counters[i] >= MAX_BOUNCE) {
            sensor_states[i] = NOT_DIPPED;
            sensor_debounce_counters[i] = 0;
          } else {
            sensor_debounce_counters[i]++;
          }

        } else {
          sensor_states[i] = DIPPED;
        }
        break;
      case NOT_DIPPED:
        if (sensor_i_is_dipped) {
          sensor_states[i] = BOUNCE_BEFORE_DIPPED;
          sensor_debounce_counters[i] = 0;
        }
        break;
      case BOUNCE_BEFORE_DIPPED:
        if (sensor_i_is_dipped) {
          if (sensor_debounce_counters[i] >= MAX_BOUNCE) {
            sensor_states[i] = DIPPED;
            sensor_debounce_counters[i] = 0;
          } else {
            sensor_debounce_counters[i]++;
          }

        } else {
          sensor_states[i] = NOT_DIPPED;
        }
        break;
    }
  }
}

uint8_t calculateCRC(byte error_data) {
  CRC8 crc;
  crc.add(error_data);
  return crc.getCRC();
}


bool check_sensors_error_condition() {
  byte storedError;
  uint8_t storedCRC;
  EEPROM.get(0, storedError);
  EEPROM.get(1, storedCRC);
  CRC8 crc;
  crc.add(storedError);
  uint8_t recalculatedCRC = crc.getCRC();
  if (storedCRC == recalculatedCRC) {
    tank_sensor_error = storedError;
    return true;
  } else {
    return false;
  }
}
void setSensorError() {
  byte sensor_error = 1;
  tank_sensor_error = 1;  // global
  EEPROM.put(0, sensor_error);
  EEPROM.put(1, calculateCRC(sensor_error));
}

bool isDippedFromState(unsigned char sensor_state) {
  if (sensor_state == DIPPED || sensor_state == BOUNCE_BEFORE_NOT_DIPPED) {
    return true;
  }
  return false;
}

bool tankFaulty(unsigned char sensor_state_low, unsigned char sensor_state_full, unsigned char sensor_state_max, int led_pin) {
  bool low_dipped = false, full_dipped = false, max_dipped = false;
  low_dipped = isDippedFromState(sensor_state_low);
  full_dipped = isDippedFromState(sensor_state_full);
  max_dipped = isDippedFromState(sensor_state_max);

  if (max_dipped && (!full_dipped || !low_dipped)) {
    return true;
  }

  if (!max_dipped && full_dipped && !low_dipped) {
    return true;
  }
  return false;
}
bool tankEmpty(unsigned char sensor_state_low, unsigned char sensor_state_full, unsigned char sensor_state_max, int led_pin) {

  bool low_dipped = false, full_dipped = false, max_dipped = false;
  low_dipped = isDippedFromState(sensor_state_low);
  full_dipped = isDippedFromState(sensor_state_full);
  max_dipped = isDippedFromState(sensor_state_max);

  if ((low_dipped && full_dipped) || (full_dipped && max_dipped) || (low_dipped && max_dipped)) {
    digitalWrite(led_pin, HIGH);
    return true;
  } else {
    digitalWrite(led_pin, LOW);
    return false;
  }
}

bool tankFilled(unsigned char sensor_state_low, unsigned char sensor_state_full, unsigned char sensor_state_max, int led_pin) {

  bool low_dipped = false, full_dipped = false, max_dipped = false;
  low_dipped = isDippedFromState(sensor_state_low);
  full_dipped = isDippedFromState(sensor_state_full);
  max_dipped = isDippedFromState(sensor_state_max);

  if ((low_dipped && full_dipped) || (full_dipped && max_dipped) || (low_dipped && max_dipped)) {
    digitalWrite(led_pin, HIGH);
    return true;
  } else {
    digitalWrite(led_pin, LOW);
    return false;
  }
}

unsigned char buzzer_processing(unsigned char current_state) {
  static int counter = 0;
  static int on_counter = 5;
  static int off_counter = 2;
  static int delay_counter = 200;
  switch (current_state) {
    case STATE_0:
      if (tank_sensor_error) {
        Serial.println("buzzer_processing");
        counter = 0;
        current_state = STATE_1;
        digitalWrite(BUZZER, HIGH);
      }
      break;
    case STATE_1:
      if (counter >= on_counter) {
        digitalWrite(BUZZER, LOW);
        current_state = STATE_2;
        counter = 0;
      } else {
        counter++;
      }
      break;
    case STATE_2:
      if (counter >= off_counter) {
        digitalWrite(BUZZER, HIGH);
        current_state = STATE_3;
        counter = 0;
      } else {
        counter++;
      }
      break;
    case STATE_3:
      if (counter >= on_counter) {
        digitalWrite(BUZZER, LOW);
        current_state = STATE_4;
        counter = 0;
      } else {
        counter++;
      }
      break;
    case STATE_4:
      if (counter >= delay_counter) {
        digitalWrite(BUZZER, HIGH);
        current_state = STATE_1;
        counter = 0;
      } else {
        counter++;
      }
  }
  return current_state;
}

unsigned char upper_tank_processing(unsigned char current_state, bool upper_tank_filled, bool underground_tank_filled, int ssr_pin, int &debounceCounter) {

  static const int MAX_BOUNCE = 5 * 20;

  switch (current_state) {
    case START:
      if (!underground_tank_filled) {
        current_state = DO_NOTHING;
        digitalWrite(ssr_pin, LOW);

      } else {
        if (!upper_tank_filled) {
          current_state = BOUNCE_BEFORE_FILLING;
          debounceCounter = 0;
        }
      }
      break;
    case BOUNCE_BEFORE_FILLING:
      if (!underground_tank_filled) {
        current_state = DO_NOTHING;
        digitalWrite(ssr_pin, LOW);
      } else {
        if (!upper_tank_filled) {
          if (debounceCounter >= MAX_BOUNCE) {
            current_state = FILLING;
            digitalWrite(ssr_pin, HIGH);
          } else {
            debounceCounter++;
          }
        } else {
          current_state = START;
        }
      }
      break;
    case FILLING:
      if (!underground_tank_filled) {
        current_state = DO_NOTHING;
        digitalWrite(ssr_pin, LOW);
      } else {
        if (upper_tank_filled) {
          current_state = BOUNCE_BEFORE_OFF;
          debounceCounter = 0;
        }
      }
      break;
    case BOUNCE_BEFORE_OFF:
      if (!underground_tank_filled) {
        current_state = DO_NOTHING;
        digitalWrite(ssr_pin, LOW);
      } else {
        if (upper_tank_filled) {
          if (debounceCounter >= MAX_BOUNCE) {
            current_state = DO_NOTHING;
            digitalWrite(ssr_pin, LOW);
          } else {
            debounceCounter++;
          }
        } else {
          current_state = FILLING;
        }
      }
      break;

    case DO_NOTHING:
      Serial.println("hang");
      break;
  }
  return current_state;
}
