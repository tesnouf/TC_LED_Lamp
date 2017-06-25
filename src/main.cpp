
// #include <Arduino.h>
/*
   Connect to the network
   Subscribe to multiple MQTT channels
   Select light sequence based on MQTT channel payload

   Sketch subscribes to MQTT channels, converts the payload to an INTEGER and sets a MODE variable based on the payload.
   mode variable determines the actions taken through the loop statement.


GRRRRR I had this running then lost it all in a git upload... thought that was why git existed
learnign experience that one!

Rewritten and needs to be tested before a clean up is run

*/
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "dimmer.h"
#include "config.h"


/*
  To add a LDR to pin A0
  Set a variable to record the LDR results if LDR < x then set variable mode to 1 if > x then set to 0
  Aditional Variable ManMQTT boolean allows for the light to be controlled by MQTT messages OR by the LDR!
  Add in WiFi set up that turns off base station transmission
*/
// Enable debug prints
#define MY_DEBUG

// delete this section and switch out the comments in config.h if this does not work
const char* ssid = dSSID ;
const char* password = dPASSWORD ;
const char* mqtt_server = dMQTT_SERVER ;


WiFiClient espClient;
PubSubClient client(espClient);

int function;  //sets the light function
String mode; // sets automatic or manual
int LDRValue;
int LDRSetValue = 500;  // potentially goes from 0 - 1023  need to set this up and then see what values are returned in the serial monitor.


// Setup details for items to control
int ledPin1 = D7; //LED Light Strip
int LDRPin = A0; // Light Dependant Resistor
int toLevel;
int currentLevel;
#define FADE_DELAY 10  // Delay in ms for each percentage fade up/down (10ms = 1s full-range dim)



// LED Light timer Variables
unsigned long previousMillisLED = 0;
const long LEDinterval = 2000;
int ledState = LOW;

//LDR Resistor timer variables
unsigned long previousMillisLDR = 0;
const long LDRinterval = 500;



// void fadeToLevel(int toLevel);
void fadeToLevel( int toLevel )
{

    int delta = ( toLevel - currentLevel ) < 0 ? -1 : 1;
    Serial.println(delta);
    Serial.println(toLevel);
    Serial.println(currentLevel);

    while ( currentLevel != toLevel ) {
        currentLevel += delta;
        // analogWrite( ledPin1, (int)(currentLevel / 100. * 255) );
        analogWrite( ledPin1, (int)(currentLevel * 10.23) );
        // analogWrite( ledPin1, (int)(currentLevel / 100 * 255) ); // test without decimal
        delay( FADE_DELAY );
    }
}

void dimmerTransition() {
        //  Retrieve the power or dim level from the incoming request message
        int requestedLevel = toLevel;
        // Clip incoming level to valid range of 0 to 100
        requestedLevel = requestedLevel > 100 ? 100 : requestedLevel;
        requestedLevel = requestedLevel < 0   ? 0   : requestedLevel;

        Serial.print( "Changing level to " );
        Serial.print( requestedLevel );
        Serial.print( ", from " );
        Serial.println( currentLevel );
        fadeToLevel( requestedLevel );
}
void dimmer() {
  if (currentLevel != toLevel) {
  dimmerTransition();
} else {
  analogWrite( ledPin1, (int)(toLevel * 10.23) );
}
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (BUILTIN_LED == HIGH) {
      digitalWrite(BUILTIN_LED, LOW);
    } else {
      digitalWrite(BUILTIN_LED, HIGH);
    }
  }
  #ifdef MY_DEBUG
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  #endif

}


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  payload[length] = '\0'; //layloaf length null??
  String strTopic = String((char*)topic);  // Convert the Topic to a string to make it easier to run statements on it.
  String strPayload = String((char*)payload); // Convert the Payload to a string for display
  int intPayload = strPayload.toInt();    //convert the MQTT payload to an Integer
  bool boolPayload = payload;
  // May be able to clean this up more with
  // int intPayload = (char*)payload.toInt();  //do the conversion in one line and the strTopic and strPayload ar redundant


  // This needs to be developed to grab the interger value form the payload and store it as the mode value
  if (strTopic == "home/TC/function") {
    function = intPayload;
  }

  if (strTopic == "home/TC/modetype") {
    mode = strPayload;  //Likely that this will not work - need ot confirm that boolean and integer types are set up correctly...
  }

  if (strTopic == "home/TC/LDRLimit") {
    LDRSetValue = intPayload * 10.23;  //grab the % value from the dimer nad convert to a 0-1023 value...
  }
  if (strTopic == "home/TC/Dimmer") {
    toLevel = intPayload;
    // dimmerTransition();
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("TC_LED_Client")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("home/TCNode", "1");  //LET OpenHAB know that I am online!
      //Add in chnge to the publish to allow OpenHAB to determine the node is active.
      // ... and resubscribe
      client.subscribe("home/TC/function");
      client.subscribe("home/TC/modetype");
      client.subscribe("home/TC/LDRLimit");
      client.subscribe("home/TC/Dimmer");


    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 3 seconds before retrying
      delay(3000);
    }
  }
}

