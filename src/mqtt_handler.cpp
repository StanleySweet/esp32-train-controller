#include "mqtt_handler.h"
#include "esp_mac.h" // esp_read_mac, ESP_MAC_WIFI_STA
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace mqtt {

// -------- DeviceInfo Implementation --------

std::string DeviceInfo::getMacAddress() {
  uint8_t mac[6];
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != 0) {
    throw std::runtime_error("Failed to read MAC address");
  }

  char mac_str[13]; // 12 chars + null terminator
  std::snprintf(mac_str, sizeof(mac_str), "%02X%02X%02X%02X%02X%02X", mac[0],
                mac[1], mac[2], mac[3], mac[4], mac[5]);

  return std::string(mac_str);
}

std::string DeviceInfo::getDeviceId() {
  return "train_controller_" + getMacAddress();
}

std::string DeviceInfo::getDeviceInfoJson(const std::string &deviceId) {
  char buffer[256];
  int len = std::snprintf(
      buffer, sizeof(buffer),
      R"({"identifiers":["%s"],"name":"Electric Train Controller","manufacturer":"Stan Industries","model":"ESP32-C3 Super Mini","sw_version":"1.0"})",
      deviceId.c_str());

  if (len < 0 || static_cast<size_t>(len) >= sizeof(buffer)) {
    throw std::runtime_error("Device JSON buffer too small");
  }

  return std::string(buffer);
}

// -------- DiscoveryPublisher Implementation --------

void DiscoveryPublisher::publishDiscoveryConfig(PubSubClient &client) {
  try {
    std::string deviceId = DeviceInfo::getDeviceId();
    std::string deviceJson = DeviceInfo::getDeviceInfoJson(deviceId);
    publishDirectionConfig(client, deviceId, deviceJson);
    publishLineSensorConfig(client, deviceId, deviceJson, "left");
    publishLineSensorConfig(client, deviceId, deviceJson, "right");
    publishTrainControlSwitch(client, deviceId, deviceJson);
    publishIPSensor(client, deviceId, deviceJson);
    publishMotorStateSensor(client, deviceId, deviceJson);
  } catch (...) {
    // Optionally log error
  }
}

void DiscoveryPublisher::publishDirectionConfig(PubSubClient &client,
                                                const std::string &deviceId,
                                                const std::string &deviceJson) {
  std::string topic = "homeassistant/sensor/" + deviceId + "/direction/config";

  std::string payload =
      std::string("{") + "\"name\": \"Train Motor Direction\"," +
      "\"state_topic\": \"" + deviceId + "/motor_direction/state\"," +
      "\"unique_id\": \"" + deviceId + "_direction\"," +
      "\"device_class\": \"enum\"," + "\"icon\": \"mdi:arrow-left-right\"," +
      "\"device\": " + deviceJson + "}";

  client.publish(topic.c_str(), payload.c_str(), true);
}

void DiscoveryPublisher::publishLineSensorConfig(PubSubClient &client,
                                                 const std::string &deviceId,
                                                 const std::string &deviceJson,
                                                 const std::string &side) {
  std::string topic =
      "homeassistant/binary_sensor/" + deviceId + "/line_" + side + "_sensor/config";
  std::string name = std::string(1, std::toupper(side[0])) + side.substr(1);
  std::string payload = "{\"name\": \"" + name +
                        " Line Sensor\","
                        "\"state_topic\": \"" +
                        deviceId + "/line_" + side +
                        "_sensor/state\","
                        "\"payload_on\": \"BLACK\","
                        "\"payload_off\": \"WHITE\","
                        "\"unique_id\": \"" +
                        deviceId + "_line_" + side +
                        "_sensor\","
                        "\"device_class\": \"motion\","
                        "\"icon\": \"mdi:motion-sensor\","
                        "\"device\": " +
                        deviceJson + "}";

  client.publish(topic.c_str(), payload.c_str(), true);
}

void DiscoveryPublisher::publishTrainControlSwitch(
    PubSubClient &client, const std::string &deviceId,
    const std::string &deviceJson) {
  std::string topic = "homeassistant/switch/" + deviceId + "/move/config";
  std::string payload = "{"
                        "\"name\": \"Move Train\","
                        "\"command_topic\": \"" +
                        deviceId +
                        "/motor/set\","
                        "\"state_topic\": \"" +
                        deviceId +
                        "/motor/state\","
                        "\"unique_id\": \"" +
                        deviceId +
                        "_move\","
                        "\"payload_on\": \"START\","
                        "\"payload_off\": \"STOP\","
                        "\"icon\": \"mdi:train\","
                        "\"device\": " +
                        deviceJson + "}";

  client.publish(topic.c_str(), payload.c_str(), true);
}

void DiscoveryPublisher::publishIPSensor(PubSubClient &client,
                                         const std::string &deviceId,
                                         const std::string &deviceJson) {
  std::string topic = "homeassistant/sensor/" + deviceId + "/ip/config";
  std::string payload = "{"
                        "\"name\": \"Train Controller IP\","
                        "\"state_topic\": \"" +
                        deviceId +
                        "/ip/state\","
                        "\"unique_id\": \"" +
                        deviceId +
                        "_ip\","
                        "\"icon\": \"mdi:ip-network\","
                        "\"device\": " +
                        deviceJson + "}";

  client.publish(topic.c_str(), payload.c_str(), true);
}

void DiscoveryPublisher::publishMotorStateSensor(
    PubSubClient &client, const std::string &deviceId,
    const std::string &deviceJson) {
  std::string topic = "homeassistant/sensor/" + deviceId + "/state/config";
  std::string payload = "{"
                        "\"name\": \"Train Motor State\","
                        "\"state_topic\": \"" +
                        deviceId +
                        "/motor/state\","
                        "\"unique_id\": \"" +
                        deviceId +
                        "_state\","
                        "\"device_class\": \"enum\","
                        "\"payload_on\": \"START\","
                        "\"payload_off\": \"STOP\","
                        "\"icon\": \"mdi:engine\","
                        "\"device\": " +
                        deviceJson + "}";

  client.publish(topic.c_str(), payload.c_str(), true);
}

} // namespace mqtt
