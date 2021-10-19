#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include "SinricPro.h"
#include "SinricProThermostat.h"
#include <ESP8266HTTPClient.h>
#include <EEPROM.h>


#define APP_KEY           "---------------------------"      // Should look like "de0bxxxx-1x3x-4x3x-ax2x-5dabxxxxxxxx"
#define APP_SECRET        "-----------------------------------"   // Should look like "5f36xxxx-x3x7-4x3x-xexe-e86724a9xxxx-4c4axxxx-3x3x-x5xe-x9x3-333d65xxxxxx"
#define THERMOSTAT_ID     "-------------------------------"    // Should look like "5dc1564130xxxxxxxxxxxxxx"

#define RELAY_PIN D6
#define BUTTON_PIN D7

const char* wifiAd="Termostat_Daire_1_Röle_1";  // daire röle değişirken değiş
const char* serverName="/durum/daire1/role1";

IPAddress staIP(192, 168, 1, 111);   // daire röle değişirken ip değiş

IPAddress apSubnet(255, 255, 0, 0);
IPAddress gateway(192, 168, 1, 1);
IPAddress dns1(8,8,8,8);
IPAddress dns2(8,8,4,4);

//----------------------------------------------------//
ESP8266WebServer server(80);
DynamicJsonDocument doc(2048);
SinricProThermostat &myThermostat = SinricPro[THERMOSTAT_ID];

WiFiManager wm;


//---------------------Resetleme değişkenleri----------------------//
bool res; //wifimanager oto bağlantı durumu takibi için
int buttonSayac=0;
bool button=false;
int resetle=0;
//verilerin kaydedilmesi için kontrol değişkeni
//bool shouldSaveConfig = false;
//---------------------------------------------------------------//

//-----------------Termostat değişkenleri----------------------//
float sonTemp = 0;
float sonHum = 0;
float temperature=-100;
int humudity=-1;
String sensor_values;
int roleAcik=0;
bool dhtHata=true;
bool hatacikis=false;
String anaKombi_value;
int anaKombi_live=0;
bool dhtHataMsg=false;
bool anaKombiHataMsg=false;
bool anaKombiHataCikis=false;
//--------------------------------------------------------------//

//----------------SinricPro değişkenleri---------------------//
float targetTemp=-200;
bool globalPowerState=true;
String thermostatMode;

bool modeDegisti = false;
//-----------------------------------------------------------//

//--------------------Zamanlama-----------------------------//
unsigned long lastTime = -50000;
unsigned long timerDelay = 60 *1000;
unsigned long sonTempAlma = 0;
unsigned long sonAnaKombiLive = -120001;
//------------------------------------------------------------//

unsigned long int resetlemeZamani= 43200000; // 1000*60*60*12  ==> 12 saat

//------------------------------------------------------------------------------//

void gunlukReset()
{
  if(millis() > resetlemeZamani)
  {
    resetle=1;
  }
}

//-----------------------------------------------------------------------------//


//ayarların kaydedilmesini hatırlatmak için
/*void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}*/

//-------------------RESET BUTTON-------------------------------------------//
IRAM_ATTR void resetlefonk(){

    if (!button){
    if (digitalRead(BUTTON_PIN)==HIGH){
      buttonSayac=millis();
      button=true;
    }
  }else{
    if (digitalRead(BUTTON_PIN)==LOW){
        button=false;
        if ((millis()-buttonSayac)>=50){
          if ((millis()-buttonSayac)>=3000){
            Serial.println(">3 sn basıldı tümünü resetliyor");
             //wm.resetSettings();
             // ESP.restart();
             resetle=2;
          }
          else
          {
            resetle=1;
            Serial.println("<3 sn esp restart");
          }
        }
    }
  }
}

//--------------------------------------------------------------------------------//

//---------------------EEPROM YAZMA OKUMA FONKİSYONLARI -----------------------------------//

void writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
  byte len = strToWrite.length();
  EEPROM.write(addrOffset, len);
  for (int i = 0; i < len; i++)
  {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }
   EEPROM.commit();
   delay(500);
}

String readStringFromEEPROM(int addrOffset)
{
int newStrLen = EEPROM.read(addrOffset);
char data[newStrLen + 1];
for (int i = 0; i < newStrLen; i++)
{
data[i] = EEPROM.read(addrOffset + 1 + i);
}
data[newStrLen] = '\0';
return String(data);
}

//----------------

void yazEEPROM(String yazilacakOlan,float targetYaz)
{
  Serial.println("saving to eeprom target Temp:");
  String yazilacak=String(targetYaz);
  writeStringToEEPROM(100, yazilacak);
  Serial.print("eproma yazdı:");
  Serial.println(yazilacak);
  //end save

  Serial.println("saving to eeprom thermostat Mode:");
  String yazilacak2=String(yazilacakOlan);
  writeStringToEEPROM(200, yazilacak2);
  Serial.print("eproma yazdı:");
  Serial.println(yazilacak2);
  //end save
}

void okuTargetTemp()
{
  int index=EEPROM.read(100);
  Serial.print("epromdan okunan index :");
  Serial.println(index);

  String token =readStringFromEEPROM(100);
  //token.toCharArray(blynk_token_vana,34);
  targetTemp=token.toFloat();

  Serial.print("epromdan okudu target Temp:");
  Serial.println(token);
  Serial.println(targetTemp);
}

void okuThermostatMode()
{
  int index2=EEPROM.read(200);
  Serial.print("epromdan okunan index :");
  Serial.println(index2);

  String token2 =readStringFromEEPROM(200);
  //token.toCharArray(blynk_token_vana,34);
  thermostatMode=token2;

  Serial.print("epromdan okudu thermostat mode:");
  Serial.println(token2);
  Serial.println(thermostatMode);
}

void okuState()
{
  int index3=EEPROM.read(300);
  Serial.print("epromdan okunan index :");
  Serial.println(index3);

  String token3 =readStringFromEEPROM(300);
  //token.toCharArray(blynk_token_vana,34);
  if (token3 ="1")
  {
    globalPowerState=true;
  }
  else
  {
    globalPowerState=false;
  }

  Serial.print("epromdan okudu termostat state:");
  Serial.println(token3);
  Serial.println(globalPowerState);
}

void okuEEPROM ()
{
  okuTargetTemp();
  okuThermostatMode();
  okuState();
}

void targetTempYazEEPROM()
{
  Serial.println("saving to eeprom target Temp:");
  String yazilacak=String(targetTemp);
  writeStringToEEPROM(100, yazilacak);
  Serial.print("eproma yazdı:");
  Serial.println(yazilacak);
  delay(1000);
  //end save
}

void thermostatStateYazEEPROM()
{
  Serial.println("saving to eeprom thermostat state:");
  String yazilacak=String(globalPowerState);
  writeStringToEEPROM(300, yazilacak);
  Serial.print("eproma yazdı:");
  Serial.println(yazilacak);
  delay(1000);
}

void thermostatModeYazEEPROM()
{
  Serial.println("saving to eeprom thermostat mode:");
  String yazilacak=String(thermostatMode);
  writeStringToEEPROM(200, yazilacak);
  Serial.print("eproma yazdı:");
  Serial.println(yazilacak);
  delay(1000);
  //end save
}

void resetlemeYazEEPROM()
{
  Serial.println("saving to eeprom reseteleme mode:");
  String yazilacak=String(resetle);
  writeStringToEEPROM(400, yazilacak);
  Serial.print("eproma yazdı:");
  Serial.println(yazilacak);
  delay(1000);
  //end save
}

void okuResetlemeEEPROM()
{
  int index2=EEPROM.read(400);
  Serial.print("epromdan okunan index :");
  Serial.println(index2);

  String token2 =readStringFromEEPROM(400);
  //token.toCharArray(blynk_token_vana,34);
  resetle=token2.toInt();

  Serial.print("epromdan okudu Resetleme mode:");
  Serial.println(token2);
  Serial.println(resetle);
}
//----------------------------------------------------------------------------------//

