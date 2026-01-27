#include <Arduino.h>
#include <WiFi.h>
#include "time.h"
#include <WiFiUdp.h>
#include <WString.h>
#include <ArduinoJson.h>
#include <esp_log.h>

#define PACKET_MAX_SIZE 256

const char *ssid = "XauXiHouse";
const char *password = "HoSt26Wo03HiwoTr&PaNg";
// const char *ssid = "tuankhang";
// const char *password = "12345789";

WiFiUDP udp;
const int LOCAL_PORT = 12346;
int MASTER_PORT;
int COMPUTER_PORT;
IPAddress MASTER_IP;
IPAddress COMPUTER_IP;

enum class PTP_MSG_TYPE : uint8_t
{
  Sync = 1,
  Follow_Up = 2,
  Delay_Req = 3,
  Delay_Resp = 4,
  Log_Msg = 5,
  Start_PTP = 6
};

struct PtpMsg
{
  PTP_MSG_TYPE type;
  int64_t time;
  String message;
};

void sendDelayRequest();
void sendMsg(PtpMsg msg, IPAddress destIp, int destPort);
void sendLogMessage(String message);
void configureMasterIP(String ipInfo);

void clientPTP();
PtpMsg receivePtpMsg();

void loop()
{

  // If no packet -> return
  if (!udp.parsePacket())
    return;

  clientPTP();
}

void clientPTP()
{
  PtpMsg msg = receivePtpMsg();
  int64_t t1 = -1;
  int64_t t2 = -1;
  // Receive Sync
  switch (msg.type)
  {
  case PTP_MSG_TYPE::Sync:
  {
    // Record t2
    t2 = esp_timer_get_time();
    sendLogMessage("Slave recorded current time t2 = " + String(t2) + "\n");
    // MASTER_IP = udp.remoteIP();
    // MASTER_PORT = udp.remotePort();
    Serial.println("In sync, MASTER_IP found" + MASTER_IP.toString());
    Serial.printf("Received sync message\n");
    sendLogMessage("Slave received SYNC message from master\n");
    break;
  }
  case PTP_MSG_TYPE::Follow_Up:
  {
    t1 = msg.time;
    // Send delay Requests
    sendLogMessage("Slave received FOLLOW_UP message from master t1=" + String(t1) + "\n");
    Serial.printf("Received follow_Up message, t1= %llu\n", t1);
    sendDelayRequest();
    break;
  }
  case PTP_MSG_TYPE::Delay_Resp:
  {
    int64_t t3 = msg.time;
    sendLogMessage("Slave received Delay Response from Master t3=" + String(t3) + "\n");
    int64_t t4 = esp_timer_get_time();
    sendLogMessage("Slave has current time t4=" + String(t4));
    sendLogMessage("Slave compute offset = ((t2 - t1) + (t3 - t4)) / 2\n");
    sendLogMessage("Slave start to synchronize time\n");
    int64_t offset = ((t2 - t1) + (t3 - t4)) / 2;
    int64_t corrected_time = esp_timer_get_time() + offset;
    String local_time = "Local time:     " + String(esp_timer_get_time()) + '\n';
    String corrected_time_str = "Corrected time: " + String(corrected_time) + "\n";
    String offset_str = "Offset:              " + String(offset) + "\n";

    Serial.print(local_time);
    Serial.print(corrected_time_str);
    Serial.print(offset_str);

    sendLogMessage(local_time);
    sendLogMessage(corrected_time_str);
    sendLogMessage(offset_str);

    break;
  }
  case PTP_MSG_TYPE::Start_PTP:
  {
    // Read computer IP
    COMPUTER_IP = udp.remoteIP();
    COMPUTER_PORT = udp.remotePort();
    Serial.println("Computer IP: " + COMPUTER_IP.toString());
    Serial.println("Received Start PTP");
    configureMasterIP(msg.message);
    sendLogMessage("Slave is ready for PTP Protocol");
    break;
  }
  default:
    break;
  }
}

void setup()
{
  esp_log_level_set("*", ESP_LOG_NONE);
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 SLAVE OK");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println(String("SLAVE_IP:") + WiFi.localIP().toString());
  udp.begin(LOCAL_PORT);
}

void sendDelayRequest()
{
  Serial.println("Slave start to send Delay Request\n");
  PtpMsg delayReqMsg = {PTP_MSG_TYPE::Delay_Req, esp_timer_get_time()};
  Serial.println("Master IP: " + MASTER_IP.toString());
  sendMsg(delayReqMsg, MASTER_IP, MASTER_PORT);
  sendLogMessage("Slave sent Delay Request to Master\n");
}

PtpMsg receivePtpMsg()
{
  if (udp.parsePacket())
  {
    char incomingPacket[PACKET_MAX_SIZE];

    int len = udp.read(incomingPacket, sizeof(incomingPacket) - 1);
    if (len > 0)
    {
      incomingPacket[len] = '\0'; // null-terminate
    }
    Serial.println("Received Packet" + String(incomingPacket));
    // Parse JSON
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, incomingPacket);

    if (!error)
    {
      uint8_t type = doc["type"];
      int64_t time = doc["time"];
      const char *message = doc["message"];
      return PtpMsg{static_cast<PTP_MSG_TYPE>(type), time, String(message)};
    }
    else
    {
      Serial.print("Failed to parse JSON: ");
      Serial.println(error.c_str());
    }
  }
}

void sendMsg(PtpMsg msg, IPAddress destIp, int destPort)
{
  StaticJsonDocument<256> doc;
  doc["type"] = (uint8_t)msg.type;
  doc["time"] = msg.time;
  doc["message"] = msg.message;
  char buffer[256];
  size_t n = serializeJson(doc, buffer, sizeof(buffer));
  udp.beginPacket(destIp, destPort);
  udp.write((uint8_t *)&buffer, n);
  udp.endPacket();
}

void sendMsgToMaster(PtpMsg msg)
{
  sendMsg(msg, MASTER_IP, MASTER_IP);
}

void sendLogMessage(String message)
{
  PtpMsg msg = {};
  msg.type = PTP_MSG_TYPE::Log_Msg;
  msg.message = message;
  sendMsg(msg, COMPUTER_IP, COMPUTER_PORT);
}

void configureMasterIP(String ipInfo)
{
  // Compute Slave IP and PORT IP
  // Format X.X.X.X:port
  String slaveAddressInfo = ipInfo;
  int colonIndex = slaveAddressInfo.indexOf(':');
  String ipStr = slaveAddressInfo.substring(0, colonIndex);
  String portStr = slaveAddressInfo.substring(colonIndex + 1);

  if (!MASTER_IP.fromString(ipStr))
  {
    // invalid IP
    Serial.println("Invalid IP");
    return;
  }
  MASTER_PORT = portStr.toInt();
}