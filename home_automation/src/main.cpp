#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>
#include <driver/i2s.h>
#include <esp_task_wdt.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "config.h"

// Relay State - To remember the toggle state for relay
bool relayState1 = LOW;
bool relayState2 = LOW;
bool relayState3 = LOW;

// Switch State
bool switchState1 = LOW;
bool switchState2 = LOW;
bool switchState3 = LOW;

int gasSensorVal;
bool blinkLed = false;

// Voice command
bool toBeUpdated = false;
const char *device_name;
const char *trait_value;

BlynkTimer timer;

File file;
const char filename[] = "/recording.wav";
const int headerSize = 44;

void wavHeader(byte* header, int wavSize)
{
  header[0] = 'R';
  header[1] = 'I';
  header[2] = 'F';
  header[3] = 'F';
  unsigned int fileSize = wavSize + headerSize - 8;
  header[4] = (byte)(fileSize & 0xFF);
  header[5] = (byte)((fileSize >> 8) & 0xFF);
  header[6] = (byte)((fileSize >> 16) & 0xFF);
  header[7] = (byte)((fileSize >> 24) & 0xFF);
  header[8] = 'W';
  header[9] = 'A';
  header[10] = 'V';
  header[11] = 'E';
  header[12] = 'f';
  header[13] = 'm';
  header[14] = 't';
  header[15] = ' ';
  header[16] = 0x10;
  header[17] = 0x00;
  header[18] = 0x00;
  header[19] = 0x00;
  header[20] = 0x01;
  header[21] = 0x00;
  header[22] = 0x01;
  header[23] = 0x00;
  header[24] = 0x80;
  header[25] = 0x3E;
  header[26] = 0x00;
  header[27] = 0x00;
  header[28] = 0x00;
  header[29] = 0x7D;
  header[30] = 0x00;
  header[31] = 0x00;
  header[32] = 0x02;
  header[33] = 0x00;
  header[34] = 0x10;
  header[35] = 0x00;
  header[36] = 'd';
  header[37] = 'a';
  header[38] = 't';
  header[39] = 'a';
  header[40] = (byte)(wavSize & 0xFF);
  header[41] = (byte)((wavSize >> 8) & 0xFF);
  header[42] = (byte)((wavSize >> 16) & 0xFF);
  header[43] = (byte)((wavSize >> 24) & 0xFF);
}

void listSPIFFS(void)
{
  Serial.println(F("\r\nListing SPIFFS files:"));
  static const char line[] PROGMEM =  "=================================================";

  Serial.println(FPSTR(line));
  Serial.println(F("  File name                              Size"));
  Serial.println(FPSTR(line));

  fs::File root = SPIFFS.open("/");
  if (!root) {
    Serial.println(F("Failed to open directory"));
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(F("Not a directory"));
    return;
  }

  fs::File file = root.openNextFile();
  while (file) {

    if (file.isDirectory()) {
      Serial.print("DIR : ");
      String fileName = file.name();
      Serial.print(fileName);
    } else {
      String fileName = file.name();
      Serial.print("  " + fileName);
      // File path can be 31 characters maximum in SPIFFS
      int spaces = 33 - fileName.length(); // Tabulate nicely
      if (spaces < 1) spaces = 1;
      while (spaces--) Serial.print(" ");
      String fileSize = (String) file.size();
      spaces = 10 - fileSize.length(); // Tabulate nicely
      if (spaces < 1) spaces = 1;
      while (spaces--) Serial.print(" ");
      Serial.println(fileSize + " bytes");
    }

    file = root.openNextFile();
  }

  Serial.println(FPSTR(line));
  Serial.println();
  delay(1000);
}

void SPIFFSInit()
{
  if(!SPIFFS.begin(true)){
    Serial.println("SPIFFS initialisation failed!");
    while(1) yield();
  }
}

void i2sInit()
{
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = i2s_bits_per_sample_t(I2S_SAMPLE_BITS),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = 0,
    .dma_buf_count = 64,
    .dma_buf_len = 1024,
    .use_apll = 1
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);

  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };

  i2s_set_pin(I2S_PORT, &pin_config);
}

