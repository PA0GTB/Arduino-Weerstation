/* Arduino Weerstation met NodeMCU versie 1.2
****************************************************************************
Concept weerstation - basiscode geschreven door Koen Vervloesem en Martijn Overman van Reshift.nl
in Computer!Totaal april 2018
***************************************************************************
Arduino Weerstation gemaakt door Cor Struyk, 29 juli 2019
- Combinatie van weeralarm en weerstation
- Toepassing van een 3.2 inch TFT scherm 240 x 320 met ILI9341 driver
in de bijlagen zitten de lijsten met gegevens van alle weerstations
wijzig de regels : 37, 38, 40, 41, 43, 320 en 323 voor juiste keuze en gegevens

Modificatie sketch : November 2019 : aanpassing Vorst signalering PA0GTB en opschoning regels
--------------------------------------------------------------------------
Instelling voor het Arduino weerstation  
display toont :
- luchtvochtigheid in procenten
- temperatuur in graden C
- windrichting en windsnelheid in Bft
- luchtdruk in hPa
- regenval in mmu
- Vrijzicht afstand

De gekleurde Led geeft de geldende KNMI weer-alarmcode aan en evt. vorstindicatie (Wit)
Weerinformatie wordt opgehaald bij geselecteerd KNMI meetstation
De regenindicatie komt via Buienradar
***************************************************************************
Voor testdoeleinden kan de Seriele Monitor ingeschakeld worden
*/

#include <ESP8266WiFi.h>
#include <TFT_eSPI.h> // Graphics and font library for ILI9341 driver chip
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h

// Stel hier de Wifi access parameters in.
const char* ssid = "hier SSID van Wifi station";
const char* password = "hier wachtwaard van Wifi station";
const String rainurl = "/data/raintext/?lat=51.34&lon=4.56"; // Vervang lat en lon door de coordinaten van het geselecteerde weerstatiom met twee decimalen.
const String weerstation="6350"; // Meetstation Gilze-Rijen, zie bijlagen voor de volledige lijst
const String weerurl="/weermonitor.php?weerstation=" + weerstation;
const String alarmurl = "/weeralarm.php?regio=noord-brabant"; // Zie bijlage voor de volledige lijst (gebruik kleine letters).

// Aansluiting IO poorten van NodeMCU voor LEDs 
const int rood   =  2; // D4
const int oranje = 15; // D8
const int geel   =  0; // D3
const int groen  =  3; // RX  
const int wit    = 16; // D0

#define ROW_SIZE 12

const int wachttijd = 300000; // 5 minuten.
const int periode = 1; // Aantal perioden van 5 minuten vooruit, minimumaantal is 1.
unsigned long previousMillis = 0;
const char* weerhost = "arduino.reshift.nl";
String raindomain = "gpsgadget.buienradar.nl";

const int httpPort = 80;
bool nat = false;
bool ijs = false;
bool alarm = false;

const String code_rood="warning-overview--red";
const String code_oranje="warning-overview--orange";
const String code_geel="warning-overview--yellow";
const String code_groen="warning-overview--green";
const String params_url = "/nu.php";
const String regen_begin_tag = "<regen>";
const String regen_einde_tag = "</regen>";
const String tijd_begin_tag = "<tijdstip>";
const String tijd_einde_tag = "</tijdstip>";
String nu = "";

WiFiClient client;

