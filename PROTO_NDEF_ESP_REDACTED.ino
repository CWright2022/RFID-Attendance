//This file has been redacted to redact personal info like my SSID, password, and some other things.
//Code is entirely functional if these are replaced with real values in the section designated below
//It's kinda a Frankenstein, made up of mostly a heavily modified NDEF reading sketch with some MQTT bits.

#include <SPI.h>
#include <PN532_SPI.h> //nfc and SPI stuff
#include <PN532.h>
#include <NfcAdapter.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

PN532_SPI pn532spi(SPI, 3); //idk what this does, but the number is the CS pin number
NfcAdapter nfc = NfcAdapter(pn532spi);//this too

//*******************************************PIN NUMBERS AND OTHER USEFUL SETUP GLOBALS***********************************

const char* mqtt_server = "BROKER IP HERE";
char ssid[] = "SSID HERE";
char password[]  = "PASSWORD HERE";

int redPin = 5;
int greenPin = 4; //change these for RGB LED pin #s, make sure to use PWM pins
int bluePin = 0;
int buzzPin = 16;
int auxPin = 2;

char inTopic[] = "INPUT TOPIC HERE"; //topic used for input TO this device
char outTopic[] = "OUTPUT TOPIC HERE";//topic for output FROM this device
//************************************************************************************************************************
WiFiClient espClient;
PubSubClient client(espClient);//idk what this does, maybe starts PubSub?

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  //Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setColor(int red, int green, int blue) //now let's get the RGB LED setup
{
  map(red, 0, 255, 0, 1023);
  map(green, 0, 255, 0, 1023);//map values for ESP 10bit ADC, easer to copy-paste RGB values
  map(blue, 0, 255, 0, 1023);
  analogWrite(redPin, red);
  analogWrite(greenPin, green);
  analogWrite(bluePin, blue);
}
/*
  this is input stuff. Not really useful for this project, just more for POC at this point. Maybe use for OTA updates? Remote overrides? Shutdowns?
  currently connected to an auxillary LED for show.
*/
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(auxPin, HIGH);   // Turn the LED on
  } else {
    digitalWrite(auxPin, LOW);  // Turn the LED off by making the voltage LOW
  }
}
//RECONNECT LOGIC BELOW
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a client ID
    String clientId = "door1";
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(outTopic, "reconnect occurred");
      // ... and resubscribe
      client.subscribe(inTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void setup(void) {
  int buzzPin = 16;//idk why I gotta do this, but it works this way
  Serial.begin(115200);
  nfc.begin();//starts NFC service
  Serial.println("ready for NFC tag");
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);//set all appropriate pins as outputs
  pinMode(buzzPin, OUTPUT);
  pinMode(auxPin, OUTPUT);
  setup_wifi(); //declared earlier
  client.setServer(mqtt_server, 1883); //sets broker IP and port. IP is at top of file
  client.setCallback(callback); //set callback action, also defined earlier
}

void loop(void) {
  if (!client.connected()) {
    reconnect();  //reconnect loop here, ensure that client is connected
  }
  client.loop();
  setColor(255, 0, 0);//make status LED red
  digitalWrite(buzzPin, LOW);//shuts off buzzer

  if (nfc.tagPresent())//when we get a tag
  {
    String payloadAsString = "";
    NfcTag tag = nfc.read();
    Serial.println(tag.getTagType());
    Serial.print("UID: "); Serial.println(tag.getUidString());

    if (tag.hasNdefMessage()) // every tag won't have a message
    {

      NdefMessage message = tag.getNdefMessage();
      Serial.print("\nThis NFC Tag contains an NDEF Message with ");
      Serial.print(message.getRecordCount());
      Serial.print(" NDEF Record");
      if (message.getRecordCount() != 1) {
        Serial.print("s");
      }
      Serial.println(".");

      // cycle through the records, printing some info from each
      int recordCount = message.getRecordCount();
      for (int i = 0; i < recordCount; i++)
      {
        Serial.print("\nNDEF Record "); Serial.println(i + 1);
        NdefRecord record = message.getRecord(i);
        // NdefRecord record = message[i]; // alternate syntax

        Serial.print("  TNF: "); Serial.println(record.getTnf());
        Serial.print("  Type: "); Serial.println(record.getType()); // will be "" for TNF_EMPTY

        // The TNF and Type should be used to determine how your application processes the payload
        // There's no generic processing for the payload, it's returned as a byte[]
        int payloadLength = record.getPayloadLength();
        byte payload[payloadLength];
        record.getPayload(payload);

        // Print the Hex and Printable Characters
        Serial.print("  Payload (HEX): ");
        PrintHexChar(payload, payloadLength);

        // Force the data into a String (might work depending on the content)
        // Real code should use smarter processing (HAHA LOL NOPE)
        for (int c = 0; c < payloadLength; c++) {
          payloadAsString += (char)payload[c];
        }
        payloadAsString.remove(0, 3);
        Serial.print("  Payload (as String): ");
        Serial.println(payloadAsString);

        // id is probably blank and will return ""
        String uid = record.getId();
        if (uid != "") {
          Serial.print("  ID: "); Serial.println(uid);
        }
      }
    }
    setColor(252, 161, 3);//yellow, for "I read this tag but haven't verified you yet
    client.publish(outTopic, String(payloadAsString).c_str(), true);
    delay(100);
  }
  delay(400);
}