void voiceController(float& intent_confidence, const char *device_name, float& device_confidence, const char *trait_value, float& trait_confidence) {
    if (intent_confidence < 0.7)
    {
        Serial.printf("Only %.f%% certain on intent\n", 100 * intent_confidence);
        return;
    }
    if (strcmp(device_name, "red") != 0 && strcmp(device_name, "green") != 0 && strcmp(device_name, "blue") != 0)
    {
        Serial.println("Device not found");
        return;
    }
    if (device_confidence < 0.7)
    {
        Serial.printf("Only %.f%% certain on device\n", 100 * device_confidence);
        return;
    }
    if (strcmp(trait_value, "on") != 0 && strcmp(trait_value, "off") != 0)
    {
        Serial.println("Can't work out the intent action");
        return;
    }
    if (trait_confidence < 0.7)
    {
        Serial.printf("Only %.f%% certain on trait\n", 100 * trait_confidence);
        return;
    }

    toBeUpdated = true;
}

void uploadFile(){
  file = SPIFFS.open(filename, FILE_READ);
  if(!file){
    Serial.println("FILE IS NOT AVAILABLE!");
    return;
  }

  Serial.println("===> Upload FILE to Node.js Server");

  HTTPClient client;
  client.begin("http://192.168.110.103:8888/uploadCommand");
  client.addHeader("Content-Type", "audio/wav");
  int httpResponseCode = client.sendRequest("POST", &file, file.size());
  Serial.print("httpResponseCode : ");
  Serial.println(httpResponseCode);

  if(httpResponseCode == 200){
    String response = client.getString();
    StaticJsonDocument<500> doc;
    deserializeJson(doc, response);

    const char* text = doc["text"];
    const char *intent_name = doc["intent_name"];
    float intent_confidence = doc["intent_confidence"];
    device_name = doc["device_name"];
    float device_confidence = doc["device_confidence"];
    trait_value = doc["trait_value"];
    float trait_confidence = doc["trait_confidence"];

    Serial.println("==================== Transcription ====================");
    Serial.println(text);
    Serial.print("Intent: ");
    Serial.println(intent_name);
    Serial.print("Intent Confidence: ");
    Serial.println(intent_confidence);
    Serial.print("Device Name: ");
    Serial.println(device_name);
    Serial.print("Device Confidence: ");
    Serial.println(device_confidence);
    Serial.print("Trait Value: ");
    Serial.println(trait_value);
    Serial.print("Trait Confidence: ");
    Serial.println(trait_confidence);
    Serial.println("====================      End      ====================");

    voiceController(intent_confidence, device_name, device_confidence, trait_value, trait_confidence);
  }else{
    Serial.println("Error: Failed to detect voice... Please Try Again!!!");
  }
  file.close();
  client.end();
}

void i2s_adc_data_scale(uint8_t * d_buff, uint8_t* s_buff, uint32_t len)
{
    uint32_t j = 0;
    uint32_t dac_value = 0;
    for (int i = 0; i < len; i += 2) {
        dac_value = ((((uint16_t) (s_buff[i + 1] & 0xf) << 8) | ((s_buff[i + 0]))));
        d_buff[j++] = 0;
        d_buff[j++] = dac_value * 256 / 2048;
    }
}

void i2s_adc(void *arg)
{
  bool voiceBtnState = HIGH;
  while(true)
  {
    // Enter active/recording state if button is switched
    if (digitalRead(VOICE_BTN_PIN) != voiceBtnState)
    {
      voiceBtnState = !voiceBtnState;
      blinkLed = true;
      SPIFFS.remove(filename);
      file = SPIFFS.open(filename, FILE_WRITE);
      if(!file){
        Serial.println("File is not available!");
      }

      byte header[headerSize];
      wavHeader(header, FLASH_RECORD_SIZE);

      file.write(header, headerSize);
      listSPIFFS();
      blinkLed = false;
      digitalWrite(MIC_LED_PIN, HIGH);

      int i2s_read_len = I2S_READ_LEN;
      int flash_wr_size = 0;
      size_t bytes_read;

      char* i2s_read_buff = (char*) calloc(i2s_read_len, sizeof(char));
      uint8_t* flash_write_buff = (uint8_t*) calloc(i2s_read_len, sizeof(char));

      i2s_read(I2S_PORT, (void*) i2s_read_buff, i2s_read_len, &bytes_read, portMAX_DELAY);
      i2s_read(I2S_PORT, (void*) i2s_read_buff, i2s_read_len, &bytes_read, portMAX_DELAY);
      i2s_read(I2S_PORT, (void*) i2s_read_buff, i2s_read_len, &bytes_read, portMAX_DELAY);
      i2s_read(I2S_PORT, (void*) i2s_read_buff, i2s_read_len, &bytes_read, portMAX_DELAY);

      Serial.println(" *** Recording Start *** ");
      while (flash_wr_size < FLASH_RECORD_SIZE) {
          //read data from I2S bus, in this case, from ADC.
          i2s_read(I2S_PORT, (void*) i2s_read_buff, i2s_read_len, &bytes_read, portMAX_DELAY);
          //example_disp_buf((uint8_t*) i2s_read_buff, 64);
          //save original data from I2S(ADC) into flash.
          i2s_adc_data_scale(flash_write_buff, (uint8_t*)i2s_read_buff, i2s_read_len);
          file.write((const byte*) flash_write_buff, i2s_read_len);
          flash_wr_size += i2s_read_len;
          ets_printf("Sound recording %u%%\n", flash_wr_size * 100 / FLASH_RECORD_SIZE);
          ets_printf("Never Used Stack Size: %u\n", uxTaskGetStackHighWaterMark(NULL));
      }

      digitalWrite(MIC_LED_PIN, LOW);

      file.close();

      free(i2s_read_buff);
      i2s_read_buff = NULL;
      free(flash_write_buff);
      flash_write_buff = NULL;

      listSPIFFS();

      if(Blynk.connected()){
        uploadFile();
      }
    }
  }
}

