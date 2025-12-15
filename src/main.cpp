#include <Arduino.h>
#include <FastLED.h>

// --- CONFIGURATION ---
#define LED_PIN     5
#define AUDIO_PIN   A0
#define NUM_LEDS    300
#define BRIGHTNESS  255
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

// --- SENSITIVITY ---
#define SENSITIVITY 100   // Max expected volume range
#define NOISE_GATE  3    // Ignore small static
#define GAIN_FACTOR 3
#define PEAK_FALL   1     // Falling speed of the dot

CRGB leds[NUM_LEDS];

int zeroPoint = 512;
int currentHeight = 0;
int peakPosition = 0;
uint8_t hue = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Setup LEDs
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 8000);
    FastLED.clear(true);

    Serial.println("--- STARTING CALIBRATION ---");
    long sum = 0;
    for(int i = 0; i < 100; i++) {
        sum += analogRead(AUDIO_PIN);
        delay(2);
    }
    zeroPoint = sum / 100;
    Serial.print("Calibration Complete. Zero Point set to: ");
    Serial.println(zeroPoint);
    Serial.println("----------------------------");
    delay(1000);
}

void loop() {
    // READ AUDIO
    int raw = analogRead(AUDIO_PIN);

    // Calculate Loudness (Amplitude)
    int amplitude = abs(raw - zeroPoint);
    amplitude = amplitude * GAIN_FACTOR;

    // Filter Noise
    if (amplitude < NOISE_GATE) {
        amplitude = 0;
    }

    // CALCULATE HEIGHT
    int targetHeight = map(amplitude, 0, SENSITIVITY, 0, NUM_LEDS);
    targetHeight = constrain(targetHeight, 0, NUM_LEDS);

    // Smoothing
    if (targetHeight > currentHeight) {
        currentHeight = targetHeight;
    } else {
        currentHeight = (currentHeight * 10 + targetHeight) / 11;
    }

    // Peak Dot Logic
    if (currentHeight > peakPosition) {
        peakPosition = currentHeight;
    } else {
        EVERY_N_MILLISECONDS(30) {
            if (peakPosition > 0) peakPosition -= PEAK_FALL;
        }
    }

    // --- DEBUG LOGGING ---
    // This will print: "Raw: 530 | Amp: 45 | Height: 120" for Arduino Serial Plotter graph
    Serial.print("Raw:");
    Serial.print(raw);
    Serial.print(" \t| Amp:"); // \t creates a tab space
    Serial.print(amplitude);
    Serial.print(" \t| Height:");
    Serial.println(currentHeight);

    // DRAW
    FastLED.clear();
    hue++;

    // Draw Bar
    for (int i = 0; i < currentHeight; i++) {
        leds[i] = CHSV(hue + (i * 2), 255, 255);
    }

    // Draw Peak
    if (peakPosition < NUM_LEDS && peakPosition > 0) {
        leds[peakPosition] = CRGB::White;
    }

    FastLED.show();
    delay(2);
}