#include <SoftwareSerial.h>
#include <Adafruit_VC0706.h>
#include <LowPower.h>

#include "SIM800L.h"

#define SIM800_TX_PIN 2
#define SIM800_RX_PIN 3
#define SIM800_RST_PIN 4
#define CAM_TX_PIN 5
#define CAM_RX_PIN 6

#define MOSFET_PIN 10
#define MOSFET_LED_PIN 13

#define CHUNK_SIZE 10000

int interval = 600; // Default interval between 2 shots

void (*resetFunc)(void) = 0;

const char APN[] = "free";

SIM800L *sim800l;
SoftwareSerial *serial;
SoftwareSerial cameraconnection = SoftwareSerial(CAM_TX_PIN, CAM_RX_PIN);
Adafruit_VC0706 cam = Adafruit_VC0706(&cameraconnection);
uint16_t jpglen = 0;

/**
 * Pass the camera stream to the SIM800L
 **/
void CameraCallback(Stream *stream, uint32_t length)
{
  serial->flush();
  cameraconnection.listen();
  uint32_t currentLength = length;
  uint16_t jpglen = cam.frameLength();
  delay(5000);
  uint8_t imgsize = cam.getImageSize();

  while (currentLength > 0)
  {
    // read 32 bytes at a time;
    uint8_t *buffer;
    uint8_t bytesToRead = min(32, currentLength);
    buffer = cam.readPicture(bytesToRead);
    for (int i = 0; i < bytesToRead; i++)
    {
      stream->write(buffer[i]);
    }
    currentLength -= bytesToRead;
  }
  delay(500);
  serial->listen();
}

