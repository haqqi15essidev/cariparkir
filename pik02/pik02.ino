#include <WiFi.h>
#include <ArduinoJson.h>
#include "RTClib.h"
#include "time.h"
#include <HTTPClient.h>
#include <EEPROM.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
int boot_trigger = 0;
int tone_flag = 0;

WebServer server(80);

int flag_first_data = 0;
int need_to_sleep = 0;

String get_addr = "https://hub.api.myveego.id/lockey/getStatus/1691103880002";
String post_addr = "https://hub.api.myveego.id/lockey/postStatus/";
String data_to_concat ="";
String booking_command = "";
int c_restart = 0;
DynamicJsonDocument doc(1024);

//time local & NTP config
// const char* ntpServer = "id.pool.ntp.org";
const char* ntpServer = "asia.pool.ntp.org";
const long  gmtOffset_sec = 3600*7;
const int   daylightOffset_sec = 3600;
String time_to_pub = "";
int day_now, month_now, year_now, hour_now, min_now, sec_now;

// ir sensor pin
#define lm1 12
#define lm2 13
#define lm3 15
int lm1read, lm2read, lm3read = 0;

//battery voltage buffer
float percen_vbat, percen_buffer_vbat ;
float voltage_vbat = 0;

//usage calculate
unsigned int start_time = 0;
unsigned int end_time = 0;
unsigned int parking_duration = 0;

// motor pin
#define r0 18 
#define r1 19
#define buzzer 17 
#define voltage 34

//ultrasonic pin
#define trigger 14
#define echo 35
int obstacle = 0;
int car_distance = 0;

//deepsleep config
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP_LONG  36000        /* Time ESP32 will go to sleep (in seconds) */
#define TIME_TO_SLEEP_SHORT  10        /* Time ESP32 will go to sleep (in seconds) */


RTC_DATA_ATTR int bootCount = 0;
RTC_DS1307 rtc;

//wifi setup
const char* host = "1691103880002";
const char* ssid = "cariparkir";
const char* password = "LockeyV1P";

//command buffer
int command_flag, buzzer_flag = 0;
String time_detail = "";
int close_time = 90000;

// unsigned long send_interval  = 1800000;//publish interval real
unsigned long send_interval  = 300000;//publish interval debugging
unsigned long previousTime = 0;//millis buffer
//millis() == milisekon timer dari internal clock arduino

String state = "1"; //string state to send

//production code
String imei = "1691103880002";
//MKG
// String latitude = "-6.21743528" ;
// String longitude = "106.66364";
//PIK
String latitude = "-6.108708" ;
String longitude = "106.740394";

//addres EEPROM
int status_lockey_addr = 9;


//web OTA
/* Style */
String style =
"<style>#file-input,input{width:100%;height:44px;border-radius:4px;margin:10px auto;font-size:15px}"
"input{background:#f1f1f1;border:0;padding:0 15px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
"#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:left;display:block;cursor:pointer}"
"#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
"form{background:#fff;max-width:258px;margin:75px auto;padding:30px;border-radius:5px;text-align:center}"
".btn{background:#3498db;color:#fff;cursor:pointer}</style>";

/* Login page */
String loginIndex = 
"<form name=loginForm>"
"<h1>ESP32 Login</h1>"
"<input name=userid placeholder='User ID'> "
"<input name=pwd placeholder=Password type=Password> "
"<input type=submit onclick=check(this.form) class=btn value=Login></form>"
"<script>"
"function check(form) {"
"if(form.userid.value=='admin' && form.pwd.value=='admin')"
"{window.open('/serverIndex')}"
"else"
"{alert('Error Password or Username')}"
"}"
"</script>" + style;
 
