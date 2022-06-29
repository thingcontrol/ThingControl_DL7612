/*############################ An example send data via LoRa (P2P) to LoRa Bridge (LoRa (P2P)) ##########################
   Using thingcontrol board V 1.0 read ambient temperature and humidity values from an XY-MD02 sensor via Modbus RTU send
   to LoRa Bridge (LoRa (P2P)) by LoRa shield (thingcontrol)
  ########################################################################################################################*/

#include <HardwareSerial.h>
#include <ThingControl_DL7612.h>
#include "REG_CONFIG.h"

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <Wire.h>

ThingControl_DL7612 lora;
String device_ID, payload01, token01;
String devcode = "0673";
String tempcode = "0267";
String humicode = "0768";
struct dataDecode
{
  String device_ID;
  String temp;
  String hum;
};

dataDecode datadec;

#define WIFI_AP ""
#define WIFI_PASSWORD ""

String deviceToken = "OLE2aFuGXBEyGJyhfBEn";

char thingsboardServer[] = "mqtt.thingcontrol.io";

String json = "";

WiFiClientSecure wifiClient;
PubSubClient client(wifiClient);

int status = WL_IDLE_STATUS;
int PORT = 8883;

void setup() {
  Serial.begin(115200);
  device_ID = lora.getDevEui();
  device_ID = device_ID.substring(12, 16);

  Serial.println(F("Starting... LoRa Bridge"));
  Serial.print("Device ID  : ");
  Serial.println(device_ID);
  Serial.println(F("***********************************"));

  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);

  if (!wifiManager.autoConnect("@Thingcontrol_AP8")) {
    Serial.println("failed to connect and hit timeout");
    delay(1000);
  }
  client.setServer( thingsboardServer, PORT );
  reconnectMqtt();
}

void loop() {
  status = WiFi.status();
  if ( status == WL_CONNECTED)
  {
    if ( !client.connected() )
    {
      reconnectMqtt();
    }
    client.loop();
  }
  sendSensor();
  delay(3000);
}

String convert2Hex(String tempdata) {
  unsigned long datatemp;
  int dataLength;
  String sensorData;
  datatemp = tempdata.toInt();
  sensorData = String(datatemp, HEX);
  dataLength = sensorData.length();
  if (dataLength == 3) {
    sensorData = "0" + sensorData;
  } else if (dataLength == 2) {
    sensorData = "00" + sensorData;
  } else if (dataLength == 1) {
    sensorData = "000" + sensorData;
  } else  if (sensorData == 0) {
    sensorData = "0000";
  }
  return sensorData;
}

void sendSensor() {
  /* Decode
    case 0x0673:// Device ID (4 digit of DEVEUI)  2 Byte
    case 0x0267:// Temperature (0-100)  2 Byte
    case 0x0768:// Humidity (0-100)  2 Byte

    example data
    sensorData = "067316CE026700FA07680171";
                  0673 16ce 0267 00fa 0768 0171
    sensorData = '0673' + device_ID.substring(12, 16) + '0267' + sensor.temperature + '0768' + sensor.moisture
  */
  int dataLength;
  String sensorData;
  sensorData = device_ID + "0000";
  dataLength = sensorData.length();
  payload01 = lora.sendp2pHexData(dataLength / 2 , sensorData);
  if (payload01 != ("N/A")) {
    byte index = payload01.indexOf(F(","));
    payload01 = payload01.substring(index + 1, payload01.length());
    Serial.print("Receive Payload : ");
    Serial.println(payload01);
    sensorDataDecode(payload01, devcode);
    sensorDataDecode(payload01, tempcode);
    sensorDataDecode(payload01, humicode);
    sendtelemetry();
  }
}

// decode Hex sensor string data to object
void sensorDataDecode(String hexStr, String code) {
  String data_str = hexStr;
  String code_str = code;
  String data_sep;
  byte index;
  index = data_str.indexOf(code_str);
  data_sep = data_str.substring(index + 4, index + 8);

  switch (code_str.toInt()) {
    case 673:
      datadec.device_ID = data_sep;
      break;
    case 267:
      datadec.temp = data_sep;
      break;
    case 768:
      datadec.hum = data_sep;
      break;
    default:
      break;
  }
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void setWiFi()
{
  Serial.println("OK");
  Serial.println("+:Reconfig WiFi  Restart...");
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  if (!wifiManager.startConfigPortal("ThingControlCommand"))
  {
    Serial.println("failed to connect and hit timeout");
    delay(5000);
  }
  Serial.println("OK");
  Serial.println("+:WiFi Connect");
  client.setServer( thingsboardServer, PORT );  // secure port over TLS 1.2
  reconnectMqtt();
}

void processTele(char jsonTele[])
{
  char *aString = jsonTele;
  Serial.println("OK");
  Serial.print(F("+:topic v1/devices/me/ , "));
  Serial.println(aString);
  client.publish( "v1/devices/me/telemetry", aString);
}

void reconnectMqtt()
{
  if ( client.connect("Thingcontrol_AT", deviceToken.c_str(), NULL) )
  {
    Serial.println( F("Connect MQTT Success."));
    client.subscribe("v1/devices/me/rpc/request/+");
  }
}

void sendtelemetry()
{
  String json = "";
  json.concat("{\"device_id\":\"");
  json.concat(datadec.device_ID);
  json.concat("\",\"temp\":\"");
  json.concat(datadec.temp);
  json.concat("\",\"hum\":\"");
  json.concat(datadec.hum);
  json.concat("\"}");
  Serial.println(json);
  // Length (with one extra character for the null terminator)
  int str_len = json.length() + 1;
  // Prepare the character array (the buffer)
  char char_array[str_len];
  // Copy it over
  json.toCharArray(char_array, str_len);
  processTele(char_array);
}