void setup()
{
  pinMode(MOSFET_PIN, OUTPUT);
  pinMode(MOSFET_LED_PIN, OUTPUT);

  randomSeed(analogRead(0));

  // Initialize Serial Monitor for debugging
  Serial.begin(115200);
  while (!Serial);

  digitalWrite(MOSFET_PIN, HIGH);
  digitalWrite(MOSFET_LED_PIN, HIGH);


  Serial.println(cam.frameLength());
  delay(5000);
  // Print out the camera version information (optional)
  char *reply = cam.getVersion();
  if (reply == 0)
  {
    Serial.println("Failed to get version");
  }
  else
  {
    Serial.println(reply);
  }

  // Initialize a SoftwareSerial
  serial = new SoftwareSerial(SIM800_TX_PIN, SIM800_RX_PIN);
  serial->begin(9600);
  delay(1000);

  // Equivalent line with the debug enabled on the Serial
  sim800l = new SIM800L((Stream *)serial, SIM800_RST_PIN, 200, 512, (Stream *)&Serial);

  // Setup module for GPRS communication
  setupModule();

  digitalWrite(MOSFET_PIN, HIGH);
  digitalWrite(MOSFET_LED_PIN, HIGH);

  // Wait until the module is ready to accept AT commands
  while (!sim800l->isReady())
  {
    Serial.println(F("Problem to initialize AT command, retry in 1 sec"));
    delay(1000);
  }
  Serial.println(F("Setup Complete!"));
  // Wait for the GSM signal

  // Wait for operator network registration (national or roaming network)
  NetworkRegistration network = sim800l->getRegistrationStatus();
  while (network != REGISTERED_HOME && network != REGISTERED_ROAMING)
  {
    delay(1000);
    network = sim800l->getRegistrationStatus();
  }
  Serial.println(F("Network registration OK"));
  delay(1000);

  // Setup APN for GPRS configuration
  bool success = sim800l->setupGPRS(APN);
  while (!success)
  {
    success = sim800l->setupGPRS(APN);
    delay(5000);
  }
  Serial.println(F("GPRS config OK"));

  // Establish GPRS connectivity (5 trials)
  bool connected = false;
  for (uint8_t i = 0; i < 5 && !connected; i++)
  {
    delay(1000);
    connected = sim800l->connectGPRS();
  }

  // Check if connected, if not reset the module and setup the config again
  if (connected)
  {
    Serial.println(F("GPRS connected !"));
  }
  else
  {
    Serial.println(F("GPRS not connected !"));
    Serial.println(F("Reset the module."));
    sim800l->reset();
    setupModule();
    return;
  }

  Serial.println(F("Start HTTP POST..."));
  delay(500);
  // Try to locate the camera
  if (cam.begin())
  {
  }
  else
  {
    Serial.println("No camera found?");
    return;
  }

  cam.setBaud9600();
  cam.setImageSize(VC0706_640x480);
  cam.takePicture();
  jpglen = cam.frameLength();
  serial->listen();
  Serial.println(jpglen);

  uint16_t rc = sim800l->doGet("https://sim.yavin.space/interval", 65535);
  if (rc == 200)
  {
    // Success, output the data received on the serial
    Serial.print(F("HTTP GET successful ("));
    Serial.print(sim800l->getDataSizeReceived());
    Serial.println(F(" bytes)"));
    Serial.print(F("Interval is : "));
    interval = atoi(sim800l->getDataReceived());
    Serial.println(interval);

    if (interval < 300)
      interval = 300;
    if (interval > 43200)
      interval = 43200;
  }
  else
  {
    // Failed...
    Serial.print(F("HTTP GET error "));
    Serial.println(rc);
  }

  // Do HTTP POST communication with 10s for the timeout (read)
  uint32_t totalSize = jpglen;
  uint32_t nbChunks = totalSize / CHUNK_SIZE + 1;
  uint32_t id = random(2147483647L);
  Serial.println(nbChunks);
  for (uint32_t chunkIndex = 0L; chunkIndex < nbChunks; chunkIndex++)
  {

    char *url = (char *)malloc(250);
    uint32_t currentSize;
    if (chunkIndex == nbChunks - 1)
    {
      currentSize = totalSize % (CHUNK_SIZE + 1);
      sprintf(url, "https://sim.yavin.space/upload/%lu/last", id);
    }
    else
    {
      currentSize = CHUNK_SIZE;
      sprintf(url, "https://sim.yavin.space/upload/%lu/%lu", id, chunkIndex);
    }
    Serial.println(url);
    uint16_t rc = sim800l->doPost(url, "image/jpeg", currentSize, &SomeCallback, 65535, 4294967295);
    if (rc == 200)
    {
      // Success, output the data received on the serial
      Serial.print(F("HTTP POST successful ("));
      Serial.print(sim800l->getDataSizeReceived());
      Serial.println(F(" bytes)"));
      Serial.print(F("Received : "));
      Serial.println(sim800l->getDataReceived());
    }
    else
    {
      // Failed...
      Serial.print(F("HTTP POST error "));
      Serial.println(rc);
    }
    free(url);
  }

  // Close GPRS connectivity (5 trials)
  bool disconnected = sim800l->disconnectGPRS();
  for (uint8_t i = 0; i < 5 && !connected; i++)
  {
    delay(1000);
    disconnected = sim800l->disconnectGPRS();
  }

  if (disconnected)
  {
    Serial.println(F("GPRS disconnected !"));
  }
  else
  {
    Serial.println(F("GPRS still connected !"));
  }

  // Go into low power mode
  bool lowPowerMode = sim800l->setPowerMode(MINIMUM);
  if (lowPowerMode)
  {
    Serial.println(F("Module in low power mode"));
  }
  else
  {
    Serial.println(F("Failed to switch module to low power mode"));
  }

  bool normalPowerMode = sim800l->setPowerMode(NORMAL);
  if (lowPowerMode)
  {
    Serial.println(F("Module in normal power mode"));
  }
  else
  {
    Serial.println(F("Failed to switch module to normal power mode"));
  }

  // Approximately sleeping "interval" seconds

  digitalWrite(MOSFET_PIN, LOW);
  digitalWrite(MOSFET_LED_PIN, LOW);
  for (uint8_t i = 0; i < (interval / 64); i++)
  {
    for (uint8_t j = 0; j < 8; j++)
    {
      LowPower.powerDown(SLEEP_8S, ADC_ON, BOD_OFF);
    }

    // Waking up the battery and prevent it to shut down by drawing some current
    digitalWrite(MOSFET_PIN, HIGH);
    digitalWrite(MOSFET_LED_PIN, HIGH);
    delay(1000);
    digitalWrite(MOSFET_PIN, LOW);
    digitalWrite(MOSFET_LED_PIN, LOW);
  }
  resetFunc();
}

void setupModule()
{
}
