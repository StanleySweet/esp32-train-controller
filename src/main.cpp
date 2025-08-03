#include <Arduino.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <string>

#include "mqtt_handler.h"
#include "tb6612fng_motor.h"

const char *ssid = "SSID";
const char *password = "PASSWORD";
const char *mqtt_server = "IP";
const char *mqtt_user = "esp32-tchoutchou";
const char *mqtt_pass = "PASSWORD";
/*
 * Login page
 */
const char *loginIndex =
    "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
    "<tr>"
    "<td colspan=2>"
    "<center><font size=4><b>ESP32 Login Page</b></font></center>"
    "<br>"
    "</td>"
    "<br>"
    "<br>"
    "</tr>"
    "<td>Username:</td>"
    "<td><input type='text' size=25 name='userid'><br></td>"
    "</tr>"
    "<br>"
    "<br>"
    "<tr>"
    "<td>Password:</td>"
    "<td><input type='Password' size=25 name='pwd'><br></td>"
    "<br>"
    "<br>"
    "</tr>"
    "<tr>"
    "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
    "</tr>"
    "</table>"
    "</form>"
    "<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='admin' && form.pwd.value=='admin')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
    "</script>";

/*
 * Server Index Page
 */

const char *serverIndex =
    "<script "
    "src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></"
    "script>"
    "<form method='POST' action='#' enctype='multipart/form-data' "
    "id='upload_form'>"
    "<input type='file' name='update'>"
    "<input type='submit' value='Update'>"
    "</form>"
    "<div id='prg'>progress: 0%</div>"
    "<script>"
    "$('form').submit(function(e){"
    "e.preventDefault();"
    "var form = $('#upload_form')[0];"
    "var data = new FormData(form);"
    " $.ajax({"
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
    "}"
    "}, false);"
    "return xhr;"
    "},"
    "success:function(d, s) {"
    "console.log('success!')"
    "},"
    "error: function (a, b, c) {"
    "}"
    "});"
    "});"
    "</script>";

enum class EMotorMode { START, STOP };

enum class EMotorDirection { FORWARD, REVERSE };

WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);

// Motor control pins (update according to ESP32-C3 Super Mini pinout)
int IN1{1};
int IN2{2};
int PWM_PIN{5}; // PWM pin for TB6612FNG motor driver

// QRE1113 digital sensor pins (update according to ESP32-C3 Super Mini pinout)
uint16_t SENSOR_LEFT{3};
uint16_t SENSOR_RIGHT{4};

// State tracking
int lastLeft{-1};
int lastRight{-1};

std::string DEVICE_ID = mqtt::DeviceInfo::getDeviceId();
std::string MOTOR_STATE_TOPIC = (DEVICE_ID + "/motor/state");
std::string MOTOR_DIRECTION_STATE_TOPIC =
    (DEVICE_ID + "/motor_direction/state");
std::string SENSOR_LEFT_STATE_TOPIC = (DEVICE_ID + "/line_left_sensor/state");
std::string SENSOR_RIGHT_STATE_TOPIC = (DEVICE_ID + "/line_right_sensor/state");
std::string IP_STATE_TOPIC = (DEVICE_ID + "/ip/state");
std::string AVAILABILITY_TOPIC = DEVICE_ID + "/availability";

EMotorMode motorMode{EMotorMode::STOP};
EMotorDirection currentDirection{EMotorDirection::FORWARD};
TB6612FNGMotor motor(IN1, IN2, PWM_PIN, 0, 2000, 8);