/* Server Index Page */
String serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
"<input type='file' name='update' id='file' onchange='sub(this)' style=display:none>"
"<label id='file-input' for='file'>   Choose file...</label>"
"<input type='submit' class=btn value='Update'>"
"<br><br>"
"<div id='prg'></div>"
"<br><div id='prgbar'><div id='bar'></div></div><br></form>"
"<script>"
"function sub(obj){"
"var fileName = obj.value.split('\\\\');"
"document.getElementById('file-input').innerHTML = '   '+ fileName[fileName.length-1];"
"};"
"$('form').submit(function(e){"
"e.preventDefault();"
"var form = $('#upload_form')[0];"
"var data = new FormData(form);"
"$.ajax({"
"url: '/update',"
"type: 'POST',"
"data: data,"
"contentType: false,"
"processData:false,"
"xhr: function() {"
"var xhr = new window.XMLHttpRequest();"
"xhr.upload.addEventListener('progress', function(evt) {"
"if (evt.lengthComputable) {"
"var per = evt.loaded / evt.total;"
"$('#prg').html('progress: ' + Math.round(per*100) + '%');"
"$('#bar').css('width',Math.round(per*100) + '%');"
"}"
"}, false);"
"return xhr;"
"},"
"success:function(d, s) {"
"console.log('success!') "
"},"
"error: function (a, b, c) {"
"}"
"});"
"});"
"</script>" + style;

WiFiClient wifiClient; 

void printLocalTime(){ 
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }else{
   hour_now = timeinfo.tm_hour;
   min_now  = timeinfo.tm_min;
   sec_now  = timeinfo.tm_sec;

   day_now = timeinfo.tm_mday;
   month_now = timeinfo.tm_mon + 1;
   year_now = timeinfo.tm_year +1900;
   rtc.adjust(DateTime(year_now, month_now, day_now, hour_now, min_now, sec_now));
  }
}
void setup_wifi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);

   IPAddress local_IP(192, 168, 0, 112);
   IPAddress gateway(192, 168, 0, 1);
   IPAddress subnet(255, 255, 255, 0);
   IPAddress primaryDNS(8, 8, 8, 8);   //optional
   IPAddress secondaryDNS(8, 8, 4, 4); //optional
   
 if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
   Serial.println("STA Failed to configure");
 }
  
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      c_restart += 1;
      if(c_restart >10 ){
        ESP.restart();
      }
    }
    randomSeed(micros());
    Serial.println("");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void setup_wifi_urgent_case() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    randomSeed(micros());
    Serial.println("");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void normal_closing(int flag_tone, String datatype){
  unsigned int breaking = 5000;
  unsigned int breaktrigger = millis();
  while(1){
  if(flag_tone){
    // Serial.println("buzzering on");
    digitalWrite(buzzer, HIGH);
  }else{
    // Serial.println("buzzering off");
    digitalWrite(buzzer, LOW);
  }
  obstacle = distance();
  Serial.print("lm1read: ");
  Serial.print(digitalRead(lm1));
  Serial.print(" || ");
  Serial.print("lm2read: ");
  Serial.println(digitalRead(lm2));
  adjustClose();
  delay(10);
  if(millis() - breaktrigger >= breaking ){
    Serial.println("back to opening");
    digitalWrite(buzzer,LOW);
    opening_alert();
    break;
    } 
  if(digitalRead(lm1) == 0 && digitalRead(lm2) == 0){
    digitalWrite(17, LOW);
    stopMovement();
    EEPROM.write(status_lockey_addr, 1);
    state = "1";
    booking_command = "1";
    command_flag = 0;
    tone_flag = 0;
    end_time = millis();
    parking_duration = (end_time - start_time)/1000;
    publish_data(datatype);
    delay(100);
    break;
    }     
   }  
  }

void wakeup_closing(int flag_tone, String datatype){
  while(1){
  if(flag_tone){
    // Serial.println("buzzering on");
    digitalWrite(buzzer, HIGH);
  }else{
    // Serial.println("buzzering off");
    digitalWrite(buzzer, LOW);
  }
  obstacle = distance();
  Serial.print("lm1read: ");
  Serial.print(digitalRead(lm1));
  Serial.print(" || ");
  Serial.print("lm2read: ");
  Serial.println(digitalRead(lm2));
  adjustClose();
  delay(5); 
  if(digitalRead(lm1) == 0 && digitalRead(lm2) == 0 ){
    digitalWrite(17, LOW);
    stopMovement();
    EEPROM.write(status_lockey_addr, 1);
    state = "1";
    booking_command = "1";
    command_flag = 0;
    tone_flag = 0;
    publish_data(datatype);
    delay(100);
    break;
    }     
   }  
  }


