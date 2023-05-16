// WiFi credentials
#define WIFI_SSID ""
#define WIFI_PASS ""

// define Blynk parameters
#define BLYNK_TEMPLATE_ID ""
#define BLYNK_TEMPLATE_NAME ""
#define BLYNK_AUTH_TOKEN ""
#define BLYNK_PRINT Serial

// define the GPIOs connected with Relays and switches
#define RELAY_PIN_1    14  //D14
#define RELAY_PIN_2    12  //D12
#define RELAY_PIN_3    13  //D13

#define SWITCH_PIN_1   23  //D23
#define SWITCH_PIN_2   22  //D22
#define SWITCH_PIN_3   21  //D21

#define WIFI_LED_PIN   2   //D2
#define GAS_LED_PIN    32  //D32 led indicator for gas
#define GAS_SENSOR_PIN 35  //D35 pin connected with gas sensor analog output
#define VOICE_BTN_PIN  19  //D19 voice input button
#define MIC_LED_PIN    33  //D33 mic indicator led

// define virtual pins for Blynk
#define VPIN_BUTTON_1    V0
#define VPIN_BUTTON_2    V1
#define VPIN_BUTTON_3    V2

#define VPIN_BUTTON_ALL  V3
#define VPIN_GAS_SENSOR  V4

// I2S configuration settings
#define I2S_WS            26
#define I2S_SD            27
#define I2S_SCK           25
#define I2S_PORT          I2S_NUM_0
#define I2S_SAMPLE_RATE   (16000)
#define I2S_SAMPLE_BITS   (16)
#define I2S_READ_LEN      (16 * 1024)
#define I2S_CHANNEL_NUM   (1)
#define FLASH_RECORD_SIZE (I2S_CHANNEL_NUM * I2S_SAMPLE_RATE * I2S_SAMPLE_BITS / 8 * 3)


// command recognition settings
#define COMMAND_RECOGNITION_ACCESS_KEY ""
