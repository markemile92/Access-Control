//#############INCLUDES############
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino.h>
#include <U8g2lib.h>
#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
#include <WebServer.h>
#endif
#include <AutoConnect.h>
#include <AutoConnectCredential.h>
#include <PageBuilder.h>
#include <HTTPClient.h>

//#############DEFINES#############
#define SS_PIN 5
#define SS_PIN2 15
#define RST_PIN 2
#define red 33
#define blue 22
#define green 32
//#define DebugEnable 
//client states
#define processing 1
#define signInAccepted 2
#define signOutAccepted 3
#define signNotAccepted 4
#define newUser 5
//autoconnect
#define CREDENTIAL_OFFSET 0

//###################Auto_Connect##############

#if defined(ARDUINO_ARCH_ESP8266)
ESP8266WebServer Server;
#elif defined(ARDUINO_ARCH_ESP32)
WebServer Server;
#endif

AutoConnect      Portal(Server);
String viewCredential(PageArgument&);
String delCredential(PageArgument&);





//#########GLOBAL_VARIABLES########
byte nuidPICC[4];
byte nuidPICC2[4];
String Reader1Data = "";
String Reader2Data = "";
String Name="";
boolean reader1Ready= false;
boolean reader2Ready= false;
int clientStatus;
int connectionCounter ;
int screenInitCnt;
int screenCnt ;



//for WIFI 
//##########################################|
//const char* ssid = "Samer";          //####|
//const char* password =  "pierre92";//####|
//##########################################|


String string2Send ; 

//#############RFIDs###############
MFRC522 rfid[2];
MFRC522::MIFARE_Key key;

TaskHandle_t Core0Task;  //Code for core 0

U8G2_ST7920_128X64_F_SW_SPI screen1(U8G2_R0, /* clock=*/ 13, /* data=*/ 12, /* CS=*/ 14, /* reset=*/ 8);

U8G2_ST7920_128X64_F_SW_SPI screen2(U8G2_R0, /* clock=*/ 27, /* data=*/ 26, /* CS=*/ 25, /* reset=*/ 8);

//########################Auto_Connect_Code#######################
static const char PROGMEM html[] = R"*lit(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8" name="viewport" content="width=device-width, initial-scale=1">
  <style>
  html {
  font-family:Helvetica,Arial,sans-serif;
  -ms-text-size-adjust:100%;
  -webkit-text-size-adjust:100%;
  }
  .menu > a:link {
    position: absolute;
    display: inline-block;
    right: 12px;
    padding: 0 6px;
    text-decoration: none;
  }
  </style>
</head>
<body>
<div class="menu">{{AUTOCONNECT_MENU}}</div>
<form action="/del" method="POST">
  <ol>
  {{SSID}}
  </ol>
  <p>Enter deleting entry:</p>
  <input type="number" min="1" name="num">
  <input type="submit">
</form>
</body>
</html>
)*lit";

static const char PROGMEM autoconnectMenu[] = { AUTOCONNECT_LINK(BAR_24) };

// URL path as '/'
PageElement elmList(html,
  {{ "SSID", viewCredential },
   { "AUTOCONNECT_MENU", [](PageArgument& args) {
                            return String(FPSTR(autoconnectMenu));} }
  });
PageBuilder rootPage("/", { elmList });

// URL path as '/del'
PageElement elmDel("{{DEL}}", {{ "DEL", delCredential }});
PageBuilder delPage("/del", { elmDel });

// Retrieve the credential entries from EEPROM, Build the SSID line
// with the <li> tag.
String viewCredential(PageArgument& args) {
  AutoConnectCredential  ac(CREDENTIAL_OFFSET);
  struct station_config  entry;
  String content = "";
  uint8_t  count = ac.entries();          // Get number of entries.

  for (int8_t i = 0; i < count; i++) {    // Loads all entries.
    ac.load(i, &entry);
    // Build a SSID line of an HTML.
    content += String("<li>") + String((char *)entry.ssid) + String("</li>");
  }


  // Returns the '<li>SSID</li>' container.
  return content;
}

