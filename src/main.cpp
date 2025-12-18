#include <Arduino.h>
#include "Arduino_LED_Matrix.h" // <--- MUST BE FIRST to avoid conflict
#include <FastLED.h>            // <--- MUST BE SECOND

#define LED_PIN     5
#define AUDIO_PIN   A0
#define SWITCH_PIN  2
#define NUM_LEDS    300
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS  150 // Max: 255
#define NOISE_GATE  15 // Filter out static noise
#define GAIN_FACTOR 3
#define PEAK_FALL   1 // How fast the peak led should fall
#define MIN_CEILING 120
#define DECAY_RATE  100

CRGB leds[NUM_LEDS];
ArduinoLEDMatrix matrix;

int zeroPoint = 512;
int currentHeight = 0;
int peakPosition = 0;
uint8_t hue = 0;
int maxVol = 100; // Dynamic Ceiling

// --- IDLE EFFECT VARIABLES ---
uint8_t currentPatternIndex = 0;
uint8_t gHue = 0;

// --- MATRIX BUFFER ---
// The Uno R4 uses a grid of 8 rows x 12 columns
uint8_t frame[8][12];

void rainbowWithGlitter();
void confetti();
void sinelon();
void bpm();
void juggle();
void fire();
void snow();
void twinkle();
void police();
void breathing();
void candyCane();

// List of patterns to cycle through (Total: 11)
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = {
    rainbowWithGlitter, confetti, sinelon, bpm, juggle,
    fire, snow, twinkle, police, breathing, candyCane
};

void setup() {
    Serial.begin(115200);
    delay(1000);

    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, 8000);
    FastLED.clear(true);

    matrix.begin();

    pinMode(SWITCH_PIN, INPUT_PULLUP);

    long sum = 0;
    for(int i=0; i<200; i++) { sum += analogRead(AUDIO_PIN); delay(2); }
    zeroPoint = sum / 200;

    // If calibration sees "dead air" (1 or 2), force a safe default
    if (zeroPoint < 50) {
        zeroPoint = 512;
    }
}

