#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "DHTesp.h"
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <arduino_homekit_server.h>
#include <EEPROM.h>

#define DHT_PIN D2
#define BUTTON_PIN D7
DHTesp dht;
WiFiManager wm;

IPAddress staIP(192, 168, 1, 145);
IPAddress gateway(192, 168, 1, 1);
IPAddress apSubnet(255, 255, 0, 0);

 // Use WiFiClient class to create TCP connections
  
  const char * host = "192.168.1.141";         
  const int httpPort = 80;
  const char* wifiAd="Daire_4_Sensor_1";

//---------------------------------------------------//
const char* targetTempServer="http://192.168.1.141/targetTemp";
const char* stateServer="http://192.168.1.141/state";
const char* hataServer="http://192.168.1.141/hata";
const char* roleServer="http://192.168.1.141/role";

//--------------------------------------------//
float temperature;
float humidity;
float alinanTargetTemp=0;
String alinanThermostatMode;
int alinanRole=0;
unsigned long int sonSenkron=-30000;
//------------------------------------------------//
unsigned long int sonGonderme=-30000;
int hata=0;
//---------------------Resetleme değişkenleri----------------------//
bool res; //wifimanager oto bağlantı durumu takibi için
int buttonSayac=0;
bool button=false;
int resetle=0;
//verilerin kaydedilmesi için kontrol değişkeni
bool shouldSaveConfig = false;
//---------------------------------------------------------------//

///////////Home kit değişkenler//////////////////
extern "C" homekit_server_config_t config;
extern "C" homekit_characteristic_t current_temperature;
extern "C" homekit_characteristic_t target_temperature;
extern "C" homekit_characteristic_t target_state;
extern "C" homekit_characteristic_t current_state;
extern "C" homekit_characteristic_t current_humidity;
unsigned long int homeKitTempGonderme=0;
uint8 state;
//---------------------------------------------------------------------------//

unsigned long int resetlemeZamani= 43200000; // 1000*60*60*12  ==> 12 saat

//------------------------------------------------------------------------------//

void gunlukReset()
{
  if(millis() > resetlemeZamani)
  {
    resetle=1;
  }
}

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

void resetlemeYazEEPROM()
{
  Serial.println("saving to eeprom reseteleme mode:");
  String yazilacak=String(resetle);
  writeStringToEEPROM(100, yazilacak);
  Serial.print("eproma yazdı:");
  Serial.println(yazilacak);
  delay(1000);
  //end save
}

void okuResetlemeEEPROM()
{
  int index2=EEPROM.read(100);
  Serial.print("epromdan okunan index :");
  Serial.println(index2);

  String token2 =readStringFromEEPROM(100);
  //token.toCharArray(blynk_token_vana,34);
  resetle=token2.toInt();

  Serial.print("epromdan okudu Resetleme mode:");
  Serial.println(token2);
  Serial.println(resetle);
}
//----------------------------------------------------------------------------------//

//-----------------------------------------------------------------------------//

String httpGETRequest(String serverName) {
  WiFiClient client;
  HTTPClient http;
  // Your IP address with path or Domain name with URL path
  http.begin(client, serverName);

  //Serial.println(serverName);
  // Send HTTP POST request
  int httpResponseCode = http.GET();

  String payload = "--";

  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
    //almaHataSayac=0;
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    hata=2;
    //almaHataSayac++;
  }
  // Free resources
  http.end();

  return payload;
  }

//--------------------------------------------------------------------------//


//------------------Home Kit---------------------------------------//

void homeKitTempHumGonder()
{
  if(millis()-homeKitTempGonderme > 60000 && (current_temperature.value.float_value != temperature || current_humidity.value.float_value != humidity))
  {
    current_temperature.value.float_value = temperature;
    homekit_characteristic_notify(&current_temperature, current_temperature.value);
    current_humidity.value.float_value = humidity;
    homekit_characteristic_notify(&current_humidity, current_humidity.value);

    homeKitTempGonderme=millis();
  }
}

