#pragma once

#include <esp_http_client.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief    HTTP OTA Firmware upgrade.
 *
 * This function performs HTTP OTA Firmware upgrade
 *
 * @param[in]  config       pointer to esp_http_client_config_t structure.
 *
 * @note     For secure HTTPS updates, the `cert_pem` member of `config`
 *           structure must be set to the server certificate.
 *
 * @return
 *    - ESP_OK: OTA data updated, next reboot will use specified partition.
 *    - ESP_FAIL: For generic failure.
 *    - ESP_ERR_OTA_VALIDATE_FAILED: Invalid app image
 *    - ESP_ERR_NO_MEM: Cannot allocate memory for OTA operation.
 *    - ESP_ERR_FLASH_OP_TIMEOUT or ESP_ERR_FLASH_OP_FAIL: Flash write failed.
 *    - For other return codes, refer OTA documentation in esp-idf's app_update component.
 */
esp_err_t esp_http_ota(const esp_http_client_config_t *config);

#ifdef __cplusplus
}
#endif
