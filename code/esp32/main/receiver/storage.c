#include "esp_now.h"
#include "nvs_flash.h"

nvs_handle_t namespace;

static const char *NAMESPACE = "storage";

esp_err_t storage_init(void)
{
    return nvs_open(NAMESPACE, NVS_READWRITE, &namespace);
}