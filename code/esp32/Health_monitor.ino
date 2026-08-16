#include <Wire.h>

const uint8_t PIN_HEART_ADC   = 34;
const uint8_t PIN_THERMISTOR  = 35;
const uint8_t PIN_SDA         = 32;
const uint8_t PIN_SCL         = 33;

const float NTC_NOMINAL_OHMS   = 10000.0;
const float NTC_NOMINAL_TEMP_C = 25.0;
const float NTC_B_COEFFICIENT  = 3950.0;
const float SERIES_RESISTOR    = 10000.0;
const float ADC_MAX_COUNTS     = 4095.0;

const uint16_t HEART_SAMPLE_INTERVAL_MS = 4;
const uint16_t BASELINE_WINDOW          = 64;
const uint32_t MIN_BEAT_INTERVAL_MS     = 300;
const uint32_t MAX_BEAT_INTERVAL_MS     = 2000;
const uint16_t CONTACT_ADC_FLOOR        = 200;

namespace MAX30102 {

    const uint8_t I2C_ADDR = 0x57;

    const uint8_t REG_INTR_ENABLE_1  = 0x02;
    const uint8_t REG_INTR_ENABLE_2  = 0x03;
    const uint8_t REG_FIFO_WR_PTR    = 0x04;
    const uint8_t REG_OVF_COUNTER    = 0x05;
    const uint8_t REG_FIFO_RD_PTR    = 0x06;
    const uint8_t REG_FIFO_DATA      = 0x07;
    const uint8_t REG_FIFO_CONFIG    = 0x08;
    const uint8_t REG_MODE_CONFIG    = 0x09;
    const uint8_t REG_SPO2_CONFIG    = 0x0A;
    const uint8_t REG_LED1_PA_RED    = 0x0C;
    const uint8_t REG_LED2_PA_IR     = 0x0D;
    const uint8_t REG_PART_ID        = 0xFF;

    bool present = false;

    bool writeReg(uint8_t reg, uint8_t val) {
        Wire.beginTransmission(I2C_ADDR);
        Wire.write(reg);
        Wire.write(val);
        return Wire.endTransmission() == 0;
    }

int readReg(uint8_t reg) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return -1;
    if (Wire.requestFrom((int)I2C_ADDR, 1) != 1) return -1;
    return Wire.read();
}

bool readFifoBurst(uint8_t* buf, uint8_t nBytes) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(REG_FIFO_DATA);
    if (Wire.endTransmission(false) != 0) return false;
    uint8_t got = Wire.requestFrom((int)I2C_ADDR, (int)nBytes);
    if (got != nBytes) return false;
    for (uint8_t i = 0; i < nBytes; i++) buf[i] = Wire.read();
    return true;
}

bool begin() {
    Wire.beginTransmission(I2C_ADDR);
    if (Wire.endTransmission() != 0) return false;

    int partId = readReg(REG_PART_ID);
    if (partId != 0x15) return false;

    writeReg(REG_MODE_CONFIG, 0x40);
    uint32_t t0 = millis();
    while (millis() - t0 < 200) {
        int m = readReg(REG_MODE_CONFIG);
        if (m >= 0 && !(m & 0x40)) break;
        delay(5);
    }

writeReg(REG_INTR_ENABLE_1, 0x00);
writeReg(REG_INTR_ENABLE_2, 0x00);

writeReg(REG_FIFO_WR_PTR, 0x00);
writeReg(REG_OVF_COUNTER, 0x00);
writeReg(REG_FIFO_RD_PTR, 0x00);

writeReg(REG_FIFO_CONFIG, 0x5F);

writeReg(REG_MODE_CONFIG, 0x03);

writeReg(REG_SPO2_CONFIG, 0x4B);

writeReg(REG_LED1_PA_RED, 0x24);
writeReg(REG_LED2_PA_IR, 0x24);

present = true;
return true;
}

