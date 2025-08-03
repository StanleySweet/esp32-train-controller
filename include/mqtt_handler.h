#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <PubSubClient.h>
#include <string>

namespace mqtt {

class DeviceInfo {
public:
  static std::string getMacAddress();
  static std::string getDeviceId();
  static std::string getDeviceInfoJson(const std::string &deviceId);
};

class DiscoveryPublisher {
public:
  static void publishDiscoveryConfig(PubSubClient &client);

private:
  static void publishDirectionConfig(PubSubClient &client,
                                     const std::string &deviceId,
                                     const std::string &deviceJson);
  static void publishLineSensorConfig(PubSubClient &client,
                                      const std::string &deviceId,
                                      const std::string &deviceJson,
                                      const std::string &side);
  static void publishTrainControlSwitch(PubSubClient &client,
                                        const std::string &deviceId,
                                        const std::string &deviceJson);
  static void publishIPSensor(PubSubClient &client, const std::string &deviceId,
                              const std::string &deviceJson);
  static void publishMotorStateSensor(PubSubClient &client,
                                      const std::string &deviceId,
                                      const std::string &deviceJson);
};

} // namespace mqtt

#endif // MQTT_HANDLER_H