// Delete a credential entry, the entry to be deleted is passed in the
// request parameter 'num'.
String delCredential(PageArgument& args) {
  AutoConnectCredential  ac(CREDENTIAL_OFFSET);
  if (args.hasArg("num")) {
    int8_t  e = args.arg("num").toInt();
    if (e > 0) {
      struct  station_config entry;

      // If the input number is valid, delete that entry.
      int8_t  de = ac.load(e - 1, &entry);  // A base of entry num is 0.
      if (de > 0) {
        ac.del((char *)entry.ssid);

        // Returns the redirect response. The page is reloaded and its contents
        // are updated to the state after deletion. It returns 302 response
        // from inside this token handler.
        Server.sendHeader("Location", String("http://") + Server.client().localIP().toString() + String("/"));
        Server.send(302, "text/plain", "");
        Server.client().flush();
        Server.client().stop();

        // Cancel automatic submission by PageBuilder.
        delPage.cancel();
      }
    }
  }
  return "";
}

void AP(){

  rootPage.insert(Server);    // Instead of Server.on("/", ...);
  delPage.insert(Server);     // Instead of Server.on("/del", ...);

  // Set an address of the credential area.
  AutoConnectConfig Config;
  Config.boundaryOffset = CREDENTIAL_OFFSET;
  Portal.config(Config);

  screen1.clearBuffer();
  screen1.setFont(u8g2_font_t0_22b_me);//u8g2_font_t0_22b_me
  screen1.setCursor(0,20);
  screen1.print("IoTricks");
  screen1.sendBuffer();
  screen2.clearBuffer();
  screen2.setFont(u8g2_font_t0_22b_me);
  screen2.setCursor(0,20);
  screen2.print("IoTricks");
  screen2.sendBuffer();

  //WiFi.begin(ssid, password); 
  screen2.setFont(u8g2_font_6x12_me);
  screen2.setCursor(0,40);
  screen2.print("Connecting to Wifi");
  screen1.setFont(u8g2_font_6x12_me);
  screen1.setCursor(0,40);
  screen1.print("Connecting to Wifi");
    if ( screenInitCnt < 4 ){
      screenInitCnt++;
      screen1.print(".");
      screen1.sendBuffer();
      screen2.print(".");
      screen2.sendBuffer();
    }else{
      screenInitCnt=0;
      screen1.clearBuffer();
      screen1.setFont(u8g2_font_t0_22b_me);//u8g2_font_t0_22b_me
      screen1.setCursor(0,20);
      screen1.print("IoTricks");
      screen2.clearBuffer();
      screen2.setFont(u8g2_font_t0_22b_me);
      screen2.setCursor(0,20);
      screen2.print("IoTricks");
      screen2.setFont(u8g2_font_6x12_me);
      screen2.setCursor(0,40);
      screen2.print("Connecting to Wifi");
      screen1.setFont(u8g2_font_6x12_me);
      screen1.setCursor(0,40);
      screen1.print("Connecting to Wifi");
      screen1.sendBuffer();
      screen2.sendBuffer();
    }
    delay(500);
  // Start
  if (Portal.begin()) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
    Serial.println("WIFI connected..");
    screen1.setCursor(0,40);
    screen1.print("WIFI connected       ");
    screen1.sendBuffer();
    screen2.setCursor(0,40);
    screen2.print("WIFI connected       ");
    screen2.sendBuffer();
    delay(2000);
    screen1.clearBuffer();
    screen1.setFont(u8g2_font_t0_22b_me);//u8g2_font_t0_22b_me
    screen1.setCursor(0,20);
    screen1.print("Club5");
    screen1.setCursor(50,30);
    screen1.setFont(u8g2_font_4x6_tf);
    screen1.print("Powered by IoTricks");
    screen2.clearBuffer();
    screen2.setFont(u8g2_font_t0_22b_me);//u8g2_font_t0_22b_me
    screen2.setCursor(0,20);
    screen2.print("Club5");
    screen2.setCursor(50,30);
    screen2.setFont(u8g2_font_4x6_tf);
    screen2.print("Powered by IoTricks");
    screen1.setCursor(10,50);
    screen1.setFont(u8g2_font_6x10_tf);
    screen1.print("Sign In");
    screen1.sendBuffer();
    screen2.setCursor(10,50);
    screen2.setFont(u8g2_font_6x10_tf);
    screen2.print("Sign Out");
    screen2.sendBuffer();
    
    
    
  }
    
    
  }
 


