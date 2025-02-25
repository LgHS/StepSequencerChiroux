#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <USBHost_t36.h>

#define LED_PIN 2
#define NUM_LEDS 16
#define BPM_MIN 50
#define BPM_MAX 180
#define START_STOP_PIN 12

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_RGB + NEO_KHZ800);
usb_midi_class myusbMIDI;

const uint8_t PCF_ADDRESSES_1[6] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25}; 
const uint8_t PCF_ADDRESSES_2[8] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27}; 

bool stepMatrix[16][7] = {0};
const uint8_t LINE_ORDER[7] = {3, 2, 1, 0, 6, 5, 4};
const int POT_PINS[7] = {A15, A1, A16, A17, A6, A7, A8};
uint8_t MIDI_NOTES[7] = {0, 8, 16, 24, 32, 40, 48};

int currentColumn = 0;
bool isRunning = false;  
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

void configurePCF(uint8_t address, TwoWire &bus);
void checkStartStopButton();
void updateMidiNotes();
int readBPM();
void readButtons();
void displayColumn(int column, int greenValue);
void sendMidiNotes(int column);

void setup() {
    Serial.begin(115200);
    Wire.begin();
    Wire1.setSDA(17);
    Wire1.setSCL(16);
    Wire1.begin();
    pinMode(START_STOP_PIN, INPUT_PULLUP);
    strip.begin();
    strip.show(); 
    myusbMIDI.begin(); 
    for (uint8_t i = 0; i < 6; i++) configurePCF(PCF_ADDRESSES_1[i], Wire);
    for (uint8_t i = 0; i < 8; i++) configurePCF(PCF_ADDRESSES_2[i], Wire1);
}

void configurePCF(uint8_t address, TwoWire &bus) {
    bus.beginTransmission(address);
    bus.write(0xFF);  
    bus.endTransmission();
}

void loop() {
    checkStartStopButton();  
    if (!isRunning) {  
        return;  
    }
    updateMidiNotes();  
    int bpm = readBPM();
    int stepDelay = (60000 / bpm) / 4;
    int greenValue = map(bpm, BPM_MIN, BPM_MAX, 165, 50);
    readButtons();  
    displayColumn(currentColumn, greenValue);
    sendMidiNotes(currentColumn);  
    currentColumn = (currentColumn + 1) % 16;
    delay(stepDelay);
}

void updateMidiNotes() {
    for (int i = 0; i < 7; i++) {
        int rawValue = analogRead(POT_PINS[i]);
        MIDI_NOTES[i] = map(rawValue, 0, 1023, i * 8, (i * 8) + 7);  
    }
}

void checkStartStopButton() {
    static bool lastButtonState = HIGH;
    bool buttonState = digitalRead(START_STOP_PIN);
    if (buttonState == LOW && lastButtonState == HIGH) {  
        isRunning = !isRunning;  
        Serial.println(isRunning ? "▶️ START" : "⏹️ STOP");
        if (!isRunning) {
            currentColumn = 0;
            strip.clear();
            strip.setPixelColor(0, strip.Color(0, 30, 120)); 
            strip.setPixelColor(4, strip.Color(0, 30, 120)); 
            strip.setPixelColor(8, strip.Color(0, 30, 120)); 
            strip.setPixelColor(12, strip.Color(0, 30, 120)); 
            strip.show();
        }
    }
    lastButtonState = buttonState;
}

int readBPM() {
    int rawValue = analogRead(A9);
    return map(rawValue, 0, 1023, BPM_MIN, BPM_MAX);
}

void readButtons() {
    for (uint8_t i = 0; i < 6; i++) {
        Wire.requestFrom(PCF_ADDRESSES_1[i], (uint8_t)1);
        if (Wire.available()) {
            uint8_t state = Wire.read();
            uint8_t colOffset = (i % 2) * 8;  
            uint8_t row = LINE_ORDER[4 + (i / 2)]; 
            for (uint8_t col = 0; col < 8; col++) {
                stepMatrix[colOffset + col][row] = (state & (1 << col)) ? 1 : 0;  
            }
        }
    }

    for (uint8_t i = 0; i < 8; i++) {
        Wire1.requestFrom(PCF_ADDRESSES_2[i], (uint8_t)1);
        if (Wire1.available()) {
            uint8_t state = Wire1.read();
            uint8_t colOffset = (i % 2) * 8;
            uint8_t row = LINE_ORDER[i / 2]; 
            for (uint8_t col = 0; col < 8; col++) {
                stepMatrix[colOffset + col][row] = (state & (1 << col)) ? 1 : 0;  
            }
        }
    }
}

void displayColumn(int column, int greenValue) {
    strip.clear();
    strip.setPixelColor(0, strip.Color(0, 30, 120)); 
    strip.setPixelColor(4, strip.Color(0, 30, 120)); 
    strip.setPixelColor(8, strip.Color(0, 30, 120)); 
    strip.setPixelColor(12, strip.Color(0, 30, 120)); 
    strip.setPixelColor(column, strip.Color(255, greenValue, 0)); 
    strip.show();
}

void sendMidiNotes(int column) {
    for (int row = 0; row < 7; row++) {
        if (stepMatrix[column][row]) {  
            myusbMIDI.sendNoteOn(MIDI_NOTES[row], 127, 1);  
            delay(10);  
            myusbMIDI.sendNoteOff(MIDI_NOTES[row], 0, 1);   
        }
    }
}
