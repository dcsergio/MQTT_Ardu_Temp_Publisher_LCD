#include <LiquidCrystal.h>
#include <OneWire.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>

/*
  In questa applicazione si utilizza in termometro digitale DS18B20
  ed un display LCD 16x2, per visualizzare la temperatura e inviarla ad un broker MQTT

*/
#define COLONNE 16
#define RIGHE 2
#define UPDATE_DELAY_SEC 5

//#define BUTTON_MAX 11
//#define BUTTON_MIN 12
// DS18S20 Temperature chip i/o
OneWire ds(9);  // on pin 8
LiquidCrystal lcd(2, 3, 5, 6, 7, 8);
byte data[12];
byte addr[8];
byte simbolo[8] = {
  B00110,
  B01001,
  B01001,
  B00110,
  B00000,
  B00000,
  B00000,
};

byte i;
int HighByte, LowByte, TReading, SignBit;
long int Tc_100;
long int Tc_100_Min = 25000;
long int Tc_100_Max = -10000;
unsigned long time = 0;

const char* server = "m23.cloudmqtt.com";
const int mqttPort = 12873;
const char* mqttUser = "rptjayur";
const char* mqttPassword = "PGhWwGq20Tb-";
char topic[10] = "home_temp";

// assign a MAC address for the ethernet controller.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
// fill in your address here:
byte mac[] = {0xDE, 0xFE, 0xCA, 0x5E, 0xDE, 0xC1};
// fill in an available IP address on your network here,
// for manual configuration:
// da provare
// anche con IP e gateway diversi
IPAddress ip(192, 168, 1, 7);
IPAddress myDns(1, 1, 1, 1);
IPAddress gateway(192, 168, 1, 254);
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}
// initialize the library instance:

EthernetClient ethClient;
PubSubClient client(server, mqttPort, callback, ethClient);

const unsigned long postingInterval = UPDATE_DELAY_SEC * 1000; //delay between updates



void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("arduinoClient", mqttUser, mqttPassword)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(topic, "reconnect");
      // ... and resubscribe
      //client.subscribe("inTopic");
    } else {
      stampaStringa(String(client.state()), "failed");
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup(void) {


  Serial.begin(9600);
  Ethernet.begin(mac, ip, myDns, gateway);

  pinMode(13, OUTPUT);
  lcd.begin(COLONNE, RIGHE);
  lcd.print("Warmup...");

  if ( !ds.search(addr)) {
    lcd.setCursor(0, 0);
    lcd.print("No addresses.\n");
  }
  lcd.createChar(1, simbolo);
  lcd.setCursor(0, 0);
  lcd.print("                ");


  delay(1500);
}

void loop(void) {
  time = millis();
  digitalWrite(13, HIGH);
  leggiTemp();
  stampaTempC(Tc_100, "    ");
  stampaTempF(Tc_100);
  digitalWrite(13, LOW);

  if (!client.connected()) {
    reconnect();
  }
  String message =  makeTemperatureString(Tc_100);
  char copy[50];
  message.toCharArray(copy, 50);
  client.publish(topic, copy);
  //client.loop();

  delay(postingInterval - 750);
  stampaStringa("publish", "home_temp");
  delay(postingInterval);
}

void stampaStringa(String a, String b) {
  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 0);
  lcd.print(a);

  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(b);
}

//stampa sul dispay la temperatura in Fahrenheit sul rigo 2
void stampaTempF(long int Tc_100) {

  int F_Whole = 32 + (9 * Tc_100 / 5) / 100;  // separate off the whole and fractional portions
  int F_Fract = (3200 + 9 * Tc_100 / 5) % 100;


  lcd.setCursor(0, 1);
  lcd.print("          ");
  lcd.setCursor(0, 1);
  if (F_Whole > 0) {
    lcd.print(" ");
  }
  lcd.print(F_Whole);
  lcd.print(".");
  if (F_Fract < 10)
  {
    lcd.print("0");
  }
  lcd.print(F_Fract);
  lcd.setCursor(10, 1);
  lcd.write(1);
  lcd.print("F");
}

//stampa sul dispay la temperatura in Centigradi sul rigo 1
//aggingendo una stringa alla fine, negli ultimi 4 spazi
void stampaTempC(long int Tc_100, String a) {

  int Whole = abs(Tc_100) / 100;  // separate off the whole and fractional portions
  int Fract = abs(Tc_100) % 100;

  lcd.setCursor(0, 0);
  lcd.print("          ");
  lcd.setCursor(0, 0);
  if (Tc_100 < 0) // If it is negative
  {
    lcd.print("-");
  }
  else {
    lcd.print(" ");
  }

  lcd.print(Whole);
  lcd.print(".");
  if (Fract < 10)
  {
    lcd.print("0");
  }
  lcd.print(Fract);

  lcd.setCursor(10, 0);
  lcd.write(1);
  lcd.print("C");

  lcd.setCursor(12, 0);
  lcd.print(a);
}


void leggiTemp() {

  if ( OneWire::crc8( addr, 7) != addr[7]) {
    lcd.setCursor(0, 0);
    lcd.print("CRC is not valid!\n");
    return;
  }

  if ( addr[0] == 0x10) {
    //lcd.print("DS18S20");
  }
  else if ( addr[0] == 0x28) {
    //lcd.print("DS18B20");
  }
  else {
    lcd.print("Unknown device");
    return;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end

  delay(750);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.

  ds.reset();
  ds.select(addr);
  ds.write(0xBE);         // Read Scratchpad

  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
  }

  LowByte = data[0];
  HighByte = data[1];
  TReading = (HighByte << 8) + LowByte;
  SignBit = TReading & 0x8000;  // test most sig bit
  if (SignBit) // negative
  {
    TReading = (TReading ^ 0xffff) + 1; // 2's comp
  }
  Tc_100 = (6 * TReading) + TReading / 4;    // multiply by (100 * 0.0625) or 6.25


  if (SignBit) {
    Tc_100 = -Tc_100;
  }
}

String makeTemperatureString(long int value) {
  String a = String(value / 100);
  String fill = (value % 100) < 10 ? "0" : "";
  String b = String(value % 100);
  return String(a + "." + fill + b);
}