//#################################################################

void setup() {

  Serial.begin(115200);
  delay(1000);  
  SPI.begin();
  delay(1000);
  screen1.begin();
  screen2.begin();
  delay(1000);
  pinMode( red, OUTPUT);
  pinMode( blue, OUTPUT);
  pinMode( green, OUTPUT);
  
  //Parameters for core 0 task
  xTaskCreatePinnedToCore(
      Core0code, /* Function to implement the task */
      "Core0code", /* Name of the task */
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      1,  /* Priority of the task */
      &Core0Task,  /* Task handle. */
      0); /* Core where the task should run */
      
  

  rfid[0].PCD_Init(SS_PIN2,RST_PIN);
  rfid[1].PCD_Init(SS_PIN,RST_PIN);

  
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

//logo
  
  /*
  while (WiFi.status() != WL_CONNECTED) { //Check for the connection
    if ( screenInitCnt < 4 ){
      screenInitCnt++;
      screen1.print(".");
      screen1.sendBuffer();
      screen2.print(".");
      screen2.sendBuffer();
    }else{
      screenInitCnt=0;
      screen1.clearBuffer();
      screen1.setFont(u8g2_font_t0_22b_me);//u8g2_font_t0_22b_me
      screen1.setCursor(0,20);
      screen1.print("IoTricks");
      screen2.clearBuffer();
      screen2.setFont(u8g2_font_t0_22b_me);
      screen2.setCursor(0,20);
      screen2.print("IoTricks");
      screen2.setFont(u8g2_font_6x12_me);
      screen2.setCursor(0,40);
      screen2.print("Connecting to Wifi");
      screen1.setFont(u8g2_font_6x12_me);
      screen1.setCursor(0,40);
      screen1.print("Connecting to Wifi");
      screen1.sendBuffer();
      screen2.sendBuffer();
    }
    delay(500);
    
    
  }
    Serial.println("WIFI connected..");
    screen1.setCursor(0,40);
    screen1.print("WIFI connected       ");
    screen1.sendBuffer();
    screen2.setCursor(0,40);
    screen2.print("WIFI connected       ");
    screen2.sendBuffer();
    delay(2000);
    screen1.clearBuffer();
    screen1.setFont(u8g2_font_t0_22b_me);//u8g2_font_t0_22b_me
    screen1.setCursor(0,20);
    screen1.print("Club5");
    screen1.setCursor(50,30);
    screen1.setFont(u8g2_font_4x6_tf);
    screen1.print("Powered by IoTricks");
    screen2.clearBuffer();
    screen2.setFont(u8g2_font_t0_22b_me);//u8g2_font_t0_22b_me
    screen2.setCursor(0,20);
    screen2.print("Club5");
    screen2.setCursor(50,30);
    screen2.setFont(u8g2_font_4x6_tf);
    screen2.print("Powered by IoTricks");
    screen1.setCursor(10,50);
    screen1.setFont(u8g2_font_6x10_tf);
    screen1.print("Sign In");
    screen1.sendBuffer();
    screen2.setCursor(10,50);
    screen2.setFont(u8g2_font_6x10_tf);
    screen2.print("Sign Out");
    screen2.sendBuffer();
    */
  AP();
}


//#############LOOP_CORE1#########

