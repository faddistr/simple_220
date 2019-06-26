#ifndef AVI_H
#define AVI_H
#include <stdint.h>
#include <esp_err.h>

esp_err_t gen_avi_file(const char *fname, uint32_t dur_secs, uint32_t frm_per_sec);


#endif /* AVI_H */