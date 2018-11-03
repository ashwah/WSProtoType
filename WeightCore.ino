/*
 WeightCore
 
 Arduino | HX711
 --------+-------  
 2       | CLK
 3       | DAT
 5V      | VCC
 GND     | GND
 
*/

// Include libraries.
#include "HX711.h"
#include "WiFiEsp.h"
#include "SoftwareSerial.h"

// Define pins.
#define CLK  2
#define DOUT  3
#define RX 4
#define TX 5
// RGB must be pins that function as analog.
#define RRR 9
#define GGG 10
#define BBB 11

// Define states.
#define INITIALISING 0
#define STABLE 1
#define CHANGING 2
#define SENDING 3


// Define calibration parameters.
#define THRESHOLD_DIFFERENCE 0.15
#define THRESHOLD_PERCENT 0.05
#define CHANGE_STEPS 5
#define CALIBRATION_FACTOR -10550.0

// Instantiate hardware interfaces.
SoftwareSerial esp8266(RX,TX);
WiFiEspClient client;
HX711 scale(DOUT, CLK);

// Declare global variables.
int state;
float display_weight;
float suggest_weight;
float current_weight;
char ssid[] = "xxx";
char pass[] = "xxx";
char server[] = "xxx";


/**
 * Setup.
 */
void setup() {
  
  Serial.begin(9600);
  esp8266.begin(9600);

  pinMode(RRR, OUTPUT);
  pinMode(GGG, OUTPUT);
  pinMode(RRR, OUTPUT);

  set_state(INITIALISING);
  WiFi.init(&esp8266);
  WiFi.begin(ssid, pass);

  Serial.println("WeightCore initalizing");

  // Init scale.
  scale.set_scale(CALIBRATION_FACTOR); 
  scale.tare(); 
  set_state(CHANGING);
}

/**
 * Loop.
 */
void loop() {  
  // In the STABLE state, continually check for a significant change in weight. If
  // there is one switch to the CHANGING state.
  if (state == STABLE) {
    delay(500);
    
    // Get the newest weight.
    current_weight = scale.get_units();
    Serial.print("Weight: ");
    Serial.println(current_weight);
    // If the weight has changed a significant amount, change state to CHANGING. 
    if (significant_change(current_weight, display_weight)) {
      suggest_weight = current_weight;
      set_state(CHANGING);      
    }
  }
  
  if (state == CHANGING) {
    // Take a load of readings until we're happy it's a stable reading.
    for (int i = CHANGE_STEPS; i > 0; i--) {
      Serial.print("i=");Serial.println(i);
      delay(1000);
      
      // Refresh current weight.
      current_weight = scale.get_units();

      // If there's a significant deviation from our suggestion, update the suggestion
      // to the current weight and start counting again.
      if (significant_change(current_weight, suggest_weight)) {
        suggest_weight = current_weight;
        return;
      }
    }

    // If there's no change from the display weight reset to STABLE.
    if (!significant_change(current_weight, display_weight)) {
      Serial.println("FALSE ALARM!");  
      set_state(STABLE);
      return;
    }

    Serial.print("SENDING TO API: ");    
    Serial.println(suggest_weight, 1);
    display_weight = suggest_weight;

    // Do all the stuff for "actual" weight changes.
    set_state(SENDING);
    send_display_weight(display_weight);
    set_state(STABLE);
  } 
}

/**
 * Helper function to calculate significant change.
 */
bool significant_change(float new_weight, float original_weight) {
  float difference;
  float percent_change;

  difference = fabs(new_weight - original_weight);

  if (fabs(original_weight) < THRESHOLD_DIFFERENCE) {
    return difference > THRESHOLD_DIFFERENCE;
  }
    
  percent_change = fabs(difference / original_weight);
  return difference > THRESHOLD_DIFFERENCE && percent_change > THRESHOLD_PERCENT;
}

void set_color(unsigned int red, unsigned int green, unsigned int blue)
{
  // Deduct values from 255 as we are using a common anode LED.
  red = 255 - red;
  green = 255 - green;
  blue = 255 - blue;
  analogWrite(RRR, red);
  analogWrite(GGG, green);
  analogWrite(BBB, blue);  
}

void set_state(int this_state) {
  switch (this_state) {
    case STABLE:
      set_color(0, 255, 0);
      state = STABLE;
      break;
    case CHANGING:
      set_color(255, 127, 0);
      state = CHANGING;
      break;    
    case INITIALISING:
      set_color(255, 0, 0);
      state = INITIALISING;
      break;
    case SENDING:
      set_color(255, 0, 255);
      state = SENDING;
      break;      
  }
}



void send_display_weight(float new_display_weight) {
    
  if (client.connect(server, 3000)) {
    Serial.println("Connected to server");
    // Make a HTTP request
    String content = "{\"reading\":" + String(new_display_weight) + ",\"device_id\":99999,\"hub_id\":999999}";
    client.println("POST /weights HTTP/1.1");
    client.println("Host: 167.99.192.47:3000");
    client.println("User-Agent: Arduino/1.0");
    client.println("Connection: keep-alive");    
    client.println("Accept: */*");    
    client.println("Content-Length: " + String(content.length()));
    client.println("Content-Type: application/json");
    client.println();
    client.println(content);
    client.println("Connection: close");
    client.println();
  }
}
