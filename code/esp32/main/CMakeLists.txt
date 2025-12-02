idf_component_register(
    SRCS "main.c"
         "receiver/receiver.c"
         "receiver/storage.c"
         "receiver/display.c"
         "receiver/gps.c"
         "sensor/sensor.c"
    INCLUDE_DIRS "."
                 "receiver"
                 "sensor"
)
