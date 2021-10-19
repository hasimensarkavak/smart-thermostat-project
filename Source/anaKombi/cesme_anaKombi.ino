#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFiMulti.h>
#include <EEPROM.h>


IPAddress local_IP(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress dns1(8,8,8,8);
IPAddress dns2(8,8,4,4);

WiFiManager wm;

#define RELAY2_PIN D0
#define RELAY_PIN D2
#define BUTTON_PIN D7

//----------------------Telegram-----------------------------------//
#define BOTtoken "**********"  // BotFather'dan aldığınız TOKEN
#define CHAT_ID "**********"   //// IDBOT üzerinden /getid mesajı ile aldığımız ID bilgimizi gireceğiz
#define CHAT_ID_M "*********"
WiFiClientSecure clientTelegram;
UniversalTelegramBot bot(BOTtoken, clientTelegram);
//-----------------------------------------------------------------//
int botRequestDelay = 500;
unsigned long lastTimeBotRan;

//---------------------Resetleme değişkenleri----------------------//
bool res; //wifimanager oto bağlantı durumu takibi için
int buttonSayac=0;
bool button=false;
int resetle=0;
//---------------------------------------------------------------//
bool kapat = false;

bool anaKombiRole = true;

bool anaKombilive=true;

unsigned long int resetlemeZamani= 43200000; // 1000*60*60*12  ==> 12 saat

//------------------------------------------------------------------------------//

void gunlukReset()
{
  if(millis() > resetlemeZamani)
  {
    resetle=1;
  }
}

void resetKontrol()
{
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

//-----------------------------------------------------------------------------//

//-----------------------Daire Live Kontrolleri--------------------------------//

String DisableMessage(String daireNo,String roleNo)
{
  String off_symbol="❌";
  return "Daire "+daireNo+" Röle "+roleNo+" Bağlantı SORUNU !!"+off_symbol;
}
String EnabledMessage(String daireNo,String roleNo)
{
  String on_symbol="✅ ";
  return "Daire "+daireNo+" Röle "+roleNo+" Bağlantı DÜZELDİ !!"+on_symbol;
}

//------------------------------------------------------------------------------//

//---------------------Strucklar Röleler-------------------------//

struct role
{
  int roleNo=0;
  int daireNo=0;
  String durum="heat";
  int hataSayac=0;
  String hata="✅";
  unsigned long int sonRoleTime= -30000;
  unsigned long int sonHataTime= 0;

  //-----------------------İletisim----------------------//

  WiFiClient client;
  HTTPClient http;
  String serverName;

  String httpGETRequest() {
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
    hataSayac=0;
    hata="✅";
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    hataSayac++;
    hata="❌";
  }
  // Free resources
  http.end();

  return payload;
  }

  WiFiClient client2;
  String host;
  int httpPort=80;

  void kombiDurumGonder()
  {
    if (!client2.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
    }
    String url = "/anaKombi/";
    url += "?anaKombi=";
    url +=  "{\"anaKombi_live\":\"anaKombi_value\"}";

    url.replace("anaKombi_value", String(anaKombilive));


    // This will send the request to the server
    client2.print(String("GET ") + url + " HTTP/1.1\r\n" +
                "Host: " + host + "\r\n" +
                "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client2.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println(">>> client2 Timeout !");
        client2.stop();
        return;
      }
    }
    Serial.print("Daire No: ");
    Serial.print(daireNo);
    Serial.print(" Röle No: ");
    Serial.print(roleNo);
    Serial.println(" Kombi Durum Gonderildi");
  }

  void roleIletisim()
  {
    if(millis()-sonRoleTime > 30000)
    {
      durum=httpGETRequest();
      Serial.print("Daire No: ");
      Serial.print(daireNo);
      Serial.print(" Röle No: ");
      Serial.print(roleNo);
      Serial.print(" Durum: ");
      Serial.println(durum);
      kombiDurumGonder();
      sonRoleTime=millis();
    }
  }
  //----------------------------------------------------------//

  //------------------------Hata Kontrol---------------------//

  bool hataCikis=false;
  int msgSayac=0;
  void hataTakip()
  {
    if (hataSayac >= 4 && millis()-sonHataTime > 120000) //4*30sn
    {
      durum="off";
      hataSayac=5;
      if(msgSayac < 3)
      {
        bot.sendMessage(CHAT_ID, DisableMessage(String(daireNo),String(roleNo)), "");
        bot.sendMessage(CHAT_ID_M, DisableMessage(String(daireNo),String(roleNo)), "");
        Serial.println(DisableMessage(String(daireNo),String(roleNo)));
        msgSayac++;
      }
      hataCikis=true;
      sonHataTime=millis();
    }
    else if(hataCikis==true && hataSayac==0)
    {
      msgSayac=0;
      bot.sendMessage(CHAT_ID, EnabledMessage(String(daireNo),String(roleNo)), "");
      bot.sendMessage(CHAT_ID_M, EnabledMessage(String(daireNo),String(roleNo)), "");
      Serial.println(EnabledMessage(String(daireNo),String(roleNo)));
      hataCikis=false;
    }
  }
};