uint8_t samplesAvailable() {
    int wr = readReg(REG_FIFO_WR_PTR);
    int rd = readReg(REG_FIFO_RD_PTR);
    if (wr < 0 || rd < 0) return 0;
    int diff = wr - rd;
    if (diff < 0) diff += 32;
    return (uint8_t)diff;
}

bool readSample(uint32_t &red, uint32_t &ir) {
    uint8_t raw[6];
    if (!readFifoBurst(raw, 6)) return false;
    red = ((uint32_t)raw[0] << 16 | (uint32_t)raw[1] << 8 | raw[2]) & 0x3FFFF;
    ir  = ((uint32_t)raw[3] << 16 | (uint32_t)raw[4] << 8 | raw[5]) & 0x3FFFF;
    return true;
}
}

namespace SpO2Estimator {
    const uint16_t WINDOW = 100;
    const uint32_t FINGER_IR_FLOOR = 5000;

    double dcRed = 0, dcIr = 0;
    double sumSqRed = 0, sumSqIr = 0;
    uint16_t count = 0;
    int32_t lastSpo2 = 0;
    int32_t smoothedSpo2 = 0;
    bool fingerPresent = false;

    void reset() { sumSqRed = 0; sumSqIr = 0; count = 0; }

    void feed(uint32_t red, uint32_t ir) {
        fingerPresent = ir > FINGER_IR_FLOOR;
        if (!fingerPresent) { reset(); return; }

        if (dcRed == 0) dcRed = red; else dcRed += ((double)red - dcRed) * 0.02;
        if (dcIr == 0)  dcIr = ir;   else dcIr  += ((double)ir  - dcIr)  * 0.02;

        double acRed = (double)red - dcRed;
        double acIr  = (double)ir  - dcIr;
        sumSqRed += acRed * acRed;
        sumSqIr  += acIr * acIr;
        count++;

        if (count >= WINDOW) {
            double acRmsRed = sqrt(sumSqRed / count);
            double acRmsIr  = sqrt(sumSqIr / count);
            if (dcRed > 0 && dcIr > 0 && acRmsIr > 0) {
                double R = (acRmsRed / dcRed) / (acRmsIr / dcIr);
                double spo2 = 110.0 - 25.0 * R;
                if (spo2 > 100) spo2 = 100;
                if (spo2 < 60) spo2 = 60;
                lastSpo2 = (int32_t)round(spo2);
                smoothedSpo2 = (smoothedSpo2 == 0) ? lastSpo2 : (int32_t)round(smoothedSpo2 * 0.8 + lastSpo2 * 0.2);
            }
        reset();
    }
}
}

float    heartBaseline      = 2048.0;
float    heartEnvelope      = 0.0;
uint32_t lastBeatMs         = 0;
uint32_t lastSampleMs       = 0;
uint16_t currentBPM         = 0;
float    bpmSmoothed        = 0.0;
bool     abovePeakLast      = false;
int16_t  lastWaveform       = 0;
bool     g_contactOK        = false;
uint8_t  signalQuality      = 0;
float    tempSmoothedF      = 98.6;
uint32_t lubdubCount        = 0;

void setup() {
    Serial.begin(115200);
    delay(200);

    analogReadResolution(12);
    pinMode(PIN_HEART_ADC, INPUT);
    pinMode(PIN_THERMISTOR, INPUT);

    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(400000);

    if (!MAX30102::begin()) {
        Serial.println("STATUS,MAX30102_NOT_FOUND");
    } else {
    Serial.println("STATUS,MAX30102_INIT_OK");
}

Serial.println("STATUS,ESP32_HEALTH_MONITOR_READY");
}

void loop() {
    uint32_t now = millis();

    if (now - lastSampleMs >= HEART_SAMPLE_INTERVAL_MS) {
        lastSampleMs = now;
        processHeartSample(now);
    }

if (MAX30102::present) {
    uint8_t avail = MAX30102::samplesAvailable();
    for (uint8_t i = 0; i < avail; i++) {
        uint32_t red, ir;
        if (MAX30102::readSample(red, ir)) SpO2Estimator::feed(red, ir);
    }
}

static uint32_t lastEmit = 0;
if (now - lastEmit >= 25) {
    lastEmit = now;
    emitCSV(now);
}
}

