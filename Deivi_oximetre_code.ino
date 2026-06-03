#include <Wire.h> 
#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"

MAX30105 particleSensor;

// Configuration
const byte RATE_SIZE = 10;                 // Increased averaging window
const uint16_t IR_RED_BUFFER_SIZE = 100;
const byte SPO2_ARRAY_SIZE = 5;
const unsigned long MEASUREMENT_DURATION = 15000; // 15 seconds
const unsigned long STABILIZATION_TIME = 3000;    // Ignore first 3 seconds

// ESP32 I2C pins
const int I2C_SDA = 21;
const int I2C_SCL = 22;

// Data buffers
uint32_t irBuffer[IR_RED_BUFFER_SIZE];
uint32_t redBuffer[IR_RED_BUFFER_SIZE];
float beatAvgArray[RATE_SIZE];
float spo2AvgArray[SPO2_ARRAY_SIZE];
byte beatAvgIndex = 0;
byte spo2AvgIndex = 0;

// Measurement variables
int32_t spo2;
int8_t validSPO2;
int32_t heartRate;
int8_t validHeartRate;
float beatAvg;
float spo2Avg;
bool fingerDetected = false;

// Timing
unsigned long lastReportTime = 0;
const unsigned long REPORT_INTERVAL = 1000;
unsigned long measurementStartTime = 0;
bool measurementInProgress = false;

// Average calculation
const int MAX_SAMPLES = 50;
float bpmSamples[MAX_SAMPLES];
float spo2Samples[MAX_SAMPLES];
int sampleCount = 0;
float finalBpmAvg = 0;
float finalSpo2Avg = 0;

// Stability tracking
float lastValidBPM = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing MAX30102...");

  Wire.begin(I2C_SDA, I2C_SCL);
  
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 not found. Check wiring/power.");
    while (1);
  }

  // Configure sensor
  particleSensor.setup(60, 4, 2, 400, 411, 4096);
  particleSensor.setPulseAmplitudeRed(0x0A);
  particleSensor.setPulseAmplitudeGreen(0);

  // Pre-fill buffers
  for (byte i = 0; i < IR_RED_BUFFER_SIZE; i++) {
    while (!particleSensor.available()) particleSensor.check();
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }

  Serial.println("Place your finger on the sensor for 15 seconds.");
}

void loop() {
  uint32_t irValue = particleSensor.getIR();
  
  if (irValue > 70000) {  // Improved finger detection
    if (!fingerDetected) {
      fingerDetected = true;
      measurementStartTime = millis();
      measurementInProgress = true;
      sampleCount = 0;
      lastValidBPM = 0;

      Serial.println("Measurement started...");

      beatAvg = 0;
      spo2Avg = 0;
      beatAvgIndex = 0;
      spo2AvgIndex = 0;
      memset(beatAvgArray, 0, sizeof(beatAvgArray));
      memset(spo2AvgArray, 0, sizeof(spo2AvgArray));
    }

    // Shift buffer
    for (byte i = 1; i < IR_RED_BUFFER_SIZE; i++) {
      redBuffer[i-1] = redBuffer[i];
      irBuffer[i-1] = irBuffer[i];
    }

    while (!particleSensor.available()) particleSensor.check();
    redBuffer[IR_RED_BUFFER_SIZE-1] = particleSensor.getRed();
    irBuffer[IR_RED_BUFFER_SIZE-1] = particleSensor.getIR();
    particleSensor.nextSample();

    // Calculate HR & SpO2
    maxim_heart_rate_and_oxygen_saturation(
      irBuffer, IR_RED_BUFFER_SIZE, redBuffer,
      &spo2, &validSPO2, &heartRate, &validHeartRate
    );

    // Only collect AFTER stabilization
    bool stablePhase = (millis() - measurementStartTime > STABILIZATION_TIME);

    // HEART RATE FILTERING
    if (validHeartRate && heartRate > 50 && heartRate < 110 && stablePhase) {

      // Reject sudden spikes
      if (lastValidBPM == 0 || abs(heartRate - lastValidBPM) < 10) {
        lastValidBPM = heartRate;

        beatAvgArray[beatAvgIndex++] = (float)heartRate;
        if (beatAvgIndex >= RATE_SIZE) beatAvgIndex = 0;

        beatAvg = 0;
        for (byte i = 0; i < RATE_SIZE; i++) {
          beatAvg += beatAvgArray[i];
        }
        beatAvg /= RATE_SIZE;

        if (measurementInProgress && sampleCount < MAX_SAMPLES) {
          bpmSamples[sampleCount] = beatAvg;
        }
      }
    }

    // SPO2 FILTERING
    if (validSPO2 && spo2 >= 95 && spo2 <= 100 && stablePhase) {
      spo2AvgArray[spo2AvgIndex++] = (float)spo2;
      if (spo2AvgIndex >= SPO2_ARRAY_SIZE) spo2AvgIndex = 0;

      spo2Avg = 0;
      for (byte i = 0; i < SPO2_ARRAY_SIZE; i++) {
        spo2Avg += spo2AvgArray[i];
      }
      spo2Avg /= SPO2_ARRAY_SIZE;

      if (measurementInProgress && sampleCount < MAX_SAMPLES) {
        spo2Samples[sampleCount] = spo2Avg;
        sampleCount++;
      }
    }

    // Final result
    if (measurementInProgress && 
        (millis() - measurementStartTime >= MEASUREMENT_DURATION)) {

      measurementInProgress = false;

      finalBpmAvg = 0;
      finalSpo2Avg = 0;
      int spo2ValidSamples = 0;

      for (int i = 0; i < sampleCount; i++) {
        finalBpmAvg += bpmSamples[i];

        if (spo2Samples[i] >= 95) {
          finalSpo2Avg += spo2Samples[i];
          spo2ValidSamples++;
        }
      }

      if (sampleCount > 0) {
        finalBpmAvg /= sampleCount;
        finalSpo2Avg = spo2ValidSamples > 0 ? 
                       (finalSpo2Avg / spo2ValidSamples) : 0;

        Serial.println("\n=== RESULTS ===");
        Serial.print("Heart Rate: ");
        Serial.print(finalBpmAvg, 1);
        Serial.print(" bpm | SpO2: ");

        if (spo2ValidSamples > 0) {
          Serial.print(finalSpo2Avg, 1);
          Serial.println("%");
        } else {
          Serial.println("Insufficient valid SpO2 data");
        }

        Serial.println("===============\n");
      } else {
        Serial.println("No valid data collected. Try again.");
      }

      // Restart automatically
      if (irValue > 70000) {
        measurementStartTime = millis();
        measurementInProgress = true;
        sampleCount = 0;
        lastValidBPM = 0;
        Serial.println("Continuing measurement...");
      }
    }

    // Progress feedback
    if (millis() - lastReportTime > REPORT_INTERVAL && measurementInProgress) {
      unsigned long elapsed = (millis() - measurementStartTime) / 1000;
      Serial.print("Elapsed: ");
      Serial.print(elapsed);
      Serial.println(" seconds");
      lastReportTime = millis();
    }

  } else {
    if (fingerDetected) {
      fingerDetected = false;
      measurementInProgress = false;
      lastValidBPM = 0;

      Serial.println("Finger removed. Place finger again.");
    }
    delay(200);
  }
}