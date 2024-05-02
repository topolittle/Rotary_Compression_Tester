/*Rotary Compression Tester
This code assumes you're using a 0-200 psi - 0.5-4.5 vdc pressure transducer connected to the A0 pin.
You can use sensors with other pressure or voltage ranges but you'll need to modify the code.

Originally distributed by John Doss 2017.
refactored by Patrick McQuay, in 2020.
Heavily modified using correction factors from code by Miro Bogner, in 2022.
Modified, refactored by Stephane Gilbert in 2024 for better acquisition sampling rate, accuracy and display.
*/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// The firmware version, 'Major' and 'Minor' only
#define FIRMWARE_VERSION                "2.0"

// Sensor internal volume. See the 'Fine tuning the dead space coorrection factor' in the README.md
#define SENSOR_VOLUME_CC                3.0f    // The internal volume of the sensor itself, in cubic centimeters

// Engine specifications
#define COMPRESSION_RATIO               10      // The Renesis has a compression ration of 10:1
#define ROTOR_SINGLE_FACE_MAX_VOLUME_CC 654.0f  // The Renesis has a total of 1308cc for two rotors, this means 654cc per rotor
#define NUMBER_OF_CHAMBERS              3       // The Renesis has 3 chamber per rotors

// Gather 10 complete rotation of the rotor and keeps the last 3
#define NUMBER_OF_WARMUP_ACQUISITIONS   5
#define NUMBER_OF_ACQUISITIONS          3

#define DELAY_BETWEEN_RESULTS           4000    // Delay to wait between raw and normalized results
#define BASELINE_PRESSURE_SAMPLE        10      // Number of sample to take establish the the ambiant pressure baseline

// Threshold to look for max and min values, cannot be zero due to noise
#define MAX_THRESHOLD                   15
#define MIN_THRESHOLD                   MAX_THRESHOLD

#define SCALE_250_RPM_A0 8.5944582043344f     // polynom Fit a0 to norm values to 250rpm
#define SCALE_250_RPM_A1 -0.04802870227f      // polynom Fit a1 to norm values to 250rpm
#define SCALE_250_RPM_A2 0.00005425051599f    // polynom Fit a2 to norm values to 250rpm

#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 64    // OLED display height, in pixels

#define SENSOR_PIN A0       // The sensor is connected to A0 pin

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Structure to holds a single compression result, with pressure in PSI for each face
// and the recorded RPM at the time of the test
struct CompressionRecord {
    float facePressures[NUMBER_OF_CHAMBERS];
    int rpm;
};

// Structure to hold two compression record: one untouched and one normalized to 250 RPM
struct CompressionRecords {
    CompressionRecord compressionRecord;
    CompressionRecord normalizedCompressionRecord;
};

// Structure to hold the raw analog input from the sensor for a single test run
struct RawADRecord {
    int value[NUMBER_OF_CHAMBERS];
    int rpm; 
};

// Global variable to hold the ambient pressure. It's used as a baseline for further compression reading
float ambientPressure = 0.0f;

// Global variable to hold a conversion factor for the empty space inside the sensor assembly
float deadSpaceCorrectionFactor;

void setup() {
    // Set the dead space correction factor
    deadSpaceCorrectionFactor = computeDeadSpaceCorrectionFactor();

    Serial.begin(19200);      //serial speed
    delay(200);

    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        while (true) {
            Serial.println(F("SSD1306 allocation failed"));
            delay(1000);
        }
     }

    displayIntro();
    delay(2000);

    if (!sensorDectected()) {
        displaySensorFault();
        while (true) {} // loop forever
    }

    // Record the ambient pressure baseline
    delay(1000);
    ambientPressure = getCurrentPressure();

    displayCrank();
}

void loop() {
    // CompressionRecord rec = performTest(); 
    CompressionRecords compressionRecords = performTest();
    displayStop();
    delay(2000);
    displayResults(compressionRecords); 
}

// Compute the correction factor for the dead space inside the sensor assembly
// This dead space adds to the engine displacement volume so it must be taken into account
float computeDeadSpaceCorrectionFactor() {
    float singleFaceMinVolumeCC = (float)ROTOR_SINGLE_FACE_MAX_VOLUME_CC / (float)COMPRESSION_RATIO;
    float volumeMinWithSensorCC = singleFaceMinVolumeCC + (float)SENSOR_VOLUME_CC;
    return volumeMinWithSensorCC / singleFaceMinVolumeCC;
}

