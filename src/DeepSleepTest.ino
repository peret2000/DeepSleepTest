#include <ArduinoOTA.h>
#include <Homie.h>
#include <NtpClientLib.h>

#include <OneWire.h>
#include <DallasTemperature.h>

//ADC_MODE(ADC_VCC);


// Homie project: https://github.com/marvinroger/homie-esp8266
// NTP Client based on https://github.com/gmag11/NtpClient


#define FW_NAME "HomieSensor_T_6_20180604"	//	Max length: 32. Be careful: if it is exceeded thare is a crash
#define FW_VERSION "2.0.19"	//	Max Length: 16. Be careful: if it is exceeded thare is a crash


/* Magic sequence for Autodetectable Binary Upload */
//const char *__FLAGGED_FW_NAME = "\xbf\x84\xe4\x13\x54" FW_NAME "\x93\x44\x6b\xa7\x75";
//const char *__FLAGGED_FW_VERSION = "\x6a\x3f\x3e\x0e\xe1" FW_VERSION "\xb0\x30\x48\xd4\x1a";
/* End of magic sequence for Autodetectable Binary Upload */


//------------------------------------------
//OTA


boolean otaReady = false;

void SetupOTA()
{
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);
  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");
  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");
  ArduinoOTA.onStart([]() {
    Serial.println("[OTA] Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  otaReady = true;

  Serial.println("OTA started...");
}

//------------------------------------------


unsigned long lastLoopMillis = 0;
unsigned long lastLooNotSynced = 0;
boolean haveWifi = true;
unsigned long lastWifiDisconnected = 0;


//------------------------------------------
//HOMIE

//Declaration of setting variables
HomieSetting<long> timeInterval("interval", "Interval, in seconds, to fetch temperature. Default is 60 seconds");
HomieSetting<long> deepSleep("deep_sleep", "Wheter it goes to deep sleep mode. Defaul: 0 (no deep sleep. Any positive value: Time in sleep mode, in seconds");
HomieSetting<long> sleepMaxAwake("sleep_maxawake", "Max time awake, in seconds. If nothing to do, it goes to sleep. Only used if deep_sleep > 0. Default is 60 seconds");
HomieSetting<long> sleepNoWifi("sleep_nowifi", "After that time, in seconds, without Wifi, it goes to sleep. Only used if deep_sleep > 0, Default is 30 seconds");


HomieNode temperatureNode("temp", "Temp ºC");



//------------------------------------------
// NTP Client

boolean ntpSynced = false;
boolean firstNTPSync = true;


void SetupNTP()
{

  ntpSynced = false;

  NTP.onNTPSyncEvent([](NTPSyncEvent_t event) {
    Serial << "Trying to sync NTP Server ...........";
	temperatureNode.setProperty("comment").send(String("Trying to sync NTP Server ..........."));
    if (event == timeSyncd) {
    //if (!event) {
      Serial << "Synced!" << endl;
	temperatureNode.setProperty("comment").send(String("Synced!"));
      ntpSynced = true;

        // Only on first sync. Later, it crashes
      if (firstNTPSync) {
        temperatureNode.setProperty("uptime").send(NTP.getTimeDateString(NTP.getFirstSync()).c_str());
        Homie.getLogger() << ". Uptime: " << NTP.getTimeDateString(NTP.getFirstSync()).c_str() << endl;
        //temperatureNode.setProperty("uptime").send(NTP.getUptimeString());
        //Homie.getLogger() << ". Uptime: " << NTP.getUptimeString() << endl;
        firstNTPSync = false;
      }

    }
    else {
      if (event == noResponse)
        temperatureNode.setProperty("comment").send(String("Not yet: No Response"));
      else if (event == invalidAddress)
        temperatureNode.setProperty("comment").send(String("Not yet: Address not reachable"));
      else
        temperatureNode.setProperty("comment").send(String("Not yet: Unknown error"));
      Serial << "Not yet." << endl;
      ntpSynced = false;
    }

  });

  NTP.begin("3.es.pool.ntp.org", 1, true);
  //NTP.begin("0.europe.pool.ntp.org", 1, true);
 
}


//------------------------------------------
//DS18B20

#define ONE_WIRE_BUS D2 //Pin to which is attached a temperature sensor
#define ONE_WIRE_MAX_DEV 15 //The maximum number of devices


OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
int numberOfDevices; //Number of temperature devices found

DeviceAddress devAddr[ONE_WIRE_MAX_DEV];  //An array device temperature sensors

//Convert device id to String
String GetAddressToString(DeviceAddress deviceAddress){
  String str = "";
  for (uint8_t i = 0; i < 8; i++){
    if( deviceAddress[i] < 16 ) str += String(0, HEX);
    str += String(deviceAddress[i], HEX);
  }
  return str;
}


//------------------------------------------
//HOMIE


void onHomieEvent(const HomieEvent& event) {
  switch (event.type) {
    case HomieEventType::STANDALONE_MODE:
      Serial << "Standalone mode started" << endl;
      break;
    case HomieEventType::CONFIGURATION_MODE:
      Serial << "Configuration mode started" << endl;
      break;
    case HomieEventType::NORMAL_MODE:
      Serial << "Normal mode started" << endl;
      Serial << "Name: " << Homie.getConfiguration().name << ".Device: " << Homie.getConfiguration().deviceId << endl;
      break;
    case HomieEventType::OTA_STARTED:
      Serial << "OTA started" << endl;
      break;
    case HomieEventType::OTA_FAILED:
      Serial << "OTA failed" << endl;
      break;
    case HomieEventType::OTA_SUCCESSFUL:
      Serial << "OTA successful" << endl;
      break;
    case HomieEventType::ABOUT_TO_RESET:
      Serial << "About to reset" << endl;
      break;
    case HomieEventType::WIFI_CONNECTED:
      Serial << "Wi-Fi connected, IP: " << event.ip << ", gateway: " << event.gateway << ", mask: " << event.mask << endl;
      Serial << "SSID: " << Homie.getConfiguration().wifi.ssid << endl;
      haveWifi = true;
      SetupNTP();
      SetupOTA();
      break;
    case HomieEventType::WIFI_DISCONNECTED:
      Serial << "Wi-Fi disconnected, reason: " << (int8_t)event.wifiReason << endl;
      if (haveWifi) //  Initial situation, or after a wifi cut
      {
        haveWifi = false;
        lastWifiDisconnected = millis();
      }
      if (millis() - lastWifiDisconnected >= sleepNoWifi.get() * 1000UL) //  Too many seconds without Wifi -> Go to Sleep
        {
          Serial << "No Wifi for " << sleepNoWifi.get() << " seconds; I should go to deep sleep.................................................." << endl;
          haveWifi = true;  //  Not necessary, to avoid the same message again, before another minute
          if (deepSleep.get() > 0)
          {
            Homie.prepareToSleep();
            //ESP.deepSleep(deepSleep.get() * 1000000UL, WAKE_RF_DEFAULT);
          }   
        }

      ntpSynced = false;
      break;
    case HomieEventType::MQTT_READY:
      Serial << "MQTT Connected" << endl;
      Serial << "Topic: " << Homie.getConfiguration().mqtt.baseTopic << endl;
      break;
    case HomieEventType::MQTT_DISCONNECTED:
      Serial << "MQTT disconnected, reason: " << (int8_t)event.mqttReason << endl;
      break;
    case HomieEventType::MQTT_PACKET_ACKNOWLEDGED:
    //  Serial << "MQTT packet acknowledged, packetId: " << event.packetId << endl;
      break;
    case HomieEventType::READY_TO_SLEEP:
      Serial << "I'm Ready to sleep for " << deepSleep.get() << " seconds." << endl;
      ESP.deepSleep(deepSleep.get() * 1000000UL, WAKE_RF_DEFAULT);
      break;
  }
}


bool broadcastHandler(const String& level, const String& value) {
  Serial << "Received broadcast level " << level << ": " << value << endl;

  if (level=="1" && value=="reboot")
    {
      Serial << "Rebooting ..." << endl;
      Homie.reboot();
    }

  
  return true;
}



void setupHandler() {
  temperatureNode.setProperty("unit").send("c");
}


void loopHandler() {

  if (!ntpSynced) {
    if (millis() - lastLooNotSynced >= 1000UL || lastLooNotSynced == 0)
      {
      String currtime = NTP.getTimeStr();
      lastLooNotSynced = millis();
      //temperatureNode.setProperty("comment").send(String("Not Synced"));
      }
  }
  else {
    //temperatureNode.setProperty("comment").send(String("Synced!"));
    if (millis() - lastLoopMillis >= timeInterval.get() * 1000UL || lastLoopMillis == 0)
      {
        for(int i=0; i<numberOfDevices; i++)
        {
            float temperature = DS18B20.getTempC( devAddr[i] ); //Measuring temperature in Celsius
            temperatureNode.setProperty("degrees").send(String(temperature));
            Homie.getLogger() << "Temperature: " << temperature << " °C";
        }
    
        temperatureNode.setProperty("time").send(NTP.getTimeStr());
        Homie.getLogger() << ". Time: " << NTP.getTimeStr() << endl;
        //temperatureNode.setProperty("voltage").send(String(ESP.getVcc()));
        //Homie.getLogger() << " Voltage: " << ESP.getVcc() << endl;

        Serial << "Now that I have sent the figures, I could go to deep sleep.................................................." << endl;
        if (deepSleep.get() > 0)
        {
          Homie.prepareToSleep();
        }
        
        DS18B20.setWaitForConversion(false); //No waiting for measurement
        DS18B20.requestTemperatures(); //Initiate the temperature measurement
        
        lastLoopMillis = millis();
      }
  }

}


//Setting the temperature sensor

void SetupDS18B20(){
  DS18B20.begin();

  Serial.print("Parasite power is: "); 
  if( DS18B20.isParasitePowerMode() ){ 
    Serial.println("ON");
  }else{
    Serial.println("OFF");
  }
  
  numberOfDevices = DS18B20.getDeviceCount();
  Serial.print( "Device count: " );
  Serial.println( numberOfDevices );

  //lastTemp = millis();
  DS18B20.requestTemperatures();

  // Loop through each device, print out address
  for(int i=0;i<numberOfDevices; i++){
    // Search the wire for address
    if( DS18B20.getAddress(devAddr[i], i) ){
      //devAddr[i] = tempDeviceAddress;
      Serial.print("Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: " + GetAddressToString(devAddr[i]));
      Serial.println();
    }else{
      Serial.print("Found ghost device at ");
      Serial.print(i, DEC);
      Serial.print(" but could not detect address. Check power and cabling");
    }

    //Get resolution of DS18b20
    Serial.print("Resolution: ");
    Serial.print(DS18B20.getResolution( devAddr[i] ));
    Serial.println();

    //Read temperature from DS18b20
    float tempC = DS18B20.getTempC( devAddr[i] );
    Serial.print("Temp C: ");
    Serial.println(tempC);
  }
}


//-----------------------------------------------------------------
//      MQTT Subscribe

void onMqttConnect(bool sessionPresent)
{
  AsyncMqttClient& mqttClient = Homie.getMqttClient();
  uint16_t packetIdSub = mqttClient.subscribe("homie/terraza/temp/time", 2);
  uint16_t packetIdSub2 = mqttClient.subscribe("homie/terraza/temp/degrees", 2);
  Serial << "[MQTT onMqttConnect] Subscribed with packet ID: " << packetIdSub << endl;
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{

  Serial << "[MQTT onMqttMessage]: " << topic << ":" << ((String)payload).substring(0,len) << endl;
  Serial << "[MQTT onMqttMessage] Free size: " << ESP.getFreeHeap() << endl;
}

//-----------------------------------------------------------------


void setup() {


  Serial.begin(9600);
  Serial << endl << endl;

  Serial << "Sketch size: " << ESP.getSketchSize() << endl;
  Serial << "Free size: " << ESP.getFreeSketchSpace() << endl;

  //Serial << "Voltage: " << ESP.getVcc() << endl;

  Homie_setFirmware(FW_NAME, FW_VERSION);
  Homie_setBrand("myhomie")

  // Settings
  
  timeInterval.setDefaultValue(60).setValidator([] (long candidate) {return (candidate > 0) && (candidate < 10000);});
  deepSleep.setDefaultValue(0).setValidator([] (long candidate) {return (candidate >= 0) && (candidate < 10000);});
  sleepMaxAwake.setDefaultValue(60).setValidator([] (long candidate) {return (candidate >= 0) && (candidate < 10000);});
  sleepNoWifi.setDefaultValue(30).setValidator([] (long candidate) {return (candidate >= 0) && (candidate < 10000);});

  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);
  Homie.onEvent(onHomieEvent);

  Homie.setBroadcastHandler(broadcastHandler);

  Homie.setLedPin(5, LOW);

  Homie.setup();

  //Setup DS18b20 temperature sensor
  SetupDS18B20();

  // Set up mqtt client to subscribe to another device's messages
  AsyncMqttClient& mqttClient = Homie.getMqttClient();
  mqttClient.onConnect(onMqttConnect);

  mqttClient.onMessage(onMqttMessage);

}

void loop() {
    // In case there is no events and Homie.loop() is not called yet. For example, in case there is wifi but there
    // is not internet access, so MQTT server is not reached
  if (deepSleep.get() > 0 && millis() > sleepMaxAwake.get() * 1000UL)
  {
    Serial << "Too much time awake. Going to sleep!" << endl;
    Homie.prepareToSleep();
  }
  if (otaReady) {
    ArduinoOTA.handle();
  }
  Homie.loop();
}