void loop() {
    int raw = analogRead(AUDIO_PIN);
    int amplitude = 0;

    bool musicMode = digitalRead(SWITCH_PIN) == HIGH;

    // Only calculate volume if the input is active (> 50)
    if (musicMode && raw > 50) {
        // -----------------------------
        // MODE A: MUSIC VISUALIZER
        // -----------------------------

        amplitude = abs(raw - zeroPoint);
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

        // --- DRAW MATRIX (Volume Bars) ---
        // int matrixHeight = map(amplitude, 0, maxVol, 0, 8); // Map volume to 0-8 (Matrix Height)
        // matrixHeight = constrain(matrixHeight, 0, 8);

        // --- DRAW MATRIX (Mini Tree Mirror) ---
        // Instead of raw volume, we map the ACTUAL tree height (currentHeight)
        // This makes the matrix look exactly like a tiny version of the big tree.
        int matrixHeight = map(currentHeight, 0, NUM_LEDS, 0, 8);
        matrixHeight = constrain(matrixHeight, 0, 8);

        // Clear Frame
        for(int y=0; y<8; y+=1) {
            for(int x=0; x<12; x++) {
                frame[y][x] = 0;
            }
        }

        // Fill Frame (Bottom Up)
        // Note: On R4 Matrix, row 0 is top, row 7 is bottom.
        for (int y = 0; y < matrixHeight; y+=1) {
            // We start drawing from row 7 (bottom) and go up
            int drawRow = 7 - y;
            for(int x=0; x<12; x++) {
                frame[drawRow][x] = 1; // Turn pixel ON
            }
        }

        // Push to Matrix
        matrix.renderBitmap(frame, 8, 12);
    } else {
        // -----------------------------
        // MODE B: IDLE EFFECTS (No Music)
        // -----------------------------

        // 1. Play the Effect
        gPatterns[currentPatternIndex]();

        // 2. Cycle Pattern
        EVERY_N_SECONDS(10) { currentPatternIndex = (currentPatternIndex + 1) % 11; }
        EVERY_N_MILLISECONDS(20) { gHue++; }

        // Scan the LEDs to send "Fake Audio Data" to the chart
        int highestActivePixel = 0;
        long totalBrightness = 0;

        for(int i = 0; i < NUM_LEDS; i++) {
            // Calculate pixel brightness (Average of R, G, B)
            int brightness = (leds[i].r + leds[i].g + leds[i].b) / 3;

            // If pixel is lit, update highest point
            if(brightness > 10) {
                highestActivePixel = i;
            }
            totalBrightness += brightness;
        }

        // Update globals so Serial.print sends the visualizer data
        currentHeight = highestActivePixel;
        peakPosition = currentHeight;

        // Calculate fake amplitude based on how bright the tree is
        int avgBrightness = totalBrightness / NUM_LEDS;
        amplitude = avgBrightness * 4; // Multiply so it shows up clearly on the pink chart
        maxVol = 100; // Reset spike protection

        // --- DRAW MATRIX (Mini Tree Mirror) ---
        // Map the highest lit pixel on the tree (0-300) to the matrix height (0-8)
        int mirrorHeight = map(currentHeight, 0, NUM_LEDS, 0, 8);
        mirrorHeight = constrain(mirrorHeight, 0, 8);

        // --- DRAW MATRIX (Effect Icons) ---
        // Clear Frame first
        for(int y=0; y<8; y++) { for(int x=0; x<12; x++) frame[y][x] = 0; }

        switch(currentPatternIndex) {
            case 0: // RAINBOW: Moving Diagonal Lines
                {
                    int offset = millis() / 100;
                    for (int y = 0; y < 8; y++) {
                        for (int x = 0; x < 12; x++) {
                            // Creates diagonal bands that scroll
                            if ((x + y + offset) % 4 == 0) frame[y][x] = 1;
                        }
                    }
                }
                break;

            case 1: // CONFETTI: Random Sparkles
                {
                    // Pick 15 random spots to light up each frame
                    for(int i=0; i<15; i++) {
                        frame[random(8)][random(12)] = 1;
                    }
                }
                break;

            case 2: // SINELON: Cylon Eye / Scanner
                {
                    // A vertical bar moving left to right
                    int x = beatsin8(30, 0, 11); // Speed 30, Range 0-11
                    for(int y=0; y<8; y++) {
                        frame[y][x] = 1;
                    }
                }
                break;

            case 3: // BPM: Heart Icon
                {
                    // Simple Heart Shape
                    // Row 0 is top
                    frame[1][2]=1; frame[1][3]=1;     frame[1][8]=1; frame[1][9]=1;
                    frame[2][1]=1; frame[2][4]=1;     frame[2][7]=1; frame[2][10]=1;
                    frame[3][1]=1; frame[3][10]=1;
                    frame[4][2]=1; frame[4][9]=1;
                    frame[5][3]=1; frame[5][8]=1;
                    frame[6][4]=1; frame[6][7]=1;
                    frame[7][5]=1; frame[7][6]=1;
                }
                break;

            case 4: // JUGGLE: 3 Bouncing Dots
                {
                   // Simulate 3 balls bouncing at different speeds
                   int y1 = beatsin8(30, 0, 7); frame[7-y1][2] = 1;
                   int y2 = beatsin8(38, 0, 7); frame[7-y2][6] = 1;
                   int y3 = beatsin8(48, 0, 7); frame[7-y3][10] = 1;
                }
                break;

            case 5: // FIRE (Flickering Bottom)
            {
                // Bottom 2 rows solid
                for(int x=0; x<12; x++) { frame[7][x]=1; frame[6][x]=1; }
                // Rows 3-5 flicker
                for(int i=0; i<8; i++) frame[random(3,6)][random(12)] = 1;
            }
                break;

            case 6: // SNOW (Falling Dots)
            {
                int offset = millis() / 200;
                for(int x=0; x<12; x++) {
                    // Create "lanes" of falling snow based on X position
                    int y = (offset + x*3) % 8;
                    frame[y][x] = 1;
                }
            }
                break;

            case 7: // TWINKLE (Sparse Stars)
                if ((millis() / 250) % 2 == 0) {
                    frame[2][2]=1; frame[2][9]=1; frame[6][5]=1;
                } else {
                    frame[4][2]=1; frame[1][6]=1; frame[5][10]=1;
                }
                break;

            case 8: // POLICE (Split Screen Flash)
                if ((millis() / 200) % 2 == 0) {
                    for(int y=0;y<8;y++) for(int x=0;x<6;x++) frame[y][x]=1; // Left ON
                } else {
                    for(int y=0;y<8;y++) for(int x=6;x<12;x++) frame[y][x]=1; // Right ON
                }
                break;

            case 9: // BREATHING (Expanding/Contracting Box)
            {
                int size = beatsin8(20, 1, 4); // Size radius 1 to 4
                int cx = 6; int cy = 4;
                for(int y=0; y<8; y++) {
                    for(int x=0; x<12; x++) {
                        if(abs(x-cx) < size && abs(y-cy) < size) frame[y][x]=1;
                    }
                }
            }
                break;

            case 10: // CANDY CANE (Diagonal stripes)
            {
                int offset = millis() / 150;
                for(int y=0; y<8; y++) {
                    for(int x=0; x<12; x++) {
                        // Wider stripes than rainbow
                        if((x+y+offset) % 6 < 3) frame[y][x]=1;
                    }
                }
            }
                break;

        }

        matrix.renderBitmap(frame, 8, 12);
    }

    // ==========================================
    //      OUTPUT & TELEMETRY
    // ==========================================

    FastLED.show();

    // Send data to web (Amplitude will be 0 during idle, which is correct for the chart)
    Serial.print(raw); Serial.print(",");
    Serial.print(amplitude); Serial.print(",");
    Serial.print(maxVol); Serial.print(",");
    Serial.print(zeroPoint); Serial.print(",");
    Serial.print(currentHeight); Serial.print(",");
    Serial.print(peakPosition); Serial.print(",");
    Serial.println(musicMode ? 1 : 0);
}