void loop() {
  
  //################READER1######
  while((rfid[0].PICC_IsNewCardPresent()) && (reader1Ready == false)){
  
    if (rfid[0].PICC_ReadCardSerial()){
      convert2String( rfid[0].uid.uidByte, rfid[0].uid.size , 1);
      reader1Ready = true;
      #ifdef DebugEnable
        Serial.print(F("PICC type: "));
        MFRC522::PICC_Type piccType = rfid[0].PICC_GetType(rfid[0].uid.sak);
        Serial.println(rfid[0].PICC_GetTypeName(piccType));
        for (byte i = 0; i < 4; i++) {
          nuidPICC[i] = rfid[0].uid.uidByte[i];
        }
        
        Serial.print("From Reader 1 /n");
        Serial.println(F("The NUID tag is:"));
        Serial.print(F("In hex: "));
        printHex(rfid[0].uid.uidByte, rfid[0].uid.size);
        Serial.println();
        Serial.print(F("In dec: "));
        printDec(rfid[0].uid.uidByte, rfid[0].uid.size);
        Serial.println();
      #endif
    }
    delay(500);
  }
  // Halt PICC
  rfid[0].PICC_HaltA();

  // Stop encryption on PCD
  rfid[0].PCD_StopCrypto1();

  //#############READER2######
  while(rfid[1].PICC_IsNewCardPresent()&& (reader2Ready == false)){
    
    if (rfid[1].PICC_ReadCardSerial()){
      convert2String( rfid[1].uid.uidByte, rfid[1].uid.size , 2);
      reader2Ready = true;
      #ifdef DebugEnable
        Serial.print(F("PICC type: "));
        MFRC522::PICC_Type piccType = rfid[1].PICC_GetType(rfid[1].uid.sak);
        Serial.println(rfid[1].PICC_GetTypeName(piccType));
        for (byte i = 0; i < 4; i++) {
          nuidPICC2[i] = rfid[1].uid.uidByte[i];
        }
      
        Serial.print("From Reader 2 /n");
        Serial.println(F("The NUID tag is:"));
        Serial.print(F("In hex: "));
        printHex(rfid[1].uid.uidByte, rfid[1].uid.size);
        Serial.println();
        Serial.print(F("In dec: "));
        printDec(rfid[1].uid.uidByte, rfid[1].uid.size);
        Serial.println();
      #endif
      
    }
    delay(500);
  }
  // Halt PICC
  rfid[1].PICC_HaltA();

  // Stop encryption on PCD
  rfid[1].PCD_StopCrypto1();


  
    LedActivate();
    if ( screenCnt < 2 ){
      screenCnt++ ;
    }else{
      screenCnt=0;
      screenTask();
    }
  
}

//############FUNCTIONS##########

void screenTask(){
  screen1.clearBuffer();
  screen2.clearBuffer();
  switch(clientStatus){
    case(processing):
      CmnScreen();
      screen1.setCursor(10,50);
      screen1.setFont(u8g2_font_6x10_tf);
      screen1.print("Processing...");
      screen2.setCursor(10,50);
      screen2.setFont(u8g2_font_6x10_tf);
      screen2.print("Processing...");
      screen1.sendBuffer();
      screen2.sendBuffer();
    break;
    
    case(signInAccepted):
      
    break;
    
    case(signNotAccepted):
      
    
      //screen1.sendBuffer();
      //screen2.sendBuffer();
    break;
    
    case(newUser):
      
    
      //screen1.sendBuffer();
      //screen2.sendBuffer();
    break;
    default:
      CmnScreen();
      screen1.setCursor(10,50);
      screen1.setFont(u8g2_font_6x10_tf);
      screen1.print("Sign In");
      screen1.sendBuffer();
      screen2.setCursor(10,50);
      screen2.setFont(u8g2_font_6x10_tf);
      screen2.print("Sign Out");
      screen2.sendBuffer();
    break;
    
  }

  
}

void CmnScreen(){
    screen1.setFont(u8g2_font_t0_22b_me);//u8g2_font_t0_22b_me
    screen1.setCursor(0,20);
    screen1.print("Club5");
    screen1.setCursor(50,30);
    screen1.setFont(u8g2_font_4x6_tf);
    screen1.print("Powered by IoTricks");
    

    screen2.clearBuffer();
    screen2.setFont(u8g2_font_t0_22b_me);//u8g2_font_t0_22b_me
    screen2.setCursor(0,20);
    screen2.print("Club5");
    screen2.setCursor(50,30);
    screen2.setFont(u8g2_font_4x6_tf);
    screen2.print("Powered by IoTricks");
    
}

