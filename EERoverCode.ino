#define USE_WIFI_NINA false
#define USE_WIFI101 true
#define WINDOW_SIZE 500
#include <Arduino.h>
#include <WiFi101.h>
#include <WiFiWebServer.h>
#include "wiring_private.h"

const char ssid[] = "Ethan Iphone";
const char pass[] = "ethan1234";
const int groupNumber = 0;  // Set your group number to make the IP address constant - only do this on the EEERover network

Uart SUS( &sercom0, 8, 1, SERCOM_RX_PAD_2, UART_TX_PAD_0 );
unsigned long last = 0;
int radioFrequency = 0;
int infraredFrequency = 0;


// Minimized and optimized webpage to return when root is requested
const char webpage[] PROGMEM =
  "<html><head><style>\
.bc{text-align:center;font-size:20px;padding:20px;}\
</style></head><body>\
<div class='bc'>\
State: <span id='state'>OFF</span><br>\
Infrared: <span id='infrared'>0</span><br>\
Radio: <span id='radio'>0</span><br>\
Ultrasonic: <span id='ultrasonic'>0</span>\
</div>\
<script>\
function cmd(c){var x=new XMLHttpRequest();x.onreadystatechange=function(){\
if(x.readyState==4&&x.status==200){var r=JSON.parse(x.responseText);\
id('state').textContent=r.state;\
id('infrared').textContent=r.infrared;\
id('radio').textContent=r.radio;\
id('ultrasonic').textContent=r.ultrasonic;}};\
x.open('GET','/'+c);x.send();}\
function id(i){return document.getElementById(i);}\
document.addEventListener('keydown', function(event) {\
  switch(event.key) {\
    case 'w': cmd('forwards'); break;\
    case 's': cmd('backwards'); break;\
    case 'a': cmd('left'); break;\
    case 'd': cmd('right'); break;\
    case ' ': cmd('stop'); break;\
  }\
});\
setInterval(function(){cmd('status');},2000);\
</script></body></html>";

WiFiWebServer server(80);

// Return the web page
void handleRoot() {
  server.send(200, F("text/html"), webpage);
}

// Define motor control pins
const int enLeft = 2;
const int dirLeft = 3;
const int enRight = 4;
const int dirRight = 6;



String currentState = "";

void moveForwards() {
  digitalWrite(dirLeft, HIGH);
  digitalWrite(dirRight, HIGH);
  analogWrite(enLeft, 255);
  analogWrite(enRight, 255);
  currentState = "Forwards";
}

void moveBackwards() {
  digitalWrite(dirLeft, LOW);
  digitalWrite(dirRight, LOW);
  analogWrite(enLeft, 255);
  analogWrite(enRight, 255);
  currentState = "Backwards";
}

void turnLeft() {
  digitalWrite(dirLeft, LOW);
  digitalWrite(dirRight, HIGH);
  analogWrite(enLeft, 255);
  analogWrite(enRight, 255);
  currentState = "Left";
}

void turnRight() {
  digitalWrite(dirLeft, HIGH);
  digitalWrite(dirRight, LOW);
  analogWrite(enLeft, 255);
  analogWrite(enRight, 255);
  currentState = "Right";
}

void stop() {
  analogWrite(enLeft, 0);
  analogWrite(enRight, 0);
  currentState = "Stop";
}

void RI() {
  unsigned long now1 = micros();
  radioFrequency = 1000000 / (now1 - last);
  last = now1;
}

void AC_Handler (void) {
  unsigned long now2 = micros();
  infraredFrequency = 1000000 / (now2 - last);
  last = now2;
  REG_AC_INTFLAG |= AC_INTFLAG_COMP0; // Clear interrupt
}


// Handle sensor readings and return the status
void handleStatus() {
  int infraredValue = infraredFrequency;
  int radioValue = radioFrequency;
  String ultrasonicValue;
  while (Serial1.available()) {
    Serial1.readStringUntil(35);
    ultrasonicValue = Serial1.readStringUntil(35);
    Serial1.flush();
  }
  String response = "{\"state\":\"" + currentState + "\", \"infrared\":" + infraredValue + ", \"radio\":" + radioValue + ", \"ultrasonic\":\"" + ultrasonicValue + "\"}";
  server.send(200, "application/json", response);
}

