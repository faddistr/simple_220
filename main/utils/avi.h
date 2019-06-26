#ifndef AVI_H
#define AVI_H
#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>
#include "module.h"

enum
{
	AVI_FINISH,
};

ESP_EVENT_DECLARE_BASE(AVI_BASE);

esp_err_t gen_avi_file(const char *fname, uint32_t dur_secs, uint32_t frm_per_sec, bool isr);


#endif /* AVI_H */