void processHeartSample(uint32_t now) {
    int raw = analogRead(PIN_HEART_ADC);

    bool contactOK = (raw > CONTACT_ADC_FLOOR) && (raw < (int)(ADC_MAX_COUNTS - CONTACT_ADC_FLOOR));

    heartBaseline += ((float)raw - heartBaseline) / BASELINE_WINDOW;
    float acSignal = (float)raw - heartBaseline;

    float rectified = fabs(acSignal);
    heartEnvelope += (rectified - heartEnvelope) * 0.05;

    float threshold = heartEnvelope * 1.6 + 8.0;
    bool abovePeak = acSignal > threshold;

    signalQuality = contactOK ? (uint8_t)constrain(heartEnvelope * 2.0, 0, 100) : 0;

    if (abovePeak && !abovePeakLast && contactOK) {
        uint32_t interval = now - lastBeatMs;

        if (interval >= MIN_BEAT_INTERVAL_MS) {

            lubdubCount++;
            Serial.print("BEAT,");
            Serial.println(now);

            if (lastBeatMs != 0 && interval <= MAX_BEAT_INTERVAL_MS) {
                float instantBPM = 60000.0 / (float)interval;
                if (instantBPM >= 30 && instantBPM <= 200) {
                    bpmSmoothed = (bpmSmoothed == 0) ? instantBPM : (bpmSmoothed * 0.75 + instantBPM * 0.25);
                    currentBPM = (uint16_t)round(bpmSmoothed);
                }
        }
    lastBeatMs = now;
}
}
abovePeakLast = abovePeak;

if (contactOK && lastBeatMs != 0 && (now - lastBeatMs) > MAX_BEAT_INTERVAL_MS) {
    currentBPM = 0;
    bpmSmoothed = 0;
}
if (!contactOK) {
    currentBPM = 0;
    bpmSmoothed = 0;
}

lastWaveform = (int16_t)constrain(acSignal, -2048, 2047);
g_contactOK = contactOK;
}

float readTemperatureF() {
    int raw = analogRead(PIN_THERMISTOR);
    if (raw <= 0) raw = 1;
    if (raw >= (int)ADC_MAX_COUNTS) raw = (int)ADC_MAX_COUNTS - 1;

    float resistance = SERIES_RESISTOR * ((ADC_MAX_COUNTS / (float)raw) - 1.0);

    float steinhart = resistance / NTC_NOMINAL_OHMS;
    steinhart = log(steinhart);
    steinhart /= NTC_B_COEFFICIENT;
    steinhart += 1.0 / (NTC_NOMINAL_TEMP_C + 273.15);
    steinhart = 1.0 / steinhart;
    float celsius = steinhart - 273.15;
    float fahrenheit = celsius * 9.0 / 5.0 + 32.0;

    if (fahrenheit < 50 || fahrenheit > 115) return tempSmoothedF;

    tempSmoothedF += (fahrenheit - tempSmoothedF) * 0.05;
    return tempSmoothedF;
}

void emitCSV(uint32_t now) {
    float tempF = readTemperatureF();
    int32_t spo2Out = SpO2Estimator::fingerPresent ? SpO2Estimator::smoothedSpo2 : 0;

    Serial.print(now);                         Serial.print(',');
    Serial.print(currentBPM);                  Serial.print(',');
    Serial.print(lastWaveform);                Serial.print(',');
    Serial.print(tempF, 2);                    Serial.print(',');
    Serial.print(spo2Out);                     Serial.print(',');
    Serial.print(signalQuality);               Serial.print(',');
    Serial.print(g_contactOK ? 1 : 0);         Serial.print(',');
    Serial.print(lubdubCount);                 Serial.print(',');
    Serial.println(SpO2Estimator::fingerPresent ? 1 : 0);
}