// Generate a 404 response with details of the failed request
void handleNotFound() {
  String message = F("File Not Found\n\n");
  message += F("URI: ");
  message += server.uri();
  message += F("\nMethod: ");
  message += (server.method() == HTTP_GET) ? F("GET") : F("POST");
  message += F("\nArguments: ");
  message += server.args();
  message += F("\n");
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, F("text/plain"), message);
}

void setup() {
  // Set motor pins as output
  pinMode(enLeft, OUTPUT);
  pinMode(dirLeft, OUTPUT);
  pinMode(enRight, OUTPUT);
  pinMode(dirRight, OUTPUT);

  //UART
  SUS.begin(600); 
  pinPeripheral(8, PIO_SERCOM_ALT);


  //IR
  // Analog comparator (AC) configuration

  // Enable clock sources
  REG_PM_APBCMASK |= PM_APBCMASK_AC; // Enable APB Clock for AC (CLK_AC_APB)
  REG_GCLK_CLKCTRL = GCLK_CLKCTRL_ID_AC_ANA | GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0; // Enable comparator analog clock (GCLK_AC_ANA)
  REG_GCLK_CLKCTRL = GCLK_CLKCTRL_ID_AC_DIG | GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0; // Enable comparator digital clock (GCLK_AC_DIG)

  //SET PA04 as comparator AIN0
  pinMode(A3, INPUT);
  pinPeripheral(A3, PIO_AC_CLK);

  // Configure comparator 0
  REG_AC_COMPCTRL0 = AC_COMPCTRL_SPEED_HIGH | AC_COMPCTRL_MUXPOS_PIN0 | AC_COMPCTRL_MUXNEG_BANDGAP | AC_COMPCTRL_SWAP | AC_COMPCTRL_INTSEL_RISING | AC_COMPCTRL_HYST | AC_COMPCTRL_FLEN_MAJ5 | AC_COMPCTRL_ENABLE; // Configure and enable comparator 0
  REG_AC_INTENSET = AC_INTENSET_COMP0; // Enable Comparator interrupt for comparator 0
  REG_AC_EVCTRL |= AC_EVCTRL_COMPEO0; // Enable event output for comparator 0
  REG_AC_CTRLA |= AC_CTRLA_ENABLE; // Enable comparators

  NVIC_EnableIRQ (AC_IRQn); // Enable AC interrupts in NVIC

  Serial.begin(9600);

  // Wait 10s for the serial connection before proceeding
  // This ensures you can see messages from startup() on the monitor
  // Remove this for faster startup when the USB host isn't attached
  while (!Serial && millis() < 10000)
    ;

  Serial.println(F("\nStarting Web Server"));

  // Check WiFi shield is present
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println(F("WiFi shield not present"));
    while (true)
      ;
  }

  // Configure the static IP address if groupNumber is set
  if (groupNumber)
    WiFi.config(IPAddress(192, 168, 0, groupNumber + 1));

  // Attempt to connect to WiFi network
  Serial.print(F("Connecting to WPA SSID: "));
  Serial.println(ssid);
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }

  // Register the callbacks to respond to HTTP requests
  server.on(F("/"), handleRoot);
  server.on(F("/forwards"), [](){ moveForwards(); handleStatus(); });
  server.on(F("/backwards"), [](){ moveBackwards(); handleStatus(); });
  server.on(F("/left"), [](){ turnLeft(); handleStatus(); });
  server.on(F("/right"), [](){ turnRight(); handleStatus(); });
  server.on(F("/stop"), [](){ stop(); handleStatus(); });
  server.on(F("/status"), handleStatus);

  server.onNotFound(handleNotFound);

  server.begin();

  Serial.print(F("HTTP server started @ "));
  Serial.println(static_cast<IPAddress>(WiFi.localIP()));

  pinMode(9, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(9), RI, RISING);
}

// Call the server polling function in the main loop
void loop() {
  server.handleClient();
}
