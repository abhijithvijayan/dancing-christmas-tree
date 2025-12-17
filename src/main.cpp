#include <Arduino.h>
#include <FastLED.h>

#define LED_PIN     5
#define AUDIO_PIN   A0
#define NUM_LEDS    300
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS  150 // Max: 255
#define NOISE_GATE  15 // Filter out static noise
#define GAIN_FACTOR 3
#define PEAK_FALL   1 // How fast the peak led should fall
#define MIN_CEILING 80
#define DECAY_RATE  100

CRGB leds[NUM_LEDS];

int zeroPoint = 512;
int currentHeight = 0;
int peakPosition = 0;
uint8_t hue = 0;
int maxVol = 100; // Dynamic Ceiling

void setup() {
    Serial.begin(115200);
    delay(1000);

    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 8000);
    FastLED.clear(true);

    long sum = 0;
    for(int i=0; i<200; i++) { sum += analogRead(AUDIO_PIN); delay(2); }
    zeroPoint = sum / 200;
}

void loop() {
    int raw = analogRead(AUDIO_PIN);
    int amplitude = abs(raw - zeroPoint);
    amplitude = amplitude * GAIN_FACTOR;

    if (amplitude < NOISE_GATE) {
        amplitude = 0;
    }

    // Adjust the max ceiling based on the amplitude
    // If a loud chorus hits (volume 200), ceiling is now 200.
    if (amplitude > maxVol) {
        maxVol = amplitude;
    }

    // Every 100ms, lowers the ceiling by 1.
    // If the next song is quiet (max volume 100), the ceiling will slowly drift down from 200 to 100,
    // ensuring the quiet song eventually fills the whole tree again.
    EVERY_N_MILLISECONDS(DECAY_RATE) {
        if (maxVol > MIN_CEILING) {
            maxVol--;
        }
    }

    // translates the amplitude to the leds relative to the ceiling value
    int targetHeight = map(amplitude, 0, maxVol, 0, NUM_LEDS);
    targetHeight = constrain(targetHeight, 0, NUM_LEDS);

    if (targetHeight > currentHeight) {
        currentHeight = targetHeight; // Rise Instantly
    }
    else {
        currentHeight = (currentHeight * 15 + targetHeight) / 16;// Fall Slowly
    }

    // When the music pushes the bar up, it kicks the dot up too
    if (currentHeight > peakPosition) {
        peakPosition = currentHeight;
    }
    else {
        // When the bar drops, the dot stays in the air for a split second before falling down slowly (1 pixel every 30ms).
        EVERY_N_MILLISECONDS(30) {
            if (peakPosition > 0) {
                peakPosition -= PEAK_FALL;
            }
        }
    }

    // Ensure it never goes below 0 or above 299
    peakPosition = constrain(peakPosition, 0, NUM_LEDS - 1);

    FastLED.clear();

    hue++; // Color Cycle
    for (int i = 0; i < currentHeight; i++) {
        // CHSV creates the rainbow
        leds[i] = CHSV(
        // hue + (i * 2) means every pixel is a slightly different color than the one below it
        hue + (i * 2),
        255,
        255
        );
    }

    if (peakPosition > 0) {
        leds[peakPosition] = CRGB::White;
    }

    FastLED.show();

    // SEND 6 VALUES
    // Order: Raw, Amp, MaxVol, Zero, LedHeight, PeakPos
    Serial.print(raw); Serial.print(",");
    Serial.print(amplitude); Serial.print(",");
    Serial.print(maxVol); Serial.print(",");
    Serial.print(zeroPoint); Serial.print(",");
    Serial.print(currentHeight); Serial.print(",");
    Serial.println(peakPosition);

    delay(2);
}