// Convert a raw analog sensor reading into a pressure value in PSI
// @param rawValue: The raw value from the analog to digital conversion
// @return: The value converted into PSI
float mapRawToAbsolutePressure(int rawValue) {
    return mapfloat((float)(rawValue), (1024.0f * 0.5f / 5.0f), (1024.0f * 4.5f / 5.0f), 0.0f, 200.0f);
}

// Normalize the pressure to 250 RPM
// @param pressure: The pressure in PSI to normalize
// @param rpm: The RPM at which the pressure reading was taken
// @return: The normalized pressure, in PSI, normalized to 250 RPM
float scaleTo250Rpm(float pressure, int rpm) {
    return pressure + (SCALE_250_RPM_A0 + (SCALE_250_RPM_A1 * rpm) + (SCALE_250_RPM_A2 * sq(rpm)));
}

// Compute the actual pressure from the raw value from the analog to digital conversion
// and take into accound the ambient pressure baseline and the sensor assembly internal volume
// @param rawValue: The raw value from the analog to digital conversion at the sensor input pin
// @return: The pressure, in PSI, converted and corrected
int scaleSensorRead(int rawValue) {
    return (mapRawToAbsolutePressure(rawValue) - ambientPressure) * deadSpaceCorrectionFactor;
}

// This function is used for mapping a value from one range to another
// @param x: The input value that we want to map
// @param in_min: The lower bound of the range of the input value
// @param in_max: The higher bound of the range of the input value
// @param out_min: The lower bound of the range to which we want to map the input value
// @param out_max: The higher bound of the range to which we want to map the input value
// @return: The mapped value
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Function to convert pressure from psi (pounds per square inch) to bar
// @param psi: The PSI value to convert into bar
// @return: The pressure in bar
float psiToBar(float psi) {
    /* The function works by multiplying the input psi value by a constant conversion factor 0.068947448.
       This conversion factor is used because 1 psi is approximately equal to 0.068947448 bar.

       Therefore, this function takes a pressure in psi as input and returns the equivalent pressure in bar. */
    return psi * 0.068947448;
}

// Return the current pressure from the sensor
// We take the number defined by BASELINE_PRESSURE_SAMPLE plus one, since we do not take the first onto accound
// @return: The current pressure
float getCurrentPressure() {
    float pressure = 0;
    for(int i = 0; i < BASELINE_PRESSURE_SAMPLE + 1; i++) {
        if (i != 0 ) pressure += mapRawToAbsolutePressure(analogRead(SENSOR_PIN));
        delay(10);
    }

    // Return the average
    return pressure / (float)BASELINE_PRESSURE_SAMPLE;
}

void displayIntro() {
    display.clearDisplay();
    display.invertDisplay(true);

    display.setTextSize(2);             
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println(F("  ROTARY"));
    display.println(F("  COMP."));
    display.println(F("  TESTER"));
    display.print(F(" Ver. "));
    display.println(F(FIRMWARE_VERSION));
    display.display();
}

void displayCrank() {
    display.clearDisplay();
    display.invertDisplay(false);
    
    display.setCursor(0,0);
    display.println(F(" Waiting."));
    display.println(F("  CRANK"));
    display.println(F("  ENGINE"));
    display.println(F("  NOW"));

    display.display();
}

void displaySensorFault() {
    display.clearDisplay();
    display.invertDisplay(false);
    
    display.setCursor(0,0);
    display.println();
    display.println(F("   ERROR"));
    display.println(F(" NO SENSOR"));

    display.display();
}

void displayPerformingTest() {
    display.clearDisplay();
    display.setCursor(0,0);
    display.println();
    display.println(F("  TESTING"));
    display.println(F("    ..."));

    display.display();
}

void displayStop() {
    display.clearDisplay();
    display.setCursor(0,0);
    display.println();
    display.println(F("   STOP!"));

    display.display();
}

// Display a result in bar
// @param record: The compression record to display
// @param normalized: True if the result is a normalized one, otherwise false
void displayResultBar(CompressionRecord record, bool normalized) {
    display.clearDisplay();

    display.setCursor(0,0);
    display.print(F("RPM: "));
    display.print(min(9999, record.rpm));
    display.println((normalized) ? F("N") : F(""));
    for (int face = 0; face < NUMBER_OF_CHAMBERS; face++) {
        display.print(F("F-"));
        display.print(face + 1);
        display.print(F(": "));
        display.println(min(99, max(0, psiToBar(record.facePressures[face]))), 1);
    }

    display.display();
}