//--------------------------SİNRİCPRO THERMOSTAT FONKSİYONLARI-------------------//

bool onPowerState(const String &deviceId, bool &state) {
  Serial.printf("Thermostat %s turned %s\r\n", deviceId.c_str(), state?"on":"off");
  globalPowerState = state;

  if(globalPowerState == true)
  {
    myThermostat.sendThermostatModeEvent(thermostatMode,"Acildi");
  }
  else
  {
    thermostatMode="off";
    myThermostat.sendThermostatModeEvent("off","Kapandi");
  }

  thermostatStateYazEEPROM();
  return true; // request handled properly
}

bool onTargetTemperature(const String &deviceId, float &temperature) {
  Serial.printf("Thermostat %s set temperature to %f\r\n", deviceId.c_str(), temperature);
  targetTemp = temperature;
  delay(100);
  //myThermostat.sendTargetTemperatureEvent(targetTemp);

  targetTempYazEEPROM();
  return true;
}

bool onAdjustTargetTemperature(const String & deviceId, float &temperatureDelta) {
  targetTemp += temperatureDelta;  // calculate absolut temperature
  Serial.printf("Thermostat %s changed temperature about %f to %f", deviceId.c_str(), temperatureDelta, targetTemp);
  temperatureDelta = targetTemp; // return absolut temperature
  return true;
}

bool onThermostatMode(const String &deviceId, String &mode) {
  Serial.printf("Thermostat %s set to mode %s\r\n", deviceId.c_str(), mode.c_str());
  thermostatMode = mode;
  delay(100);

  thermostatModeYazEEPROM();

  if(globalPowerState == false)
  {
    globalPowerState=true;
    myThermostat.sendPowerStateEvent(globalPowerState,"Mode");
  }
  modeDegisti = true;
  return true;
}

void setupSinricPro() {
  myThermostat.onPowerState(onPowerState);
  myThermostat.onTargetTemperature(onTargetTemperature);
  myThermostat.onAdjustTargetTemperature(onAdjustTargetTemperature);
  myThermostat.onThermostatMode(onThermostatMode);

  // setup SinricPro
  SinricPro.onConnected([](){ Serial.printf("Connected to SinricPro\r\n"); });
  SinricPro.onDisconnected([](){ Serial.printf("Disconnected from SinricPro\r\n"); });
  SinricPro.begin(APP_KEY, APP_SECRET);
}

//----------------------------------------------------------------------------------------//

//======================THERMOSTAT ANA MANTIK FONKSİYONLARI================================//

void updateTemperature()
{
  if ((sonTemp != temperature || sonHum != humudity) && (millis()-lastTime > timerDelay))
  {
    delay(500);
    myThermostat.sendTemperatureEvent(temperature,humudity);
    Serial.println("Sinric Prp Sıcaklık Nem Gönderildi...");
    sonTemp=temperature;
    sonHum=humudity;
    lastTime=millis();
  }
}

void dhtHataYakala()
{
  if(millis()-sonTempAlma > 120000 || temperature == -100)
  {
    temperature=-100;
    humudity=0;
    dhtHata=true;
  }
  else
  {
    dhtHata=false;
  }
}