// Set the lights flashing
void flashing() {
  unsigned long currentMillisLED = millis();

  if (currentMillisLED - previousMillisLED >= LEDinterval) {
    previousMillisLED = currentMillisLED;

    if (ledState == LOW) {
      toLevel = 100;
      dimmer();
      ledState == HIGH;
    } else {
      toLevel = 0;
      ledState = LOW;
      dimmer();
    }
    // digitalWrite(ledPin1, ledState);
    // analogWrite( ledPin1, 0 );
  }
}

// If the mode is set to Manual then run a case statement to work out which function determines the operating type.
void manual() {
  // Do something with the LEDs based on the function value
  switch (function) {
    case 0:
      // if (ledState == HIGH) {
      // digitalWrite(ledPin1, LOW);   // Turn the LED OFF at D7
      analogWrite( ledPin1, 0 );
      //}
      break;
    case 1:
      // if (ledState == LOW) {
      // digitalWrite(ledPin1, HIGH);   // Turn the LED ON at D7
      analogWrite( ledPin1, 1023 );
      // }
      break;
    case 2:
      flashing(); //Run the flashng function
      break;
    // case 3:
    //   fadeToLevel(int toLevel);
    //   break;  // if this is enabled must comment out from callback and add in button in OpenHAB
  }

}

//IF the mode is set to automatic then every 0.5 secs check the Light resistor reading.  If it is dark turn on the light, if it's light turn it off.
// This may actually work better with an interrupt running on the LDR (google searhc Arduino interrupts)
void automatic() {
  unsigned long currentMillisLDR = millis();
  if (currentMillisLDR - previousMillisLDR >= LDRinterval) {
    previousMillisLDR = currentMillisLDR;
    // Get the LDR value if below x then turn on the light otherwise turn off
    LDRValue = analogRead(LDRPin);
    Serial.print("LDR Reading is:" + LDRPin);
    if (LDRValue < LDRSetValue) {
      digitalWrite(ledPin1, HIGH); // Turn on LEDs if the light is lower than the LDR set value
    } else {
      digitalWrite(ledPin1, LOW);
    }
  }
}


void setup() {
   // Add in setup notes to turn off station SSID broadcast
  WiFi.mode(WIFI_STA); // options WIFI_AP, WIFI_STA, or WIFI_AP_STA
  //WiFi.softAP("TCLEDNode"); // This works!!
  pinMode(BUILTIN_LED, OUTPUT); //Initialize the BUILTIN_LED pin as an output
  digitalWrite(BUILTIN_LED, HIGH); // Initialize the BUILTIN_LED pin as off
  pinMode(ledPin1, OUTPUT);     //Set the LED pin

  pinMode(LDRPin, INPUT);       //Set the LDR as an input
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  #ifdef MY_DEBUG
  digitalWrite(ledPin1, HIGH);
  delay(500);
  digitalWrite(ledPin1,  LOW);
  //Create an IP Variable
  IPAddress ip = WiFi.localIP();
  // Convert it to a string that can be printed
  String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  #endif

}

void loop() {
  // If client disconnected try to reconnect - turn a LED on in this case, turn it off when reconnected.
  if (!client.connected()) {
    reconnect();
    // digitalWrite(ledPin1, LOW);   // Turn the LED on may need to add in a off clause somewhere ?? start of loop?
  } else {
    //digitalWrite(ledPin1, HIGH);   // Turn the LED on Currently disabled so that
  }

  // Loop through the MQTT function to get current values for variables.
  client.loop();

  if ( mode = "OFF") {
    // manual();
    dimmer();

  } else {
    manual();
  }

}