// Display in loop the test results, raw and normalized to 250 RPM
// Note: This function never return
void displayResults(CompressionRecords records) {
    while(true) {
        displayResultBar(records.compressionRecord, false);
        delay(DELAY_BETWEEN_RESULTS);
        displayResultBar(records.normalizedCompressionRecord, true);
        delay(DELAY_BETWEEN_RESULTS);
    }
}

// Detect the presence of the pressure sensor
// @return: True if the sensor is present, otherwise false
bool sensorDectected() {
    return !(getCurrentPressure() < -10.0f);
}

// This function perform the actual compression test
CompressionRecords performTest() {
    unsigned long startTime;
    unsigned long endTime;
    int currentValue;
    int minValue;
    RawADRecord rawADRecords[NUMBER_OF_ACQUISITIONS];
    RawADRecord rawRecordSingleRun;
    CompressionRecords compressionRecords;

    // recording start time for RPM calculation
    startTime = millis();

    // main loop for data acquisition, includes warmup acquisitions
    for (int run = 0; run < NUMBER_OF_WARMUP_ACQUISITIONS + NUMBER_OF_ACQUISITIONS; run++) {
        // inner loop for reading sensor data from each chamber
        for (int face = 0; face < NUMBER_OF_CHAMBERS; face++) {
            rawRecordSingleRun.value[face] = 0;
            currentValue = analogRead(SENSOR_PIN);

            // look for peak pulse value, indicating maximum pressure
            while ((rawRecordSingleRun.value[face] - currentValue) <= 20) {
                if(currentValue > rawRecordSingleRun.value[face]) {
                    rawRecordSingleRun.value[face] = currentValue;
                }
            
                currentValue = analogRead(SENSOR_PIN);
            }

            // wait for pressure to decay before moving to next chamber
            minValue = currentValue;
            while((currentValue - minValue) < 20) {
                currentValue = analogRead(SENSOR_PIN);

                if (currentValue < minValue) {
                    minValue = currentValue;
                }
            }
        }

        // record end time for RPM calculation
        endTime = millis();

        // compute RPM
        rawRecordSingleRun.rpm = (int)round(180000.f / (endTime - startTime));

        // update display after first run
        if (run == 0)
            displayPerformingTest();

        // store data from runs only after warmup acquisitions are complete
        if (run >= NUMBER_OF_WARMUP_ACQUISITIONS) {
            rawADRecords[run - NUMBER_OF_WARMUP_ACQUISITIONS] = rawRecordSingleRun;
        }

        // reset start time for next cycle
        startTime = endTime;
    }

    // compute average of raw results
    for (int face = 0; face < NUMBER_OF_CHAMBERS; face++) {
        rawRecordSingleRun.value[face] = 0;
        for (int run = 0; run < NUMBER_OF_ACQUISITIONS; run++) {
            rawRecordSingleRun.value[face] += rawADRecords[run].value[face];
        }

        rawRecordSingleRun.value[face] /= NUMBER_OF_ACQUISITIONS;
    }

    // compute average RPM
    rawRecordSingleRun.rpm = 0;
    for (int run = 0; run < NUMBER_OF_ACQUISITIONS; run++) {
        rawRecordSingleRun.rpm += rawADRecords[run].rpm;
    }

    rawRecordSingleRun.rpm /= NUMBER_OF_ACQUISITIONS;

    // transform raw sensor reads to actual compression values
    for (int face = 0; face < NUMBER_OF_CHAMBERS; face++) {
        compressionRecords.compressionRecord.facePressures[face] = scaleSensorRead(rawRecordSingleRun.value[face]);
    }

    // copy the calculated RPM value to the compression records
    compressionRecords.compressionRecord.rpm = rawRecordSingleRun.rpm;

    // Compute the normalized result to 250 RPM
    for (int face = 0; face < NUMBER_OF_CHAMBERS; face++) {
        compressionRecords.normalizedCompressionRecord.facePressures[face] = scaleTo250Rpm(compressionRecords.compressionRecord.facePressures[face], compressionRecords.compressionRecord.rpm);
    }

    // set the normalized RPM to 250
    compressionRecords.normalizedCompressionRecord.rpm = 250;

    // return the processed compression records
    return compressionRecords;
}
