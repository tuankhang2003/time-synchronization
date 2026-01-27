#include <WiFi.h>
#include "time.h"
#include <WiFiUdp.h>
#include <WString.h>
#include <ArduinoJson.h>
#include <esp_log.h>

#define PACKET_MAX_SIZE 256
const char *ssid = "tuankhang";
const char *password = "12345789";

WiFiUDP udp;
const int LOCAL_PORT = 12345;
int SLAVE_PORT;
int COMPUTER_PORT;
IPAddress SLAVE_IP;
IPAddress COMPUTER_IP;

enum class PTP_MSG_TYPE : uint8_t
{
  Sync = 1,
  Follow_Up = 2,
  Delay_Req = 3,
  Delay_Resp = 4,
  Log_Msg = 5,
  Start_PTP = 6,
};

struct PtpMsg
{
  PTP_MSG_TYPE type;
  int64_t time;
  // only when type = Log_Msg or Start_PTP
  String message;
};

void configureSlaveIP(String ipInfo);

void setup()
{
  esp_log_level_set("WiFiUdp", ESP_LOG_NONE);
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 MASTER OK");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println("Master IP: " + WiFi.localIP().toString());
  udp.begin(LOCAL_PORT);
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

void sendMsgToSlave(PtpMsg msg)
{
  sendMsg(msg, SLAVE_IP, SLAVE_PORT);
}
void sendLogMessageStruct(PtpMsg msg)
{
  sendMsg(msg, COMPUTER_IP, COMPUTER_PORT);
}

void sendLogMessage(String message)
{
  PtpMsg msg = {};
  msg.type = PTP_MSG_TYPE::Log_Msg;
  msg.message = message;
  sendMsg(msg, COMPUTER_IP, COMPUTER_PORT);
}

void sendMsgToSlaveAndLog(PtpMsg msg)
{
  sendMsgToSlave(msg);
  sendLogMessageStruct(msg);
}

void sendSync()
{
  PtpMsg syncMsg = {PTP_MSG_TYPE::Sync};
  sendMsgToSlave(syncMsg);
  sendLogMessage("Master sent SYNC to Slave");
}

void sendFollowUp()
{

  PtpMsg followUpMsg = {};
  followUpMsg.type = PTP_MSG_TYPE::Follow_Up;
  followUpMsg.time = esp_timer_get_time();
  String msg = "Master sent Follow-Up with t1=" + String(followUpMsg.time) + '\n';
  sendMsgToSlave(followUpMsg);
  sendLogMessage(msg);
  Serial.print(msg);
}

void sendDelayResponse()
{
  PtpMsg delayRespMsg = {};
  delayRespMsg.type = PTP_MSG_TYPE::Delay_Resp;
  delayRespMsg.time = esp_timer_get_time();
  sendMsgToSlave(delayRespMsg);
  String msg = "Master sent Delay Response with t3=" + String(delayRespMsg.time) + '\n';
  sendLogMessage(msg);
  Serial.print(msg);
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

void serverPTP()
{
  // sendLogMessage("Server send SYNC message to slave\n");
  // --- Send SYNC ---
  sendSync();

  // --- Send FOLLOW-UP ---

  sendFollowUp();
  // --- Wait for DELAY-REQ ---

  unsigned long start = millis();
  while (!udp.parsePacket())
  {
    if (millis() - start > 20000)
    {
      Serial.printf("Waiting for delayReqMsg failed\n");
      return;
    }
  }
  PtpMsg delayReqMsg = receivePtpMsg();
  // --- Send DELAY-RESP ---
  if (delayReqMsg.type == PTP_MSG_TYPE::Delay_Req)
  {
    sendLogMessage("Master received Delay Message from Slave\n");
    sendDelayResponse();
  }
  else
  {
    Serial.printf("False Type Message, expected Message Type Delay Request");
    return;
  }
  // Serial.printf("Master time: %lld us\n", esp_timer_get_time());
}

void loop()
{
  if (!udp.parsePacket())
    return;
  PtpMsg msg = receivePtpMsg();
  Serial.printf("Received Packet, type: %d:  %s\n", msg.type, msg.message.c_str());
  switch (msg.type)
  {
  case PTP_MSG_TYPE::Start_PTP:
  {
    COMPUTER_IP = udp.remoteIP();
    COMPUTER_PORT = udp.remotePort();
    Serial.println("Computer_ip: " + COMPUTER_IP.toString());
    sendLogMessage("Master start PTP Protocol\n");
    configureSlaveIP(msg.message);
    serverPTP();
    break;
  }
  default:
    return;
  }
}

void configureSlaveIP(String ipInfo)
{
  // Compute Slave IP and PORT IP
  // Format X.X.X.X:port
  String slaveAddressInfo = ipInfo;
  int colonIndex = slaveAddressInfo.indexOf(':');
  String ipStr = slaveAddressInfo.substring(0, colonIndex);
  String portStr = slaveAddressInfo.substring(colonIndex + 1);

  if (!SLAVE_IP.fromString(ipStr))
  {
    // invalid IP
    Serial.println("Invalid IP");
    return;
  }
  SLAVE_PORT = portStr.toInt();
}