int alinanCurrentMode()
{
  if(alinanThermostatMode=="off" || alinanThermostatMode == "OFF")
  {
    return 0;
  }
  else if(alinanThermostatMode == "heat" || alinanThermostatMode == "HEAT")
  {
    return 1;
  }
  else if(alinanThermostatMode == "cool" || alinanThermostatMode == "COOL")
  {
   return 2; 
  }
  else
  {
    return 0;
  }
}

void homeKitTargetTempStateGonder()
{
  if(hata==0)
  {
    if(target_temperature.value.float_value != alinanTargetTemp)
    {
      target_temperature.value.float_value=alinanTargetTemp;
      homekit_characteristic_notify(&target_temperature, target_temperature.value);
      Serial.println("HomeKit Target Temp Güncellendi...");
    }
    if(target_state.value.int_value != alinanCurrentMode())
    {
      target_state.value.int_value = alinanCurrentMode();
      homekit_characteristic_notify(&target_state, target_state.value);
      Serial.println("HomeKit Thermostat Mode Güncellendi...");
    }
    if(alinanRole != current_state.value.int_value)
    {
      current_state.value.int_value = alinanRole; //homekit anlık açık kapalı durumu değiştirme
      homekit_characteristic_notify(&current_state, current_state.value);
      Serial.println("HomeKit Role Durumu Güncellendi...");
    }
  }
  else
  {
    target_state.value.int_value = 0; //homekit anlık açık kapalı durumu değiştirme
    homekit_characteristic_notify(&target_state, target_state.value);
    current_state.value.int_value = 0; //homekit anlık açık kapalı durumu değiştirme
    homekit_characteristic_notify(&current_state, current_state.value);
    Serial.println("HomeKit Güncellendi... Hata Modu");
  }
}

void targetTempStateAl()
{
  if(millis()-sonSenkron > 30000)
  {
    alinanTargetTemp=httpGETRequest(targetTempServer).toFloat();
    Serial.print("Target Temp: ");
    Serial.println(alinanTargetTemp);
    delay(500);
    alinanThermostatMode=httpGETRequest(stateServer);
    Serial.print("Thermostat Mode: ");
    Serial.println(alinanThermostatMode);
    delay(500);
    alinanRole=httpGETRequest(roleServer).toInt();
    Serial.print("Role Durumu: ");
    Serial.println(alinanRole);
    delay(500);
    hata=httpGETRequest(hataServer).toInt();
    Serial.print("Hata : ");
    Serial.println(hata);
    delay(500);

    homeKitTargetTempStateGonder();

    sonSenkron=millis();
  }
}

//----------------------------------------------------------------//

//----------------------------------------------------------------------//
//ayarların kaydedilmesini hatırlatmak için
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


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

void sicaklikNemOku()
{
  temperature = dht.getTemperature();
  //veri okunamadıysa
  if (isnan(temperature)){
    Serial.println("DHT sensör sıcaklık verisi okunamadı!!!");
    temperature = -100;
    hata=3;
  }
  /*Serial.print("Okunan Sıcaklık:");
  Serial.println(temperature);*/


  humidity = dht.getHumidity();

  //veri okunamadıysa
  if (isnan(humidity)){
    Serial.println("DHT sensör nem verisi okunamadı!!!");
    humidity = 0;
  }

  /*Serial.print("Okunan Nem:");
  Serial.println(humidity);*/

  delay(1000);
}

void resetGonder()
{
  WiFiClient client;

  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }

  String url = "/homeKit_reset/";
  url += "?homeKitReset_reading=";
  url +=  "{\"reset_reading\":\"reset_value\"}";

  url.replace("reset_value", String(resetle));


  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
              "Host: " + host + "\r\n" +
              "Connection: close\r\n\r\n");
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return;
    }
  }
}