void setup() {
  Serial.begin(9600);
  pinMode(wit, OUTPUT);
  pinMode(groen, OUTPUT);
  pinMode(geel, OUTPUT);
  pinMode(oranje, OUTPUT);
  pinMode(rood, OUTPUT);
  
  digitalWrite(wit, LOW);
  digitalWrite(groen, LOW);
  digitalWrite(geel, LOW);
  digitalWrite(oranje, LOW);
  digitalWrite(rood, LOW);
  
// Verbind met WiFi-netwerk
  Serial.println();
  Serial.println();
  Serial.print("Verbind met ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi verbonden");
  Serial.print("IP-adres: ");
  Serial.println(WiFi.localIP());

  Serial.print("Verbind met ");
  Serial.println(weerhost);
  
  if (!client.connect(weerhost, httpPort)) {
    Serial.println("Verbinding mislukt");
    client.stop();
    return;
  }

  // Haal actuele raindomain op (standaard is dat gpsgadget.buienradar.nl).
  Serial.print("Vraag URL op: ");
  Serial.println(params_url);
  client.print(String("GET ") + params_url + " HTTP/1.1\r\n" +
                 "Host: " + weerhost + "\r\n\r\n");
               
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println("Verbinding mislukt: timeout");
      client.stop();
      return;
    }
  }

  String inhoud = "";
  while(client.available() && inhoud==""){
    String regel = client.readStringUntil('\r');
    int begin_pos = regel.indexOf(regen_begin_tag);
    if(begin_pos != -1) {
      int einde_pos = regel.indexOf(regen_einde_tag);
      String inhoud = regel.substring(begin_pos + regen_begin_tag.length(), einde_pos);
      raindomain = inhoud;
      Serial.print("Rainhost: ");
      Serial.println(raindomain);
    }
  }

// Testrun voor alle Led's
  digitalWrite(wit, HIGH);
  delay(500);
  digitalWrite(wit, LOW);
  digitalWrite(groen, HIGH);
  delay(500);
  digitalWrite(groen, LOW);
  digitalWrite(geel, HIGH);
  delay(500);
  digitalWrite(geel, LOW);
  digitalWrite(oranje, HIGH);
  delay(500);
  digitalWrite(oranje, LOW);
  digitalWrite(rood, HIGH);
  delay(500);
  digitalWrite(rood, LOW);

  tft.init();
  tft.setRotation(1); // Draai het display naar de landscape positie 

}

void toon_boodschap(String boodschap, int rij = 0, int kolom = 0) {
  char boodschap_char[boodschap.length()+1];
  boodschap.toCharArray(boodschap_char, boodschap.length()+1);
  tft.setCursor(kolom*10, rij*ROW_SIZE);
  tft.println(boodschap_char);

}

void lees_tot_tag(String begin_tag, String einde_tag, String toevoeging = "", int rij = 0, int kolom = 0, int vorst = 0) {
  String inhoud = "";
  while(client.available() && inhoud==""){
    String regel = client.readStringUntil('\r');
    int begin_pos = regel.indexOf(begin_tag);
    if(begin_pos != -1) {
      int einde_pos = regel.indexOf(einde_tag);
      inhoud = regel.substring(begin_pos + begin_tag.length(), einde_pos) + toevoeging;
      toon_boodschap(inhoud, rij, kolom);
    }
  }
  if (vorst != 0 && inhoud.indexOf('-') != -1 && inhoud.length() > 4) {
    digitalWrite(wit, HIGH);
    if (ijs == false) {
      ijs = true;
    }
  }
  else if (vorst!= 0) {
    digitalWrite(wit, LOW);
    ijs = false;
  }
}