void dhtHataKontrol()
{
  if(dhtHata==true)  
  {
    if(thermostatMode != "off")
    {
      myThermostat.sendTemperatureEvent(temperature,humudity);
      Serial.println("Sinric Prp Sıcaklık Nem Gönderildi...");
      sonTemp=temperature;
      sonHum=humudity;
      lastTime=millis();
      delay(500);
      thermostatMode="off";
      if(SinricPro.isConnected())
      {
        myThermostat.sendThermostatModeEvent(thermostatMode,"DHT HATA");
        dhtHataMsg=true;
      }
      Serial.println("Thermostat mode: off ==> DHT Hatası MODE");
    }

    if(dhtHataMsg==false)
    {
      if(SinricPro.isConnected())
      {
        myThermostat.sendThermostatModeEvent(thermostatMode,"DHT HATA");
        dhtHataMsg=true;
      }
    }

    hatacikis=true;
  }

  /*if(modeDegisti==true && dhtHata==true)
  {
    thermostatMode="off";
    if(SinricPro.isConnected())
    {
      myThermostat.sendThermostatModeEvent(thermostatMode,"DHT HATA");
    }
    modeDegisti=false;
    Serial.println("Thermostat mode: off ==> DHT Hatası Degisiklik Yapılamaz!!");
  }*/

  if (hatacikis==true && dhtHata==false)
  {
    myThermostat.sendTemperatureEvent(temperature,humudity);
    Serial.println("Sinric Prp Sıcaklık Nem Gönderildi...");
    sonTemp=temperature;
    sonHum=humudity;
    lastTime=millis();
    delay(500);
    okuThermostatMode();
    if(SinricPro.isConnected())
    {
      myThermostat.sendThermostatModeEvent(thermostatMode,"DHT DÜZELDİ");
    }
    hatacikis=false;
    dhtHataMsg=false;
    Serial.println("Thermostat mode: "+thermostatMode+" ==> DHT Hatası Giderildi");
  }
}

bool anaKombiLive()
{
  if(millis()-sonAnaKombiLive > 120000)
  {
    //delay(2000);
    //Serial.println("Ana Kombi Kapalı !!!");
    anaKombi_live=0;
    return false;  
  }
  else
  {
    return true;
  }
}

void anaKombiHataKontrol()
{
  if(anaKombi_live==0 )
  {
    if(thermostatMode != "off")
    {
      thermostatMode="off";
      if(SinricPro.isConnected())
      {
        myThermostat.sendThermostatModeEvent(thermostatMode,"ANA KOMBİ HATA");
        anaKombiHataMsg=true;
      }
      Serial.println("Thermostat mode: off ==> Ana Kombi Hatası");
    }

    if(anaKombiHataMsg == false)
    {
      if(SinricPro.isConnected())
      {
        myThermostat.sendThermostatModeEvent(thermostatMode,"ANA KOMBİ HATA");
        anaKombiHataMsg=true;
        Serial.println("Thermostat mode: off ==> Ana Kombi Hatası");
      }
    }

    anaKombiHataCikis=true;
  }

  /*if(modeDegisti==true && anaKombi_live == 0)
  {
    thermostatMode="off";
    if(SinricPro.isConnected())
    {
      myThermostat.sendThermostatModeEvent(thermostatMode,"Ana Kombi HATA");
    }
    modeDegisti=false;
    Serial.println("Thermostat mode: off ==> Ana Kombi Hatası Degisiklik Yapılamaz!!");
  }*/

  if (anaKombiHataCikis==true && anaKombi_live == 1)
  {
    myThermostat.sendTemperatureEvent(temperature,humudity);
    Serial.println("Sinric Prp Sıcaklık Nem Gönderildi...");
    sonTemp=temperature;
    sonHum=humudity;
    lastTime=millis();
    delay(500);
    okuThermostatMode();
    if(SinricPro.isConnected())
    {
      myThermostat.sendThermostatModeEvent(thermostatMode,"Ana Kombi DÜZELDİ");
    }
    anaKombiHataCikis=false;
    anaKombiHataMsg=false;
    Serial.println("Thermostat mode: "+thermostatMode+" ==> AnaKombi Hatası Giderildi");
    delay(5000);
  }
}

//=========================================================================================//

//---------------------------Server Fonksiyonları-----------------------------------------//

void durumGonder()
{
  server.send(200,"text/plain", String(thermostatMode));
  Serial.println("Termostat Modu Ana Kombiye Gonderildi");
}

