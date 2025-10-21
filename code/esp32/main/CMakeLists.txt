idf_component_register(
    SRCS "main.c"
         "receiver/receiver.c"
         "sensor/sensor.c"
    INCLUDE_DIRS "."
                 "receiver"
                 "sensor"
)
