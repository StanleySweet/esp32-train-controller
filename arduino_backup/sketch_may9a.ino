#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

const char* ssid = "SSID";
const char* password = "PASSWORD";
const char* mqtt_server = "IP";
const char* mqtt_user = "esp32-tchoutchou";
const char* mqtt_pass = "PASSWORD";
const char* deviceId = "train_controller_01";
const char* availabilityTopic = "train_controller_01/availability";
const char* host = "train-controller-01";
/*
 * Login page
 */
const char* loginIndex = 
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
 
const char* serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
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


enum class EMotorMode {
  START,
  STOP
};

enum class EMotorDirection {
  FORWARD,
  REVERSE
};

WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);

// Motor control pins (update according to ESP32-C3 Super Mini pinout)
int IN1{ 1 };
int IN2{ 2 };

// QRE1113 digital sensor pins (update according to ESP32-C3 Super Mini pinout)
int SENSOR_LEFT{ 3 };
int SENSOR_RIGHT{ 4 };

// State tracking
int lastLeft{ -1 };
int lastRight{ -1 };

EMotorMode motorMode{ EMotorMode::STOP };
EMotorDirection currentDirection{ EMotorDirection::FORWARD };

void publishDiscoveryConfig() {
  // Device info (shared by all entities)
  const char* deviceInfo = ("{"
                            "\"identifiers\": [\"train_controller_01\"],"
                            "\"name\": \"Electric Train Controller\","
                            "\"manufacturer\": \"Stan Industries\","
                            "\"model\": \"ESP32-C3 Super Mini\","
                            "\"sw_version\": \"1.0\""
                            "}");

  // Motor direction sensor
  client.publish("homeassistant/sensor/train_controller_01/direction/config",
                 ("{"
                  "\"name\": \"Train Motor Direction\","
                  "\"state_topic\": \"motor/direction\","
                  "\"unique_id\": \"train_controller_01_direction\","
                  "\"device_class\": \"enum\","
                  "\"icon\": \"mdi:arrow-left-right\","
                  "\"device\": "
                  + String(deviceInfo) + "}")
                   .c_str(),
                 true);

  // Line sensor left
  client.publish("homeassistant/binary_sensor/train_controller_01/line_left/config",
                 ("{"
                  "\"name\": \"Left Line Sensor\","
                  "\"state_topic\": \"sensor/line/left\","
                  "\"payload_on\": \"BLACK\","
                  "\"payload_off\": \"WHITE\","
                  "\"unique_id\": \"train_controller_01_line_left\","
                  "\"device_class\": \"connectivity\","
                  "\"icon\": \"mdi:motion-sensor\","
                  "\"device\": "
                  + String(deviceInfo) + "}")
                   .c_str(),
                 true);

  // Line sensor right
  client.publish("homeassistant/binary_sensor/train_controller_01/line_right/config",
                 ("{"
                  "\"name\": \"Right Line Sensor\","
                  "\"state_topic\": \"sensor/line/right\","
                  "\"payload_on\": \"BLACK\","
                  "\"payload_off\": \"WHITE\","
                  "\"unique_id\": \"train_controller_01_line_right\","
                  "\"device_class\": \"connectivity\","
                  "\"icon\": \"mdi:motion-sensor\","
                  "\"device\": "
                  + String(deviceInfo) + "}")
                   .c_str(),
                 true);

  // Train control switch (Move Train)
  client.publish("homeassistant/switch/train_controller_01/move/config",
                 ("{"
                  "\"name\": \"Move Train\","
                  "\"command_topic\": \"motor/control\","
                  "\"state_topic\": \"motor/state\","
                  "\"unique_id\": \"train_controller_01_move\","
                  "\"payload_on\": \"START\","
                  "\"payload_off\": \"STOP\","
                  "\"icon\": \"mdi:train\","
                  "\"device\": "
                  + String(deviceInfo) + "}")
                   .c_str(),
                 true);

  client.publish("homeassistant/sensor/train_controller_01/ip/config",
                 ("{"
                  "\"name\": \"Train Controller IP\","
                  "\"state_topic\": \"train_controller_01/ip/state\","
                  "\"unique_id\": \"train_controller_01_ip\","
                  "\"icon\": \"mdi:ip-network\","
                  "\"device\": "
                  + String(deviceInfo) + "}")
                   .c_str(),
                 true);



  // Motor state sensor
  client.publish("homeassistant/sensor/train_controller_01/state/config",
                 ("{"
                  "\"name\": \"Train Motor State\","
                  "\"state_topic\": \"motor/state\","
                  "\"unique_id\": \"train_controller_01_state\","
                  "\"device_class\": \"enum\","
                  "\"payload_on\": \"START\","
                  "\"payload_off\": \"STOP\","
                  "\"icon\": \"mdi:engine\","
                  "\"device\": "
                  + String(deviceInfo) + "}")
                   .c_str(),
                 true);
}

void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("\nConnecting to WiFi Network..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.print(" Done\nIP: ");
  Serial.println(WiFi.localIP());
}

void publishSensorStates(int left, int right) {
  if (left != lastLeft) {
    client.publish("sensor/line/left", left == LOW ? "BLACK" : "WHITE", true);
    lastLeft = left;
  }
  if (right != lastRight) {
    client.publish("sensor/line/right", right == LOW ? "BLACK" : "WHITE", true);
    lastRight = right;
  }
}

void setMotorDirection(EMotorDirection direction) {
  if (direction == currentDirection) return;
  currentDirection = direction;

  if (direction == EMotorDirection::FORWARD) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    client.publish("motor/direction", "FORWARD", true);
  } else if (direction == EMotorDirection::REVERSE) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    client.publish("motor/direction", "REVERSE", true);
  } else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    client.publish("motor/direction", "STOPPED", true);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.print("MQTT Message [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(msg);

  if (msg == F("START")) {
    motorMode = EMotorMode::START;
    client.publish("motor/state", "START", true);
  } else if (msg == F("STOP")) {
    motorMode = EMotorMode::STOP;
    client.publish("motor/state", "STOP", true);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.setBufferSize(512)) {
      Serial.println("Buffer Size increased to 380 byte");
    } else {
      Serial.println("Failed to allocate larger buffer");
    }
    Serial.print("\nConnecting to MQTT server...");
    if (client.connect(deviceId, mqtt_user, mqtt_pass,
                       availabilityTopic, 1, true, "offline")) {
      Serial.println(" Done.");
      client.publish(availabilityTopic, "online", true);
      publishDiscoveryConfig();
      client.subscribe("motor/control");
      client.subscribe("homeassistant/status");
      IPAddress ip = WiFi.localIP();
      String ipStr = ip.toString();
      client.publish("train_controller_01/ip/state", ipStr.c_str(), true);
      client.publish("motor/state", "STOP", true);
      client.publish("motor/direction", "FORWARD", true);
      client.publish("sensor/line/left", "WHITE", true);
      client.publish("sensor/line/right", "WHITE", true);

    } else {
      Serial.println(" Failed.");
      Serial.println("Waiting 2 seconds...");
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(SENSOR_LEFT, INPUT);
  pinMode(SENSOR_RIGHT, INPUT);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) {  //http://esp32.local
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
  server.on(
    "/update", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart();
    },
    []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Update: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {  //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        /* flashing firmware to ESP*/
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {  //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
      }
    });
  server.begin();
}

void loop() {
  reconnect();
  client.loop();
  server.handleClient();

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