// ==========================================
//      EFFECT FUNCTIONS
// ==========================================

void rainbowWithGlitter() {
    // Built-in FastLED rainbow, plus white sparkles
    fill_rainbow( leds, NUM_LEDS, gHue, 7);
    if( random8() < 80) {
        leds[ random16(NUM_LEDS) ] += CRGB::White;
    }
}

void confetti() {
    // Random colored speckles that blink in and fade smoothly
    fadeToBlackBy( leds, NUM_LEDS, 10);
    int pos = random16(NUM_LEDS);
    leds[pos] += CHSV( gHue + random8(64), 200, 255);
}

void sinelon() {
    // A colored dot sweeping back and forth, with fading trails
    fadeToBlackBy( leds, NUM_LEDS, 20);
    int pos = beatsin16( 13, 0, NUM_LEDS-1 );
    leds[pos] += CHSV( gHue, 255, 192);
}

void bpm() {
    // Colored stripes pulsing at a defined Beats-Per-Minute (62 BPM)
    uint8_t BeatsPerMinute = 62;
    CRGBPalette16 palette = PartyColors_p;
    uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
    for( int i = 0; i < NUM_LEDS; i++) {
        leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
    }
}

void juggle() {
    // Eight colored dots, weaving in and out of sync with each other
    fadeToBlackBy( leds, NUM_LEDS, 20);
    byte dothue = 0;
    for( int i = 0; i < 8; i++) {
        leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(dothue, 200, 255);
        dothue += 32;
    }
}

void fire() {
    // HeatMap Color Palette
    for(int i = 0; i < NUM_LEDS; i++) {
        uint8_t noise = inoise8(i * 30, millis() / 5);
        leds[i] = ColorFromPalette(HeatColors_p, noise);
    }
}

void snow() {
    // Cold Blue Background
    fill_solid(leds, NUM_LEDS, CRGB(0, 10, 40));
    // Random White Flakes
    if(random8() < 40) {
        int pos = random16(NUM_LEDS);
        leds[pos] = CRGB::White;
    }
}

void twinkle() {
    // Fade everything slowly
    fadeToBlackBy(leds, NUM_LEDS, 5);
    // Add soft random lights
    if(random8() < 60) {
        int pos = random16(NUM_LEDS);
        leds[pos] = CHSV(45, 100, 200); // Warm Gold color
    }
}

void police() {
    // Flashing Red and Blue
    if ((millis() / 200) % 2 == 0) {
        fill_solid(leds, NUM_LEDS, CRGB::Red);
    } else {
        fill_solid(leds, NUM_LEDS, CRGB::Blue);
    }
}

void breathing() {
    // Smooth fade in and out with changing colors
    float breath = (exp(sin(millis()/2000.0*PI)) - 0.36787944)*108.0;
    fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, breath));
}

void candyCane() {
    // Scrolling Red and White stripes
    int offset = millis() / 50;
    for(int i = 0; i < NUM_LEDS; i++) {
        if ( (i + offset) % 20 < 10) {
            leds[i] = CRGB::Red;
        } else {
            leds[i] = CRGB::White;
        }
    }
}