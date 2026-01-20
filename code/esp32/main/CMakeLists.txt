idf_component_register(
    SRCS "main.c"
         "receiver/receiver.c"
         "receiver/storage.c"
         "receiver/display.c"
         "receiver/gps.c"
         "sensor/sensor.c"
         "sensor/camera.c"
         "sensor/logger.c"
         "sensor/camera.c"
          "sensor/camera.c"
          "sensor/camera.h"
    INCLUDE_DIRS "."
                 "receiver"
                 "sensor"
#    REQUIRES esp32-camera ssd1306 esp_wifi nvs_flash esp_timer fatfs
)