// reads gas sensor data
void sendSensorData()
{
  gasSensorVal = map(analogRead(GAS_SENSOR_PIN), 0, 4095, 0, 100);

  if (gasSensorVal > 30)
  {
    digitalWrite(GAS_LED_PIN, HIGH);
  }
  else
  {
    digitalWrite(GAS_LED_PIN, LOW);
  }

  Blynk.virtualWrite(VPIN_GAS_SENSOR, gasSensorVal);
}

void checkBlynkStatus() // called every 2 seconds by timer
{
  if (Blynk.connected())
  {
    digitalWrite(WIFI_LED_PIN, HIGH);
    sendSensorData();

    if (toBeUpdated)
    {
      if (!strcmp(device_name, "red") && !strcmp(trait_value, "on"))
      {
        digitalWrite(RELAY_PIN_1, LOW);
        Blynk.virtualWrite(VPIN_BUTTON_1, HIGH);
        relayState1 = HIGH;
        Serial.println("Switch-1 on");
      }
      if (!strcmp(device_name, "red") && !strcmp(trait_value, "off"))
      {
        digitalWrite(RELAY_PIN_1, HIGH);
        Blynk.virtualWrite(VPIN_BUTTON_1, LOW);
        relayState1 = LOW;
        Serial.println("Switch-1 off");
      }
      if (!strcmp(device_name, "green") && !strcmp(trait_value, "on"))
      {
        digitalWrite(RELAY_PIN_2, LOW);
        Blynk.virtualWrite(VPIN_BUTTON_2, HIGH);
        relayState2 = HIGH;
        Serial.println("Switch-2 on");
      }
      if (!strcmp(device_name, "green") && !strcmp(trait_value, "off"))
      {
        digitalWrite(RELAY_PIN_2, HIGH);
        Blynk.virtualWrite(VPIN_BUTTON_2, LOW);
        relayState2 = LOW;
        Serial.println("Switch-2 off");
      }
      if (!strcmp(device_name, "blue") && !strcmp(trait_value, "on"))
      {
        digitalWrite(RELAY_PIN_3, LOW);
        Blynk.virtualWrite(VPIN_BUTTON_3, HIGH);
        relayState3 = HIGH;
        Serial.println("Switch-3 on");
      }
      if (!strcmp(device_name, "blue") && !strcmp(trait_value, "off"))
      {
        digitalWrite(RELAY_PIN_3, HIGH);
        Blynk.virtualWrite(VPIN_BUTTON_3, LOW);
        relayState3 = LOW;
        Serial.println("Switch-3 off");
      }

      toBeUpdated = false;
    }
  }
  else
  {
    digitalWrite(WIFI_LED_PIN, LOW);
    Serial.println("Blynk Not Connected");
  }
}

