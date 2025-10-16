# ESP32 Board Code

This folder contains the code for the ESP32 boards. There are two types of devices that this code supports: sensors and transceivers. Because ESP-IDF doesn't really allow for multiple programs, these devices share a common `app_main` function, and the mode can be switched by changing the `MODE` constant in [`main.c`](main/main.c). The specific code for each device is contained in its respective folder.

