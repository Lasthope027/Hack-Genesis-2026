# Hack Genesis 2026 — ESP32 Health Monitor

## Overview

The **ESP32 Health Monitor** is a physiological monitoring system designed to acquire, process, and display multiple health-related parameters using an ESP32-based platform.

The system monitors:

* Heart rate
* Blood oxygen saturation (SpO₂)
* Body temperature

The project combines analog signal conditioning, sensor interfacing, embedded processing, and a web-based interface.

## System Architecture

The overall system follows a sensor-to-analysis pipeline involving physiological sensing, signal conditioning, data acquisition, processing, and health information presentation.

<img width="350" height="637" alt="image" src="https://github.com/user-attachments/assets/5ef6dec5-50ee-4f30-bd12-0b47047abadd" />

## Hardware

The system combines dedicated sensors with analog signal-conditioning circuitry.

### Heart Rate Monitoring

An diaphragm is used to acquire the heart-sound signal. The signal is amplified and conditioned through an analog front-end before being processed by the ESP32.

The signal path consists of:

**Diaphragm → Amplifier → Filter / Envelope Detection → ESP32**

### Temperature Monitoring

Body temperature is measured using an **NTC thermistor configured in a voltage-divider circuit**. The ESP32 processes the measured signal to obtain the corresponding temperature.

### SpO₂ Monitoring

SpO₂ is measured using the **MAX30102 pulse-oximeter sensor**. The sensor provides red and infrared measurements that are processed by the ESP32 to estimate blood oxygen saturation.

## Circuit Design

The analog front-end was designed and simulated using **LTspice**.

<img width="1332" height="860" alt="image" src="https://github.com/user-attachments/assets/f2b14fc1-c87e-4249-934c-0aff9f7146b9" />

The circuit contains sections for:

* Heart-rate sensing and amplification
* Filtering and envelope detection
* LED-bar indication
* Temperature / fever detection

## SPICE Models

Some components used in the LTspice simulation were not available in the default LTspice library. External SPICE models were therefore used for components including:

* LM386
* LM358
* LM3915

These models are stored in the `circuit/SPICE-Models/` directory and are required for simulating the corresponding circuit sections.

## Embedded Software

The ESP32 firmware handles sensor interfacing, data acquisition, signal processing, and communication with the web interface.

The web interface provides a way to present the acquired health-related data.

### ESP32 Firmware

The Arduino code for the ESP32 is located in:

`code/ESP32/Health Monitor/`

### Web Interface

The HTML-based health-monitoring interface is located in:

`code/Health Monitor/`

## Signal Processing

### Heart Rate

The heart-sound signal is processed by removing its slow-moving baseline, rectifying the signal, and tracking its envelope. An adaptive threshold is then used for peak detection, with a minimum refractory period between detected beats. The detected beat timestamps are used to calculate heart rate.

### Temperature

The NTC thermistor is used in a voltage-divider configuration. The measured signal is processed by the ESP32 to determine the corresponding temperature.

### SpO₂

The MAX30102 red and infrared signals are processed to estimate blood oxygen saturation using the ratio-of-ratios approach. The MAX30102's internal heart-rate estimate is not used for the project's BPM calculation.

## Project Structure

```text
Hack-Genesis-2026/
│
├── README.md
│
├── circuit/
│   ├── spice models/
│   │   ├── LM386 (1).asy
│   │   ├── LM386.sub
│   │   ├── LM3915.asy
│   │   ├── LM3915.sub
│   │   ├── lm358.sub
│   │   └── opamp2.asy
│   │
│   └── Health monitor circuit.asc
│
└── code/
    ├── esp32/
    │   └── Health_monitor.ino
    │
    └── webpage/
        └── Health_monitor_webpage.html
```

## Tools and Technologies

* ESP32
* Arduino
* LTspice
* HTML
* LM386
* LM358
* LM3915
* MAX30102
* NTC Thermistor
* Diaphragm

## Purpose

This project demonstrates the integration of **analog electronics, physiological sensing, embedded systems, signal processing, and a web-based user interface** into a single health-monitoring system.

The repository contains the circuit design, required SPICE models, ESP32 firmware, and web interface used in the project.