struct daire
{
  role role_1 ;
};

struct daire2 : daire
{
  role role_2;
};

struct daire4 : daire2
{
  role role_3;
};

daire daire_1;
daire2 daire_2;
daire2 daire_3;
daire4 daire_4;

//---------------------------------------------------------------//


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

//-----------------------------Telegram Check------------------------------------------//

void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i=0; i<numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;

    String from_name = bot.messages[i].from_name;
    if (from_name == "") from_name = "Guest";

    if (text == "/check") 
    {

      if(kapat==true)
      {
        bot.sendMessage(chat_id, "Sistem Durumu ==> KAPALI ❌", "");
      }
      else
      {
        bot.sendMessage(chat_id, "Sistem Durumu ==> AÇIK   ✅", "");
      }

      bot.sendMessage(chat_id, "Daire 1 Röle 1 Modu ==> "+daire_1.role_1.durum +"\nDurumu ==> "+daire_1.role_1.hata+"\n"+
      "Daire 2 Röle 1 Modu ==> "+daire_2.role_1.durum +"\nDurumu ==> "+daire_2.role_1.hata+"\n"+
      "Daire 2 Röle 2 Modu ==> "+daire_2.role_2.durum +"\nDurumu ==> "+daire_2.role_2.hata+"\n"+
      "Daire 3 Röle 1 Modu ==> "+daire_3.role_1.durum +"\nDurumu ==> "+daire_3.role_1.hata+"\n"+
      "Daire 3 Röle 2 Modu ==> "+daire_3.role_2.durum +"\nDurumu ==> "+daire_3.role_2.hata+"\n"+
      "Daire 4 Röle 1 Modu ==> "+daire_4.role_1.durum +"\nDurumu ==> "+daire_4.role_1.hata+"\n"+
      "Daire 4 Röle 2 Modu ==> "+daire_4.role_2.durum +"\nDurumu ==> "+daire_4.role_2.hata+"\n"+
      "Daire 4 Röle 3 Modu ==> "+daire_4.role_3.durum +"\nDurumu ==> "+daire_4.role_3.hata+"\n"+
      "Ana Kombi Röle Durumu ==> "+anaKombiRole, "");
    }

    if (text == "/close")
    {
      kapat=true;
      bot.sendMessage(chat_id, "Ana Kombi KAPATILDI !!!  ❌", "");
    }

    if (text == "/open")
    {
      kapat = false;
      bot.sendMessage(chat_id, "Ana Kombi AÇILDI  ✅", "");
    }

    if (text == "/start") {
      String welcome = "Çeşme Bozkurt Konağı, " + from_name + ".\n";
      welcome += "/check : Tüm Rölelerin Durumunu Getirir\n";
      welcome += "/close : Sistemi Kapatır\n";
      welcome += "/open : Sistemi Açar\n";
      bot.sendMessage(chat_id, welcome, "Markdown");
    }
  }
}