void tempHumGonder()
{
  WiFiClient client;

  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }

  if(millis()-sonGonderme > 30000)
  {
    Serial.print("Home Kit Bağlantı Sayısı: ");
    Serial.println(arduino_homekit_connected_clients_count());
    // We now create a URI for the request. Something like /data/?sensor_reading=123
    String url = "/data/";
    url += "?sensor_reading=";
    url +=  "{\"temp_reading\":\"temp_value\",\"hum_reading\":\"hum_value\"}";

    url.replace("temp_value", String(temperature));
    url.replace("hum_value", String(humidity));


    // This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                "Host: " + host + "\r\n" +
                "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println(">>> Client Timeout !");
        client.stop();
        return;
      }
    }
    sonGonderme=millis();
  }
}

void senkronTempGonder()
{
  if(target_temperature.value.float_value != alinanTargetTemp)
  {
    WiFiClient client;

    if (!client.connect(host, httpPort)) {
      Serial.println("connection failed");
      return;
    }

    String url = "/homeKit_targetTemp/";
    url += "?homeKit_reading=";
    url +=  "{\"targetTemp_reading\":\"targetTemp_value\"}";

    url.replace("targetTemp_value", String(target_temperature.value.float_value));


    // This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                "Host: " + host + "\r\n" +
                "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println(">>> Client Timeout !");
        client.stop();
        return;
      }
    }
  }
}


void senkronStateGonder()
{
  if(target_state.value.int_value != alinanCurrentMode())
  {
    WiFiClient client;

    if (!client.connect(host, httpPort)) {
      Serial.println("connection failed");
      return;
    }

    String url = "/homeKit_state/";
    url += "?homeKitState_reading=";
    url +=  "{\"state_reading\":\"state_value\"}";

    if(target_state.value.int_value==0)
    {
      url.replace("state_value", String("off"));
    }

    else if(target_state.value.int_value==1)
    {
      url.replace("state_value", String("heat"));
    }

    else if(target_state.value.int_value==2)
    {
      url.replace("state_value", String("cool"));
    }

    else
    {
      url.replace("state_value", String("off"));
    }
    


    // This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                "Host: " + host + "\r\n" +
                "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println(">>> Client Timeout !");
        client.stop();
        return;
      }
    }
  }
}

//----------------------------------------------------------------------------//
void setup() {
  EEPROM.begin(512);
  Serial.begin(115200);
  pinMode(BUTTON_PIN,INPUT_PULLUP);
  delay(10);
  dht.setup(DHT_PIN,DHTesp::DHT22);

  delay(10);
  okuResetlemeEEPROM();
  if(resetle==2)
  {
    homekit_server_reset(); //Homekite eklemede problem varsa aç home kite yükle yorum satırına al tekrar yükle
    delay(500);
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

  // set the ESP8266 to be a WiFi-client
  WiFi.config(staIP,gateway,apSubnet);
  WiFi.mode(WIFI_STA);

  Serial.println();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());

  //homekit_server_reset(); //Homekite eklemede problem varsa aç home kite yükle yorum satırına al tekrar yükle
  delay(500);
  
  arduino_homekit_setup(&config); // homekit bağlantısı ve cihaz bilgilerini gönderme
  delay(500);

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), resetlefonk, CHANGE);
}



void loop() {

  if (WiFi.status() != WL_CONNECTED)
  {
    ESP.restart();
  }

  sicaklikNemOku();
  arduino_homekit_loop(); // homekit için loop fonksiyonu

  tempHumGonder();
  //arduino_homekit_get_running_server();
  if(arduino_homekit_connected_clients_count() > 0)
  {
    targetTempStateAl();

    homeKitTempHumGonder(); 

    senkronTempGonder();
    senkronStateGonder();
  }

  gunlukReset();

  if(resetle == 1)
  {
    resetGonder();
    delay(1000);
    ESP.restart();
    resetle=0;
  }
  
  if (resetle == 2)
  {
    resetlemeYazEEPROM();
    delay(500);
    resetGonder();
    delay(1000);
    ESP.restart();
    resetle=0;
  }
}