void manualController()
{
  if (digitalRead(SWITCH_PIN_1) == LOW && switchState1 == LOW)
  {
    digitalWrite(RELAY_PIN_1, LOW);
    Blynk.virtualWrite(VPIN_BUTTON_1, HIGH);
    relayState1 = HIGH;
    switchState1 = HIGH;
    Serial.println("Switch-1 on");
  }
  if (digitalRead(SWITCH_PIN_1) == HIGH && switchState1 == HIGH)
  {
    digitalWrite(RELAY_PIN_1, HIGH);
    Blynk.virtualWrite(VPIN_BUTTON_1, LOW);
    relayState1 = LOW;
    switchState1 = LOW;
    Serial.println("Switch-1 off");
  }
  if (digitalRead(SWITCH_PIN_2) == LOW && switchState2 == LOW)
  {
    digitalWrite(RELAY_PIN_2, LOW);
    Blynk.virtualWrite(VPIN_BUTTON_2, HIGH);
    relayState2 = HIGH;
    switchState2 = HIGH;
    Serial.println("Switch-2 on");
  }
  if (digitalRead(SWITCH_PIN_2) == HIGH && switchState2 == HIGH)
  {
    digitalWrite(RELAY_PIN_2, HIGH);
    Blynk.virtualWrite(VPIN_BUTTON_2, LOW);
    relayState2 = LOW;
    switchState2 = LOW;
    Serial.println("Switch-2 off");
  }
  if (digitalRead(SWITCH_PIN_3) == LOW && switchState3 == LOW)
  {
    digitalWrite(RELAY_PIN_3, LOW);
    Blynk.virtualWrite(VPIN_BUTTON_3, HIGH);
    relayState3 = HIGH;
    switchState3 = HIGH;
    Serial.println("Switch-3 on");
  }
  if (digitalRead(SWITCH_PIN_3) == HIGH && switchState3 == HIGH)
  {
    digitalWrite(RELAY_PIN_3, HIGH);
    Blynk.virtualWrite(VPIN_BUTTON_3, LOW);
    relayState3 = LOW;
    switchState3 = LOW;
    Serial.println("Switch-3 off");
  }
}

BLYNK_WRITE(VPIN_BUTTON_1)
{
  relayState1 = param.asInt();
  digitalWrite(RELAY_PIN_1, !relayState1);
}

BLYNK_WRITE(VPIN_BUTTON_2)
{
  relayState2 = param.asInt();
  digitalWrite(RELAY_PIN_2, !relayState2);
}

BLYNK_WRITE(VPIN_BUTTON_3)
{
  relayState3 = param.asInt();
  digitalWrite(RELAY_PIN_3, !relayState3);
}

BLYNK_WRITE(VPIN_BUTTON_ALL)
{
  relayState1 = 0; digitalWrite(RELAY_PIN_1, HIGH); Blynk.virtualWrite(VPIN_BUTTON_1, relayState1); delay(100);
  relayState2 = 0; digitalWrite(RELAY_PIN_2, HIGH); Blynk.virtualWrite(VPIN_BUTTON_2, relayState2); delay(100);
  relayState3 = 0; digitalWrite(RELAY_PIN_3, HIGH); Blynk.virtualWrite(VPIN_BUTTON_3, relayState3); delay(100);
}

void setup()
{
  Serial.begin(115200);

  pinMode(RELAY_PIN_1, OUTPUT);
  pinMode(RELAY_PIN_2, OUTPUT);
  pinMode(RELAY_PIN_3, OUTPUT);

  pinMode(SWITCH_PIN_1, INPUT_PULLUP);
  pinMode(SWITCH_PIN_2, INPUT_PULLUP);
  pinMode(SWITCH_PIN_3, INPUT_PULLUP);

  pinMode(WIFI_LED_PIN, OUTPUT);
  pinMode(MIC_LED_PIN, OUTPUT);
  pinMode(GAS_LED_PIN, OUTPUT);
  pinMode(GAS_SENSOR_PIN, INPUT);
  pinMode(VOICE_BTN_PIN, INPUT_PULLUP);

  //During Starting all Relays should TURN OFF
  digitalWrite(RELAY_PIN_1, HIGH);
  digitalWrite(RELAY_PIN_2, HIGH);
  digitalWrite(RELAY_PIN_3, HIGH);

  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);

  // Setup a function to be called every 2 seconds
  timer.setInterval(2000L, checkBlynkStatus);

  SPIFFSInit();
  i2sInit();

  // make sure we don't get killed for our long running tasks
  esp_task_wdt_init(1000, false);

  xTaskCreatePinnedToCore(i2s_adc, "i2s_adc", 1024 * 3, NULL, 1, NULL, 0);
}



void loop()
{
  Blynk.run();
  manualController();
  timer.run();
  if (blinkLed)
  {
    digitalWrite(MIC_LED_PIN, HIGH);
    delay(100);
    digitalWrite(MIC_LED_PIN, LOW);
    delay(100);
  }
}