void overclose_closing(int flag_tone, String datatype){
  obstacle = distance();
    while(1){
    if(flag_tone){
      // Serial.println("buzzering on");
      digitalWrite(buzzer, HIGH);
    }else{
      // Serial.println("buzzering off");
      digitalWrite(buzzer, LOW);
    }
    Serial.print("lm1read: ");
    Serial.print(digitalRead(lm1));
    Serial.print(" || ");
    Serial.print("lm2read: ");
    Serial.println(digitalRead(lm2));
    adjustOpen();
    delay(5); 
    if(digitalRead(lm1) == 0 && digitalRead(lm2) == 0 ){
      digitalWrite(17, LOW);
      stopMovement();
      EEPROM.write(status_lockey_addr, 1);
      state = "1";
      tone_flag = 0;
      booking_command = "1";
      publish_data(datatype);
      delay(100);
      break;
      }     
    }  
}

void opening_alert(){
  stopMovement();
  digitalWrite(buzzer, LOW);
  delay(3000);
  unsigned int breaking = 900000;
  unsigned int breaktrigger = millis();
  while(1){
  obstacle = distance();
  Serial.print("lm1read: ");
  Serial.print(digitalRead(lm1));
  Serial.print(" || ");
  Serial.print("lm2read: ");
  Serial.println(digitalRead(lm2));
  adjustOpen(); 
  delay(5);
  if(digitalRead(lm1) == 1 && digitalRead(lm2) == 1 ){
    stopMovement();
    // publish_data("al");
    delay(100);
    obstacle = distance();
//    delay(900000);
    Serial.println("to execute waiting time");
    delay(900000);
    Serial.println("finish.. try to climb again.....");
    if((millis() - breaktrigger >= breaking) || !obstacle){
       obstacle = distance();
       break;
      }else{
        Serial.println("waiting for car movement");
        }
    }     
   }  
  }

void opening(){
  digitalWrite(buzzer,LOW);
  while(1){
  obstacle = distance();
  Serial.print("lm1read: ");
  Serial.print(digitalRead(lm1));
  Serial.print(" || ");
  Serial.print("lm2read: ");
  Serial.println(digitalRead(lm2));
  adjustOpen(); 
  delay(5);
  if(digitalRead(lm1) == 1 && digitalRead(lm2) == 1 ){
    stopMovement();
    EEPROM.write(status_lockey_addr, 0);
    state = "0";
    booking_command = "1";
    tone_flag = 1;
    start_time = millis();
    publish_data("ao");
    delay(close_time);
    break; 
    }     
   }  
  }

  
void publish_data(String stat_send){
  printLocalTime();
  String str = stat_send + "," + String(percen_vbat) + "," + String(state) + "," + String(car_distance) + "," + String(!obstacle) + "," + time_detail + "," + latitude + "," + longitude + "," + String(parking_duration) + "," + imei ;
  post_request(str);
  
}

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

