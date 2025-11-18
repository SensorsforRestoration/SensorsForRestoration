# Colorimetric debug — overview

This folder contains a small ESP32 camera utility used to inspect the center color of a scene and serve full frames over HTTP. It was written for the Seeed XIAO ESP32‑S3 Sense board with a DVP camera.

Files

- `main.cpp` — main firmware. Captures frames, averages a small window around the image center, maps the averaged RGB to a nearest named color from a small palette, and exposes simple HTTP endpoints.
- `camera_pins.h` — board pin mapping used by `main.cpp` (pin defines for the XIAO ESP32‑S3 Sense).

What the firmware does (high level)

- Initializes the camera in RGB565 mode, requests a frame buffer in PSRAM and applies a few sensor tuning parameters (AWB, exposure, contrast, denoise).
- Auto-detects the pixel packing/format (RGB vs BGR, little vs big endian) by sampling the center region and scoring candidate interpretations for neutral variance and brightness.
- In the main loop the code reads a frame, averages a (2*R+1)x(2*R+1) window around the center (configurable `AVG_RADIUS`) and computes the mean R,G,B values.
- The mean RGB is converted to a nearest human-friendly name using a small palette (e.g., Red, Green, Blue, Yellow, Cyan, Magenta, etc.) and printed on Serial for quick debugging.

HTTP endpoints

- `/` — simple HTML index describing available endpoints.
- `/frame.ppm` — current frame served as a binary PPM (P6) stream. Useful for quick viewing with a small Python client or `display` programs.
- `/color.json` — returns JSON `{r,g,b,name}` with the averaged center RGB and the named color.
- `/settings` — accepts query parameters to tweak sensor settings (brightness, contrast, saturation, sharpness) and returns a JSON status.

Key implementation details

- Pixel format detection: The camera often returns RGB565 but some drivers present BGR565 or different byte ordering. The code tests 4 variants (RGB/BGR × little/big endian) and picks the interpretation that yields low color variance and reasonable brightness for the center region.
- RGB565 → RGB888 conversion: `rgb565_to_rgb888_format` unpacks 5/6/5 bits into 8-bit channels and applies byte-swapping or R/B swapping depending on the detected format.
- Averaging: Averaging a small center window (default radius 3 → 7×7) reduces noise and yields a stable color sample for colorimetric checks.
- Auto-freeze: `AUTO_FREEZE_AFTER_MS` can freeze auto‑exposure / white balance after warmup to stabilize color readings across requests.

How to run & test

- Build & flash via Arduino/ESP build system for the XIAO ESP32‑S3 Sense (ensure PSRAM is available). The firmware starts a Wi‑Fi AP `ESP_CAM_DEBUG` by default.
- Connect to the AP, open the device IP (printed to Serial), then visit `/` or `/frame.ppm` in a browser or use a small script to fetch and display frames.
- Example JSON color query: `http://<ip>/color.json` returns the averaged center color and its name.

Tips and caveats

- Memory: the code requests the framebuffer in PSRAM (`CAMERA_FB_IN_PSRAM`). If PSRAM is unavailable, reduce frame size or fb_count.
- Lighting & optics: color readings depend strongly on illumination and white-balance. Freeze auto‑WB and exposure for reproducible comparisons, and use a neutral reference when calibrating.
- For more accurate colorimetry, consider converting raw RGB to a linear space and applying a simple color calibration matrix derived from known reference patches.

Suggested improvements

- Add a small CSV logging endpoint to record time‑stamped color readings for later analysis.
- Provide a command‑line viewer (`tools/view_cam.py`) that fetches `/frame.ppm` and displays it, with an option to poll `/color.json` and plot time series.
- Add AWB / color correction parameters or a per‑device calibration file to map camera RGB → standardized color space.

If you want, I can add a README entry with build/flash commands or generate a small `tools/view_cam.py` viewer to fetch frames and show the named color in real time.