void LedActivate(){
  if( clientStatus == processing ){
    digitalWrite(blue,HIGH);
    delay(100);
    digitalWrite(blue,LOW);
  }
  if ( (clientStatus == signInAccepted ) || (clientStatus == signOutAccepted )){
    if((clientStatus == signInAccepted ) ){
      //////////////////////////
      CmnScreen();
      Serial.println(Name);
      screen1.setCursor(10,40);
      screen1.setFont(u8g2_font_6x10_tf);
      screen1.print("Welcome ");
      screen1.setCursor(10,55);
      screen1.setFont(u8g2_font_6x10_tf);
      screen1.print(Name);
      screen2.setCursor(10,40);
      screen2.setFont(u8g2_font_6x10_tf);
      screen2.print("Welcome ");
      screen2.setCursor(10,55);
      screen2.setFont(u8g2_font_6x10_tf);
      screen2.print(Name);
      screen1.sendBuffer();
      screen2.sendBuffer();
      delay(1000);
      //////////////////////////////
    }else{
      //////////////////////////
      CmnScreen();
      Serial.println(Name);
      screen1.setCursor(10,40);
      screen1.setFont(u8g2_font_6x10_tf);
      screen1.print("Goodbye ");
      screen1.setCursor(10,55);
      screen1.setFont(u8g2_font_6x10_tf);
      screen1.print(Name);
      screen2.setCursor(10,40);
      screen2.setFont(u8g2_font_6x10_tf);
      screen2.print("Goodbye ");
      screen2.setCursor(10,55);
      screen2.setFont(u8g2_font_6x10_tf);
      screen2.print(Name);
      screen1.sendBuffer();
      screen2.sendBuffer();
      delay(1000);
      //////////////////////////////
    }
    
    digitalWrite(green,HIGH);
    delay(1000);
    digitalWrite(green,LOW);
    clientStatus = 0 ; 
  }
  if (clientStatus == signNotAccepted){
    digitalWrite(red,HIGH);
    delay(1000);
    digitalWrite(red,LOW);
    clientStatus = 0 ; 
  }
  if (clientStatus == newUser){
    digitalWrite(red,HIGH);
    digitalWrite(blue,HIGH);
    delay(500);
    digitalWrite(red,LOW);
    digitalWrite(blue,LOW);
    delay(500);
    digitalWrite(red,HIGH);
    digitalWrite(blue,HIGH);
    delay(500);
    digitalWrite(red,LOW);
    digitalWrite(blue,LOW);
    clientStatus = 0 ; 
  }
  if((clientStatus == 0) && (connectionCounter > 40 )){
    if(Portal.begin()){
    digitalWrite(red,HIGH);
    digitalWrite(blue,HIGH);
    digitalWrite(green,HIGH);
    delay(50);
    digitalWrite(red,LOW);
    digitalWrite(blue,LOW);
    digitalWrite(green,LOW);
    
    connectionCounter= 0;
  }else{
    digitalWrite(red,HIGH);
    delay(50);
    digitalWrite(red,LOW);
    
    connectionCounter= 0;
  }
  }
  
  connectionCounter++;
}

void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}


void printDec(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  } 
}


void convert2String ( byte *buffer, byte bufferSize , int core ){
  if(core == 1){
     Reader1Data = "";
     for (byte i = 0; i < bufferSize; i++) {
        Reader1Data=Reader1Data+String(buffer[i] ,HEX);
     }
  }
  if(core == 2){
     Reader2Data = "";
     for (byte i = 0; i < bufferSize; i++) {
        Reader2Data=Reader2Data+String(buffer[i] ,HEX);
     }
  }
  
}