int distance(){
    digitalWrite(trigger, LOW); // Set the trigger pin to low for 2uS 
    delayMicroseconds(2); 
    digitalWrite(trigger, HIGH); // Send a 10uS high to trigger ranging 
    delayMicroseconds(20); 
    digitalWrite(trigger, LOW); // Send pin low again 
    car_distance = pulseIn(echo, HIGH,26000); // Read in times pulse    
    car_distance= car_distance/58; //C
    if(car_distance > 100){
        return 0;
    }else if(car_distance < 100){
        return 1;
    }
}
void post_request(String datapost){
    String post_to = post_addr + datapost;
    Serial.println(post_to);
   if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;
      http.begin(post_to.c_str());
      http.addHeader("Authorization", "JWT eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpZCI6MzYsIm5vZGUiOiIzNyw0MCw0MSw0Myw0NCw0NSw0Niw0Nyw0OCw0OSw1MCw1MSw1Miw1Myw1NCw1NSw1Niw1Nyw1OCw1OSw2MCw2MSw2Miw2Myw2NCw2NSw2Niw2NywxMDMsMTA0LDEwNiwxMDcifQ.C6OyunKwtMl-KI9ilx47Ex2Lj-kJdV5twojLfk74KmU");
      
      int httpResponseCode = http.GET(); 
      
      if (httpResponseCode>0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();
    }else{
      Serial.println("wifi not connected");
      //setup_wifi();

    }
}

void get_request(String get_addr){
   if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;
      http.begin(get_addr.c_str());
      http.addHeader("Authorization", "JWT eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpZCI6MzYsIm5vZGUiOiIzNyw0MCw0MSw0Myw0NCw0NSw0Niw0Nyw0OCw0OSw1MCw1MSw1Miw1Myw1NCw1NSw1Niw1Nyw1OCw1OSw2MCw2MSw2Miw2Myw2NCw2NSw2Niw2NywxMDMsMTA0LDEwNiwxMDcifQ.C6OyunKwtMl-KI9ilx47Ex2Lj-kJdV5twojLfk74KmU");

      int httpResponseCode = http.GET();
      
      if (httpResponseCode>0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println("process the data");
        deserializeJson(doc, payload);
        JsonObject obj = doc.as<JsonObject>();
        String command = obj[String("lockey_status")];
        String val_long = obj["lng"];
        String val_lat = obj["lat"];
        int delay_from_user = obj["delay"];
        Serial.print("delay from myveego: ");
        Serial.print(delay_from_user);
        if(delay_from_user == close_time){
          Serial.println("is same");
          }else{
            close_time = delay_from_user;
            }
        booking_command = command;
        Serial.println("command open: ");
        Serial.println(booking_command);
        Serial.print("delay: ");
        Serial.println(delay_from_user);
      }else{
        boot_trigger +=1;
      if(boot_trigger >= 3){
        ESP.restart();
        }
        }
      http.end();
    }else{
      Serial.println("wifi not connected");
      boot_trigger +=1;
      if(boot_trigger >= 3){
        ESP.restart();
        }
    }
}

void adjustClose(){
  digitalWrite(r0, LOW);
  digitalWrite(r1, HIGH);
  }

void adjustOpen(){
  digitalWrite(r0, HIGH);
  digitalWrite(r1, LOW);
  }

void stopMovement(){
  digitalWrite(r0, LOW);
  digitalWrite(r1, LOW);
  }