void anaKombi()
{
  if (server.hasArg("anaKombi"))
  {
    anaKombi_value = server.arg("anaKombi");
    Serial.println(anaKombi_value);
  }
  deserializeJson(doc, anaKombi_value);

    anaKombi_live         = doc["anaKombi_live"].as<int>();


  Serial.println(anaKombi_live);

  server.send(200, "text/html", "Data received");
  sonAnaKombiLive=millis();
}

void tempHum()
{
  if (server.hasArg("sensor_reading"))
  {
    sensor_values = server.arg("sensor_reading");
    Serial.println(sensor_values);
  }
  deserializeJson(doc, sensor_values);

    temperature          = doc["temp_reading"].as<float>();
    humudity             = doc["hum_reading"].as<int>();


  Serial.println(temperature);
  Serial.println(humudity);

  server.send(200, "text/html", "Data received");

  sonTempAlma=millis();
}
//----------------------------------------------------------------------------------------//

//-------------------------------HOMEKİT----------------------------------------------//

float homeKitTargetTemp;
String homeKitMode;

void homeKitTargetTempAl()
{
  if (server.hasArg("homeKit_reading"))
  {
    sensor_values = server.arg("homeKit_reading");
    Serial.println(sensor_values);
  }
  deserializeJson(doc, sensor_values);

    homeKitTargetTemp          = doc["targetTemp_reading"].as<float>();


  Serial.println(homeKitTargetTemp);

  server.send(200, "text/html", "Data received");

  if(dhtHata==false && anaKombi_live == 1 && targetTemp != homeKitTargetTemp)
  {
    targetTemp=homeKitTargetTemp;
    myThermostat.sendTargetTemperatureEvent(targetTemp,"HomeKit");
    targetTempYazEEPROM();
    Serial.println("Target Temp Değişti Home kit");
  }
  /*else
  {
    Serial.println("Target Temp Değiştirilemedi Hata Var (Home Kit) !!!");
  }*/
}

void homeKitModeAl()
{
  if (server.hasArg("homeKitState_reading"))
  {
    sensor_values = server.arg("homeKitState_reading");
    Serial.println(sensor_values);
  }
  deserializeJson(doc, sensor_values);

    homeKitMode          = doc["state_reading"].as<String>();


  Serial.println(homeKitMode);

  server.send(200, "text/html", "Data received");

  if(dhtHata == false && anaKombi_live == 1 && thermostatMode != homeKitMode)
  {
    if(homeKitMode != "null")
    {
      thermostatMode=homeKitMode;
      myThermostat.sendThermostatModeEvent(thermostatMode,"HoemKit");
      thermostatModeYazEEPROM();
      Serial.println("Thermostat Mode Değişti Home Kit");
    }
  }
  /*else
  {
    Serial.println("Thermostat Mode Değiştirilemedi Hata Var (Home Kit) !!!");
  }*/
}

void homeKitTargetTempGonder()
{
  server.send(200,"text/plain", String(targetTemp));
  Serial.println("Target Temp HomeKite Gonderildi");
}

void homeKitModeGonder()
{
  server.send(200,"text/plain", String(thermostatMode));
  Serial.println("Thermostat Mode HomeKite Gonderildi");
}

void homeKitRoleGonder()
{
  server.send(200,"text/plain", String(roleAcik));
  Serial.println("Role Durumu HomeKite Gonderildi");
}

void homeKitHataGonder()
{
  if(dhtHata == true || anaKombi_live == 0)
  {
    server.send(200,"text/plain", String(1));
    Serial.println("Hata Değişkeni HomeKite Gonderildi");
  }
  if(dhtHata == false && anaKombi_live == 1)
  {
    server.send(200,"text/plain", String(0));
    Serial.println("Hata Değişkeni HomeKite Gonderildi");
  }
}