void printName(String response){
  String response_1= response.substring(response.indexOf("name"),response.indexOf("time"));
  //Serial.println(response.indexOf("name"));
  //Serial.println(y);
  int i=7;
  Name="";
  while(1){
    if(response_1.charAt(i)=='\"'){
      break;
  }
    else{
      Name.concat(response_1.charAt(i));
  }
    i++;
}
}
//#################Core0_Task##################

void Core0code( void * parameter) {
  // Add AP()



  /////////////////////////////////////HTTP////////////////////////
  
  for(;;) {
    delay(10);
    //Serial.println("Core1");

    if(reader1Ready){
      clientStatus = processing;
      HTTPClient http;   

      http.begin("https://club5.iotricks.com/api/v1/tags?sign_in_or_sign_up=true");  //Specify destination for HTTP request
      http.addHeader("Content-Type", "application/JSON");             //Specify content-type header
   
     string2Send = "{\"tag\": {\"uid\":  \"" + Reader1Data + " \"}}";

     int httpResponseCode = http.POST(string2Send);   //Send the actual POST request

     String response = http.getString();                       //Get the response to the request
        //check response 
     if ( ( httpResponseCode == 200 ) && ( response.indexOf("signed") >0) ){
        if(response.indexOf("signed_in")>0){
          clientStatus = signInAccepted;
        }else if( response.indexOf("signed_out")>0){
          clientStatus = signOutAccepted;
        }
        printName(response);
        delay(500);
        #ifdef DebugEnable
          Serial.println("Sign Success");   
        #endif
     }else if((httpResponseCode != 200)){
         clientStatus = signNotAccepted;
         delay(1000);
        #ifdef DebugEnable
          Serial.println("Sign Failed");   
        #endif
     }else if((httpResponseCode == 200) && ( response.indexOf("created" ) > 0 )){
      clientStatus = newUser;
        #ifdef DebugEnable
          Serial.println("New User");   
        #endif
     }

     
     if(httpResponseCode>0){
        #ifdef DebugEnable
        Serial.println(httpResponseCode);   //Print return code
        Serial.println(response);           //Print request answer
        #endif
      }else{
        #ifdef DebugEnable
        Serial.print("Error on sending POST: ");
        Serial.println(httpResponseCode);
        #endif
      }
      delay(500);
      reader1Ready =false;
      
    }

    
    if(reader2Ready){
      clientStatus = processing;
      HTTPClient http;   

      http.begin("https://club5.iotricks.com/api/v1/tags?sign_out=true");  //Specify destination for HTTP request
      http.addHeader("Content-Type", "application/JSON");             //Specify content-type header
   
      string2Send = "{\"tag\": {\"uid\":  \"" + Reader2Data + " \"}}";

      int httpResponseCode = http.POST(string2Send);   //Send the actual POST request

      String response = http.getString();                       //Get the response to the request
      //printName(response);
        //check response 
     if ( ( httpResponseCode == 200 ) && ( response.indexOf("signed")>0) ){
        if(response.indexOf("signed_in")>0){
          clientStatus = signInAccepted;
        }else if( response.indexOf("signed_out")>0){
          clientStatus = signOutAccepted;
        }
        
        printName(response);
        delay(500);
        #ifdef DebugEnable
          Serial.println("Sign Success");   
        #endif
     }else if((httpResponseCode != 200)){
         clientStatus = signNotAccepted;
         delay(1000);
        #ifdef DebugEnable
          Serial.println("Sign Failed");   
        #endif
     }else if((httpResponseCode == 200) && ( response.indexOf("created" ) > 0 )){
      clientStatus = newUser;
        #ifdef DebugEnable
          Serial.println("New User");   
        #endif
     }
       
       if(httpResponseCode>0){
          #ifdef DebugEnable
          Serial.println(httpResponseCode);   //Print return code
          Serial.println(response);           //Print request answer
          #endif
        }else{
          #ifdef DebugEnable
          Serial.print("Error on sending POST: ");
          Serial.println(httpResponseCode);
          #endif
        }
        delay(500);
        reader2Ready=false;
        
      }

    
  }
    vTaskDelete( NULL );

}