void setup_wifi() {
  WiFiGenericClass::mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("\nConnecting to WiFi Network..");
  while (WiFiSTAClass::status() != wl_status_t::WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.print(" Done\nIP: ");
  Serial.println(WiFi.localIP());
}

void publishSensorStates(int left, int right) {
  if (left != lastLeft) {
    client.publish(SENSOR_LEFT_STATE_TOPIC.c_str(),
                   left == LOW ? "BLACK" : "WHITE", true);
    lastLeft = left;
  }
  if (right != lastRight) {
    client.publish(SENSOR_RIGHT_STATE_TOPIC.c_str(),
                   right == LOW ? "BLACK" : "WHITE", true);
    lastRight = right;
  }
}

void setMotorDirection(EMotorDirection direction) {
  if (direction == currentDirection)
    return;
  currentDirection = direction;

  switch (direction) {
  case EMotorDirection::FORWARD:
    motor.forward(200); // adjust speed 0-255 as needed
    client.publish(MOTOR_DIRECTION_STATE_TOPIC.c_str(), "FORWARD", true);
    break;
  case EMotorDirection::REVERSE:
    motor.reverse(200);
    client.publish(MOTOR_DIRECTION_STATE_TOPIC.c_str(), "REVERSE", true);
    break;
  default:
    motor.stop();
    client.publish(MOTOR_DIRECTION_STATE_TOPIC.c_str(), "STOPPED", true);
    break;
  }
}

void callback(char *topic, byte *payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.print("MQTT Message [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(msg);

  if (msg == F("START") && motorMode == EMotorMode::STOP) {
    motorMode = EMotorMode::START;
    motor.forward(
        200); // start motor forward at speed 200 or your desired default
    client.publish(MOTOR_STATE_TOPIC.c_str(), "START", true);
  } else if (msg == F("STOP") && motorMode == EMotorMode::START) {
    motorMode = EMotorMode::STOP;
    motor.stop();
    client.publish(MOTOR_STATE_TOPIC.c_str(), "STOP", true);
  }
}

void reconnect() {
  Serial.print("Attempting MQTT connection...");
  if (client.setBufferSize(768)) {
    Serial.println("Buffer Size increased to 768 byte");
  } else {
    Serial.println("Failed to allocate larger buffer");
  }
  Serial.print("\nConnecting to MQTT server...");
  if (client.connect(DEVICE_ID.c_str(), mqtt_user, mqtt_pass,
                     AVAILABILITY_TOPIC.c_str(), 1, true, "offline")) {
    Serial.println(" Done.");
    client.publish(AVAILABILITY_TOPIC.c_str(), "online", true);
    mqtt::DiscoveryPublisher::publishDiscoveryConfig(client);
    client.subscribe((DEVICE_ID + "/#").c_str());
    client.subscribe("homeassistant/status");
    IPAddress local_ip = WiFi.localIP();
    String ipStr = local_ip.toString();
    client.publish(IP_STATE_TOPIC.c_str(), ipStr.c_str(), true);
    client.publish(MOTOR_STATE_TOPIC.c_str(), "STOP", true);
    client.publish(MOTOR_DIRECTION_STATE_TOPIC.c_str(), "FORWARD", true);
    client.publish(SENSOR_LEFT_STATE_TOPIC.c_str(), "WHITE", true);
    client.publish(SENSOR_RIGHT_STATE_TOPIC.c_str(), "WHITE", true);

  } else {
    Serial.println(" Failed.");
    Serial.println("Waiting 2 seconds...");
    delay(2000);
  }
}


void setup_mdns(){
    /*use mdns for host name resolution*/
  if (!MDNS.begin(DEVICE_ID.c_str())) { // http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
}

void setup_mqtt() {
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void setup_webserver(){
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
  server.on(
      "/update", HTTP_POST,
      []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
      },
      []() {
        HTTPUpload &upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
          Serial.printf("Update: %s\n", upload.filename.c_str());
          if (!Update.begin(
                  UPDATE_SIZE_UNKNOWN)) { // start with max available size
            Update.printError(Serial);
          }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
          /* flashing firmware to ESP*/
          if (Update.write(upload.buf, upload.currentSize) !=
              upload.currentSize) {
            Update.printError(Serial);
          }
        } else if (upload.status == UPLOAD_FILE_END) {
          if (Update.end(true)) { // true to set the size to the current
                                  // progress
            Serial.printf("Update Success: %u\nRebooting...\n",
                          upload.totalSize);
          } else {
            Update.printError(Serial);
          }
        }
      });
  server.begin();
}

void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_LEFT, INPUT);
  pinMode(SENSOR_RIGHT, INPUT);
  Serial.println("Starting TB6612FNG Motor Driver...");
  motor.begin();
  setup_wifi();
  setup_mqtt();
  setup_mdns();
  setup_webserver();
}

bool bootComplete = false;

unsigned long lastMeasureTime = 0;

int readQD(uint16_t pin){
  //Returns value from the QRE1113 
  //Lower numbers mean more refleacive
  //More than 3000 means nothing was reflected.
  pinMode( pin, OUTPUT );
  digitalWrite( pin, HIGH );  
  delayMicroseconds(10);
  pinMode( pin, INPUT );

  long time = micros();

  //time how long the input is HIGH, but quit after 3ms as nothing happens after that
  while (digitalRead(pin) == HIGH && micros() - time < 3000); 
  int diff = micros() - time; 

  return diff;
}


void loop() {
  if (!client.connected()) {
    reconnect();
  } else {
    client.loop();
    server.handleClient();

    static unsigned long bootTime = millis();
    if (!bootComplete && millis() - bootTime > 5000) {
      bootComplete = true;
    }

    if (bootComplete && millis() - lastMeasureTime >= 200) {
      unsigned long now = millis();
      lastMeasureTime = now;
      
      Serial.printf("Left Sensor: %d\n", readQD(SENSOR_LEFT));
      Serial.printf("Right Sensor: %d\n", readQD(SENSOR_RIGHT));

      int left = digitalRead(SENSOR_LEFT);
      int right = digitalRead(SENSOR_RIGHT);
      publishSensorStates(left, right);

      if (motorMode == EMotorMode::START) {
        if (left == HIGH) {
          setMotorDirection(EMotorDirection::REVERSE);
        } else if (right == HIGH) {
          setMotorDirection(EMotorDirection::FORWARD);
        } else {
          setMotorDirection(EMotorDirection::FORWARD);
        }
      }
    }
  }
}