void homeKitReset()
{
  if (server.hasArg("homeKitReset_reading"))
  {
    sensor_values = server.arg("homeKitReset_reading");
    Serial.println(sensor_values);
  }
  deserializeJson(doc, sensor_values);

    resetle         = doc["reset_reading"].as<int>();


  Serial.println(resetle);

  server.send(200, "text/html", "Data received");
}

//------------------------------------------------------------------------------------//


//--------------------------SETUP LOOP----------------------------------------------//

void setup() {
  EEPROM.begin(512);
  Serial.begin(115200);

  pinMode(BUTTON_PIN,INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN,HIGH);

  delay(10);
  okuResetlemeEEPROM();
  if(resetle==2)
  {
    wm.resetSettings();
    resetle=0;
    resetlemeYazEEPROM();
    delay(500);
  }
  else
  {
    resetle=0;
  }

  delay(100);

  wm.setConfigPortalTimeout(600); // 10 dk

  res = wm.autoConnect(wifiAd, "12345678"); // Wifimanager bağlanma satırı. Ağ adı olarak görünmesini istediğimiz
  // ismi ve belirleyeceğimiz şifreyi tanımladık. İstersek şifre de girmeyebiliriz. Korumasız ağ olarak görünür.

  if (!res) {
    Serial.println("Bağlantı Sağlanamadı");
    // ESP.restart();
  }
  else {
    Serial.println("Ağ Bağlantısı Kuruldu");
  }
  WiFi.config(staIP,gateway,apSubnet,dns1,dns2);
  WiFi.mode(WIFI_STA);
  
  Serial.println();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
  //delay(500);
  //WiFi.printDiag(Serial);
  //yazEEPROM("heat",20);
  delay(500);
  okuEEPROM();
  delay(500);
  setupSinricPro();
  delay(100);

  server.on(serverName, HTTP_GET, durumGonder);
  server.on("/data/", HTTP_GET, tempHum);
  server.on("/anaKombi/", HTTP_GET, anaKombi);
  server.on("/homeKit_targetTemp/", HTTP_GET, homeKitTargetTempAl);
  server.on("/homeKit_state/", HTTP_GET, homeKitModeAl);
  server.on("/targetTemp", HTTP_GET, homeKitTargetTempGonder);
  server.on("/state", HTTP_GET, homeKitModeGonder);
  server.on("/role", HTTP_GET, homeKitRoleGonder);
  server.on("/hata", HTTP_GET, homeKitHataGonder);
  server.on("/homeKit_reset/", HTTP_GET, homeKitReset);
  server.begin();

  delay(500);

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), resetlefonk, CHANGE);
}


void loop() {

  if (WiFi.status() != WL_CONNECTED)
  {
    ESP.restart();
  }
   
  server.handleClient();
  SinricPro.handle();           //sinric pro ana kombi kapanırsa kapatılabilir belki düşün!!

  anaKombiHataKontrol();
  updateTemperature();//Google Home sıcaklık nem gönder

  if(anaKombiLive()==true)
  {
    dhtHataYakala();
    dhtHataKontrol();
    //-------------Role Aç Kapat-------------------------//

    if ((thermostatMode == "HEAT" || thermostatMode == "heat") && (globalPowerState==true))
    {
      if (targetTemp+0.2 > sonTemp)
      {
        digitalWrite(RELAY_PIN,LOW);
        //Serial.println("Role Acik");
        roleAcik=1;
      }
      else if (targetTemp-0.2 < sonTemp)
      {
        digitalWrite(RELAY_PIN,HIGH);
        //Serial.println("Role Kapali");
        roleAcik=0;
      }
    }
    else
    {
      digitalWrite(RELAY_PIN,HIGH);
      //Serial.println("Role Kapali mod dan dolayı");
      roleAcik=0;
    }
  }
  //---------resetleme----------------//

  gunlukReset();

  if(resetle == 1)
  {
    ESP.restart();
    resetle=0;
  }

  else if (resetle == 2)
  {
    delay(500);
    resetlemeYazEEPROM();
    delay(500);
    ESP.restart();
    resetle=0;
  }
}