void setup() {
  Serial.begin(9600);
  delay(250);
  EEPROM.begin(512);
  int status_lockey = EEPROM.read(status_lockey_addr);
  pinMode(lm1, INPUT_PULLUP);
  pinMode(lm2, INPUT_PULLUP);
  pinMode(lm3, INPUT_PULLUP);
  pinMode(r0, OUTPUT);
  pinMode(r1, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(voltage, INPUT);
  pinMode(trigger, OUTPUT);
  pinMode(echo, INPUT_PULLUP);
//  delay(2000);
  rtc.begin();
  Serial.setTimeout(500);
  setup_wifi();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
//  rtc.adjust(DateTime(year_now, month_now, day_now, hour_now, min_now, sec_now));
  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();
  
 if(status_lockey == 1){
    tone_flag = 0;
    if(digitalRead(lm1) == 0 && digitalRead(lm2) == 0){
      Serial.println("normal position wake up");
    }else if(digitalRead(lm1) == 1 && digitalRead(lm2) == 0){
      overclose_closing(1, "al");
    }else if(digitalRead(lm1) == 0 && digitalRead(lm2) == 1 ){
      normal_closing(1, "ac");
    }else{
      normal_closing(1, "ac");
    }
    //tambain publish data ketika wake up
  
 }else{
  Serial.println("Lockey booked");
  tone_flag = 1;
 }
 }


void loop() {
  server.handleClient();
//  digitalWrite(17, LOW);
  printLocalTime();
// if(hour_now == 22){
//   ++bootCount;
//   Serial.println("Boot number: " + String(bootCount));
//   print_wakeup_reason();
//   esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_LONG * uS_TO_S_FACTOR);
//   Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP_LONG) +
//   " Seconds");
//   Serial.flush();
//   esp_deep_sleep_start();
//   }
  if(command_flag != 1){
    get_request(get_addr);
    }
  Serial.print("command_flag: ");
  Serial.println(command_flag);
  Serial.print("booking command: ");
  Serial.println(booking_command);
  Serial.print("close_time: ");
  Serial.println(close_time);
  Serial.println("ls1: ");
  Serial.println(digitalRead(lm1));
  Serial.println("ls2: ");
  Serial.println(digitalRead(lm2));


  DateTime time = rtc.now();
  time_detail = String(time.timestamp(DateTime::TIMESTAMP_FULL));

   //batttery value filter
  voltage_vbat = (3.3 * ((float)analogRead(34) / 4095));
  percen_vbat = (voltage_vbat/2.10) * 100 ;

   if(percen_buffer_vbat <= 0 ){
     percen_buffer_vbat = percen_vbat;
   }else if(percen_buffer_vbat > 0 && percen_vbat > percen_buffer_vbat){
     percen_vbat = percen_buffer_vbat;
   }else{
     percen_buffer_vbat = percen_vbat;
   }

  if(booking_command == "0"){
      opening();
      command_flag = 1;
  }else{
      Serial.println("command is empty or not registered");
      obstacle = distance();
          if(!obstacle){
              Serial.println("no car detected"); 
                  if(digitalRead(lm1) == 0 && digitalRead(lm2) == 0 ){
                    Serial.println("close position wake up");
                    digitalWrite(buzzer, LOW);
                  }else if(digitalRead(lm1) == 0 && digitalRead(lm2) == 1){
                    Serial.println("overclose condition");
                    obstacle = distance();
                    delay(100);
                    wakeup_closing(1, "al");
                  }else if(digitalRead(lm1) == 1 && digitalRead(lm2) == 0){
                    Serial.println("between position");
                    obstacle = distance();
                    delay(100);
                    overclose_closing(1, "al");
                  }else{
                    if(command_flag){
                      delay(9000);
                      obstacle = distance();
                      normal_closing(1, "ac");
                    }else{
                      wakeup_closing(1, "al");
                    }
                  }
          }else{
              if((digitalRead(lm1) != 0 || digitalRead(lm2) != 0) && command_flag == 0 && !tone_flag){
                digitalWrite(buzzer, HIGH);
                Serial.println("buzzering on because vandal and car detected");
              }else{
                Serial.println("normal");
                digitalWrite(buzzer, LOW);
              }
          }  
  }

  if(flag_first_data == 0){
    obstacle =  distance();
    publish_data("pr");
    previousTime += send_interval;
    Serial.println("flag data sended");
    flag_first_data = 1;
    delay(100);
  }

  if(millis() - previousTime > send_interval){
//if(bootCount % 6 == 0){
    obstacle =  distance();
    publish_data("pr");
    previousTime = millis();
    Serial.println("data published");
    delay(500);
  }else{
    Serial.println("waiting to publish");
  }
    if(!command_flag){
//          Serial.print("booking command: ");
//          Serial.println(command_flag);
//          ++bootCount;
//          Serial.println("Boot number: " + String(bootCount));
//          print_wakeup_reason();
//          esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_SHORT * uS_TO_S_FACTOR);
//          Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP_SHORT) +
//          " Seconds");
//          Serial.flush();
          stopMovement(); 
//          esp_deep_sleep_start();
      }

}