void telegramMsgKontrol()
{
  if (millis() > lastTimeBotRan + botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while(numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    lastTimeBotRan = millis();
  }
}

//----------------------------------------------------------------------------------//

//---------------------------Röle Tanımlama------------------------------------//

String serverNameTanimla(String daireNo,String roleNo)
{
  //Serial.println("http://192.168.1.1"+daireNo+roleNo+"/durum/daire"+daireNo+"/role"+roleNo);
  return "http://192.168.1.1"+daireNo+roleNo+"/durum/daire"+daireNo+"/role"+roleNo;
}

String hostTanimla(String daireNo,String roleNo)
{
  //Serial.println("192.168.1.1"+daireNo+roleNo);
  return "192.168.1.1"+daireNo+roleNo;
}

void rolelerTanimla()
{
  daire_1.role_1.daireNo=1;
  daire_1.role_1.roleNo=1;
  daire_1.role_1.serverName=serverNameTanimla("1","1");
  daire_1.role_1.host=hostTanimla("1","1");

  daire_2.role_1.daireNo=2;
  daire_2.role_1.roleNo=1;
  daire_2.role_1.serverName=serverNameTanimla("2","1");
  daire_2.role_1.host=hostTanimla("2","1");

  daire_2.role_2.daireNo=2;
  daire_2.role_2.roleNo=2;
  daire_2.role_2.serverName=serverNameTanimla("2","2");
  daire_2.role_2.host=hostTanimla("2","2");

  daire_3.role_1.daireNo=3;
  daire_3.role_1.roleNo=1;
  daire_3.role_1.serverName=serverNameTanimla("3","1");
  daire_3.role_1.host=hostTanimla("3","1");

  daire_3.role_2.daireNo=3;
  daire_3.role_2.roleNo=2;
  daire_3.role_2.serverName=serverNameTanimla("3","2");
  daire_3.role_2.host=hostTanimla("3","2");

  daire_4.role_1.daireNo=4;
  daire_4.role_1.roleNo=1;
  daire_4.role_1.serverName=serverNameTanimla("4","1");
  daire_4.role_1.host=hostTanimla("4","1");

  daire_4.role_2.daireNo=4;
  daire_4.role_2.roleNo=2;
  daire_4.role_2.serverName=serverNameTanimla("4","2");
  daire_4.role_2.host=hostTanimla("4","2");

  daire_4.role_3.daireNo=4;
  daire_4.role_3.roleNo=3;
  daire_4.role_3.serverName=serverNameTanimla("4","3");
  daire_4.role_3.host=hostTanimla("4","3");
}



//----------------------------------------------------------------------------//

//----------------------HTTP Request--------------------------------------------//

void roleDurumlariAl()
{
  
  daire_1.role_1.roleIletisim();
  daire_1.role_1.hataTakip();

  telegramMsgKontrol();
  resetKontrol();

  daire_2.role_1.roleIletisim();
  daire_2.role_1.hataTakip();

  telegramMsgKontrol();
  resetKontrol();

  daire_2.role_2.roleIletisim();
  daire_2.role_2.hataTakip();

  telegramMsgKontrol();
  resetKontrol();
  
  daire_3.role_1.roleIletisim();
  daire_3.role_1.hataTakip();

  telegramMsgKontrol();
  resetKontrol();
  
  daire_3.role_2.roleIletisim();
  daire_3.role_2.hataTakip();

  telegramMsgKontrol();
  resetKontrol();
  
  daire_4.role_1.roleIletisim();
  daire_4.role_1.hataTakip();

  telegramMsgKontrol();
  resetKontrol();
  
  daire_4.role_2.roleIletisim();
  daire_4.role_2.hataTakip();

  telegramMsgKontrol();
  resetKontrol();
  
  daire_4.role_3.roleIletisim();
  daire_4.role_3.hataTakip();

  telegramMsgKontrol();
  resetKontrol();
}

//-----------------------------------------------------------------------------//

//---------------------------SETUP LOOP--------------------------------------//

void setup() {
  EEPROM.begin(512);
  Serial.begin(115200);

  pinMode(BUTTON_PIN,INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY_PIN,LOW);
  digitalWrite(RELAY2_PIN,LOW);
  rolelerTanimla();

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

  wm.setConfigPortalTimeout(300); // 10 dk

  res = wm.autoConnect("AnaKombi", "12345678"); // Wifimanager bağlanma satırı. Ağ adı olarak görünmesini istediğimiz
  // ismi ve belirleyeceğimiz şifreyi tanımladık. İstersek şifre de girmeyebiliriz. Korumasız ağ olarak görünür.
  
  if (!res) {
    Serial.println("Bağlantı Sağlanamadı");
    // ESP.restart();
  }
  else {
    Serial.println("Ağ Bağlantısı Kuruldu");
  }

  WiFi.config(local_IP,gateway,subnet,dns1,dns2);
  WiFi.mode(WIFI_STA);

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Sinyal Seviyesi: ");
  Serial.println(WiFi.RSSI());//ağın sinyal çekim gücü

  clientTelegram.setInsecure();

  WiFi.printDiag(Serial);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), resetlefonk, CHANGE);

  delay(10);
  bot.sendMessage(CHAT_ID, "Bot Başlatıldı ANA KOMBİ", "");
  bot.sendMessage(CHAT_ID_M, "Bot Başlatıldı ANA KOMBİ", "");
}


void loop() {
  if (WiFi.status() != WL_CONNECTED)
  {
    ESP.restart();
  }

  if(kapat == false)
  {
    roleDurumlariAl();

    if (daire_1.role_1.durum =="off" && daire_2.role_1.durum =="off" && daire_2.role_2.durum == "off" && daire_3.role_1.durum == "off" && 
      daire_3.role_2.durum == "off" && daire_4.role_1.durum == "off" && daire_4.role_2.durum == "off" && daire_4.role_3.durum == "off")
    {
      digitalWrite(RELAY_PIN,HIGH); //kapalı
      digitalWrite(RELAY2_PIN,HIGH); //kapalı
      anaKombiRole=false;
    }
    else
    {
      digitalWrite(RELAY_PIN,LOW); // açık
      digitalWrite(RELAY2_PIN,LOW); // açık
      anaKombiRole=true;
    }
  }
  else
  {
    digitalWrite(RELAY_PIN,HIGH); //kapalı
    digitalWrite(RELAY2_PIN,HIGH); //kapalı
    anaKombiRole=false;
  }
  
  //-----------------------Telegram----------------------------------//

  telegramMsgKontrol();

  //--------------------------------------------------------------//

  //---------resetleme----------------//

  gunlukReset();

  resetKontrol();
  
}