void checkalarm() {

  int gevonden = 0;
  Serial.print("Verbind met ");
  Serial.println(weerhost);
  
  if (!client.connect(weerhost, httpPort)) {
    Serial.println("Verbinding mislukt");
    client.stop();
    return;
  }

  Serial.print("Vraag URL op: ");
  Serial.println(alarmurl);

  client.print(String("GET ") + alarmurl + " HTTP/1.1\r\n" +
                 "Host: " + weerhost + "\r\n\r\n");
               
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println("Verbinding mislukt: timeout");
      client.stop();
      return;
    }
  }

  while(client.available() && gevonden == 0){
    String regel = client.readStringUntil('\r');
    if(regel.indexOf(code_rood) != -1) {
      digitalWrite(rood, HIGH);
      digitalWrite(oranje, LOW);
      digitalWrite(geel, LOW);
     digitalWrite(groen, LOW);
      if(alarm == false){
          alarm = true;
      }
      gevonden = 1;
    } else if(regel.indexOf(code_oranje) != -1) {
      digitalWrite(oranje, HIGH);
      digitalWrite(rood, LOW);
      digitalWrite(geel, LOW);
      digitalWrite(groen, LOW);
      if(alarm == false){
         alarm = true;
      }
      gevonden = 1;
    } else if(regel.indexOf(code_geel) != -1) {
      digitalWrite(geel, HIGH);
      digitalWrite(rood, LOW);
      digitalWrite(oranje, LOW);
      digitalWrite(groen, LOW);
      if(alarm == false){
         alarm = true;
      }
      gevonden = 1;
    } else if(regel.indexOf(code_groen) != -1) {
      digitalWrite(groen, HIGH);
      digitalWrite(rood, LOW);
      digitalWrite(oranje, LOW);
      digitalWrite(geel, LOW);
      alarm = false;
      gevonden = 1;
    } 
  }

  Serial.println();
  Serial.println("Verbreek verbinding");
  client.stop();
}

void checkweer() {
  int weerstation_gevonden = 0;
  bool tonen = false;

  Serial.print("Verbind met ");
  Serial.println(weerhost);
  
  if (!client.connect(weerhost, httpPort)) {
    Serial.println("Verbinding mislukt");
    client.stop();
    return;
  }

  Serial.print("Vraag URL op: ");
  Serial.println(weerurl);

  client.print(String("GET ") + weerurl + " HTTP/1.1\r\n" +
                 "Host: " + weerhost + "\r\n\r\n");

  unsigned long time;  // is voor looptesten           
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println("Verbinding mislukt: timeout");
      client.stop();
      return;
    }
    tonen = true;
  }

  if (tonen == true){
  tft.fillScreen(TFT_BLACK);
  tft.setTextWrap(true);
  tft.setTextSize(1);
  tft.setTextFont(2);  
  tft.setTextColor(TFT_WHITE);  // Instelling zwarte achtergrond en eventuele tekst wissen
   
  //  display.begin();  
  }

  while(client.available() && weerstation_gevonden==0){
    String regel = client.readStringUntil('\r');
    if(regel.indexOf("<stationcode>" + weerstation) != -1){
      weerstation_gevonden = 1;      
    }
  }

  if(weerstation_gevonden==0) {
    toon_boodschap("Weerstation " + weerstation + " niet gevonden");
  } else {
    tft.setTextSize(3);  // Font grootte
//  tft.setTextSize(2);  // Zet deze regel vrij bij langere naam weerstation !
    tft.setCursor(0, 0);
    tft.setTextColor(TFT_RED);
    tft.println("Gilze-Rijen");   // tekst aanpassen bij keuze ander weerstation !
    tft.setTextSize(2);
    tft.setTextColor(TFT_YELLOW);

    lees_tot_tag("<luchtvochtigheid>", "</luchtvochtigheid>", " % luchtvochtigheid", 5);
    tft.setTextSize(2);
    lees_tot_tag("<temperatuurGC>", "</temperatuurGC>", " graden C ", 7, 0, 1);
    lees_tot_tag("<windsnelheidBF>", "</windsnelheidBF>", " Bft", 9, 19 );
    lees_tot_tag("<windrichting>", "</windrichting>", "  snelheid", 9);
    lees_tot_tag("<luchtdruk>", "</luchtdruk>", " hPa", 11);
    lees_tot_tag("<zichtmeters>", "</zichtmeters>", " m", 13);
    lees_tot_tag("<regenMMPU>", "</regenMMPU>", " mmu", 13, 18);

    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    lees_tot_tag(" zin=\"", "\">", "", 15);
  }

  Serial.println();
  Serial.println("Verbreek verbinding");
  client.stop();
}

String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;
    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void loop() {
  checkweer();
  checkalarm();
  delay(wachttijd);
}
