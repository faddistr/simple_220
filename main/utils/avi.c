#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_log.h>
#include <esp_camera.h>
#include <esp_types.h>
#include <driver/periph_ctrl.h>
#include <driver/timer.h>
#include "avi.h"


static const char *TAG = "AVI";
const char avi_video_chunk_hdr[4] = {'0','0','w','b'};

#define AVI_ISR_TIMER TIMER_0

ESP_EVENT_DEFINE_BASE(AVI_BASE);

typedef struct
{
	char dwRIFF[4];
	uint32_t full_len;
	char fourCC_avi[4];
	char dwList[4];
	uint32_t hdr_size;
	char hdrl[4];
	char avih[4];
	uint32_t dw_size;
} avi_hdr_overhead_t;

typedef struct {    
	uint32_t dwMicroSecPerFrame;    
	uint32_t dwMaxBytesPerSec;    
	uint32_t dwReserved1;    
	uint32_t dwFlags;    
	uint32_t dwTotalFrames;    
	uint32_t dwInitialFrames;    
	uint32_t dwStreams;    
	uint32_t dwSuggestedBufferSize;    
	uint32_t dwWidth;    
	uint32_t dwHeight;    
	uint32_t dwReserved[4];
} avi_hdr_main_t;

typedef struct {  
	char   list[4];
	uint32_t dwSize;
	char strl[4];
	char strh[4];
	uint32_t strh_dwSize;
	char fccType[4];  
	char fccHandler[4];  
	uint32_t dwFlags;  
	uint16_t wPriority;  
	uint16_t wLanguage;
	uint32_t dwInitialFrames;  
	uint32_t dwScale;  
	uint32_t dwRate;  
	uint32_t dwStart;  
	uint32_t dwLength;  
	uint32_t dwSuggestedBufferSize;  
	uint32_t dwQuality;  
	uint32_t dwSampleSize;  
	char    strf[4];
	uint32_t strf_dwSize;
	uint32_t strf_biSize;
	uint32_t biWidth;
	uint32_t biHeight;
	uint16_t biPlanes;
	uint16_t biBitCount;
	char     biCompression[4];
	uint32_t biSizeImage;
	uint32_t biXPelsPerMeter;
	uint32_t biYPelsPerMeter;
	uint32_t biClrUsed;
	uint32_t biClrImportant;
	char     odml_list[4];
	uint32_t ddww;
	char     odml[8];
	uint32_t szs;
	uint32_t totalframes;
	char  movi_list[4];
	uint32_t movi_dwSize;
	char  movi[4];
} avi_hdr_stream_t;

static const avi_hdr_overhead_t avi_hdr_overhead = 
{
	.dwRIFF                = { 'R', 'I', 'F', 'F'}, 
	.full_len              = 0,
	.fourCC_avi            = {'A', 'V', 'I', ' '},
	.dwList                = {'L', 'I', 'S', 'T'},
	.hdr_size              = 208,
	.hdrl                  = {'h', 'd', 'r', 'l'},
	.dw_size               = 56,
};

static const avi_hdr_stream_t avi_hdr_stream_def = 
{
	.list                  = {'L', 'I', 'S', 'T'},
	.dwSize                = 132,
	.strl                  = {'s', 't', 'r', 'l'},
	.strh                  = {'s', 't', 'r', 'h'},
	.strh_dwSize           = 48,
	.fccType               = {'v', 'i', 'd', 's'},
	.fccHandler            = {'M', 'J', 'P', 'G'},
	.dwFlags               = 0,
	.wPriority             = 0, 
	.wLanguage             = 0,
	.dwInitialFrames       = 0,
	.dwScale               = 1,
	.dwRate                = 0,
	.dwLength              = 0,
	.dwStart               = 0,
	.dwSuggestedBufferSize = 0,
	.dwQuality             = 0,
	.dwSampleSize          = 0,
	.strf                  = {'s', 't', 'r', 'f'},
	.strf_dwSize           = 40,
	.strf_biSize           = 40,
	.biWidth               = 800,
	.biHeight              = 600,
	.biPlanes              = 1,
	.biBitCount            = 24,
	.biCompression         = {'M', 'J', 'P', 'G'},
	.odml_list             = {'L', 'I', 'S', 'T'},
	.ddww                  = 16,
	.odml                  = {'o', 'd', 'm', 'l', 'd', 'm', 'l', 'h'},
	.szs                   = 4,
	.movi_list             = {'L', 'I', 'S', 'T'},
	.movi_dwSize           = 0,
	.movi                  = {'m', 'o', 'v', 'i'},
};

typedef struct 
{
	FILE *fp;
	bool is_timer;
	TaskHandle_t task;	
	uint32_t count;
	uint32_t delay;
	char *fname;
	uint32_t fps;
	TimerHandle_t timer;
	BaseType_t woken;
	SemaphoreHandle_t  sem;
} avii_t;

static void avi_timer_work_cb(TimerHandle_t pxTimer) 
{
	avii_t *avii = (avii_t *)pvTimerGetTimerID(pxTimer);

	do
	{
		camera_fb_t *fb = NULL;

		if (fwrite(avi_video_chunk_hdr, sizeof(avi_video_chunk_hdr), 1, avii->fp) != 1)
		{
			ESP_LOGE(TAG, "Failed to write file! (1)");
			avii->count = 0;
			break;
		}

		fb = esp_camera_fb_get();
		if (fwrite(&fb->len, sizeof(fb->len), 1, avii->fp) != 1)
		{
			esp_camera_fb_return(fb);
			ESP_LOGE(TAG, "Failed to write file! (2)");
			avii->count = 0;
			break;
		}

		if (fwrite(fb->buf, fb->len, 1, avii->fp) != 1)
		{
			esp_camera_fb_return(fb);
			ESP_LOGE(TAG, "Failed to write file! (3)");
			avii->count = 0;
			break;
		}

		esp_camera_fb_return(fb);
		avii->count--;
	} while (0);

	if (avii->count == 0)
	{
		ESP_ERROR_CHECK(esp_event_isr_post_to(simple_loop_handle, AVI_BASE, 
					AVI_FINISH, avii->fname, strlen(avii->fname) + 1, NULL));	
		free(avii->fname);
		free(avii);
		xTimerStop(pxTimer, 0);
		xTimerDelete(pxTimer, 0);
	}
}


static void avi_timer_cb(TimerHandle_t pxTimer) 
{
	avii_t *avii = (avii_t *)pvTimerGetTimerID(pxTimer);
	avii->is_timer = true;
}

#define TIMER_DIVIDER         8000  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds

void IRAM_ATTR timer_group0_isr(void *para)
{
	avii_t *avii = (avii_t *)para;
    TIMERG0.int_clr_timers.t0 = 1;
    TIMERG0.hw_timer[AVI_ISR_TIMER].update = 1;
    TIMERG0.hw_timer[AVI_ISR_TIMER].config.alarm_en = TIMER_ALARM_EN;

    avii->woken = pdTRUE;
    xSemaphoreGiveFromISR(avii->sem, &avii->woken);
    portYIELD_FROM_ISR();
}


static void avi_isr_timer_init(avii_t *avii, uint64_t fps)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = true;
    timer_init(TIMER_GROUP_0, AVI_ISR_TIMER, &config);
    timer_set_counter_value(TIMER_GROUP_0, AVI_ISR_TIMER, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0, AVI_ISR_TIMER, TIMER_SCALE / fps);
    ESP_LOGI(TAG, "ALARM VAL = %lld %d %lld", TIMER_SCALE / fps, TIMER_SCALE, fps);
    timer_enable_intr(TIMER_GROUP_0, AVI_ISR_TIMER);
    timer_isr_register(TIMER_GROUP_0, AVI_ISR_TIMER, timer_group0_isr, 
        (void *) avii, ESP_INTR_FLAG_IRAM, NULL);

    timer_start(TIMER_GROUP_0, AVI_ISR_TIMER);
}



static void avi_task(void * param)
{
	avii_t *avii = (avii_t *)param;
	TaskHandle_t task = avii->task;
	#if 0
	TimerHandle_t timer = xTimerCreate("AviiTimer", avii->delay / portTICK_RATE_MS,
        	pdTRUE, avii, avi_timer_cb);

	+TimerStart(timer, 0);
	#else
	vSemaphoreCreateBinary(avii->sem);
	avi_isr_timer_init(avii, avii->fps);
	#endif
	while(avii->count)
	{
#if 0
		if (avii->is_timer)
#else
	    if( xSemaphoreTake( avii->sem, portTICK_RATE_MS * avii->delay) == pdTRUE )
#endif
		{
			camera_fb_t *fb = NULL;

			if (fwrite(avi_video_chunk_hdr, sizeof(avi_video_chunk_hdr), 1, avii->fp) != 1)
			{
				ESP_LOGE(TAG, "Failed to write file! (1)");
				break;
			}

			fb = esp_camera_fb_get();
			if (fwrite(&fb->len, sizeof(fb->len), 1, avii->fp) != 1)
			{
				esp_camera_fb_return(fb);
				ESP_LOGE(TAG, "Failed to write file! (2)");
				break;
			}

			if (fwrite(fb->buf, fb->len, 1, avii->fp) != 1)
			{
				esp_camera_fb_return(fb);
				ESP_LOGE(TAG, "Failed to write file! (3)");
				break;
			}
			esp_camera_fb_return(fb);
			avii->is_timer = false;
			avii->count--;
		}
	}
#if 0
	xTimerStop(timer, 0);
	xTimerDelete(timer, 0);
#else
	timer_pause(TIMER_GROUP_0, AVI_ISR_TIMER);
	vSemaphoreDelete(avii->sem);
#endif
	fclose(avii->fp);
	
	ESP_ERROR_CHECK(esp_event_post_to(simple_loop_handle, AVI_BASE, 
					AVI_FINISH, avii->fname, strlen(avii->fname) + 1, portMAX_DELAY));	
	free(avii->fname);
	free(avii);
	vTaskDelete(task);
}

esp_err_t gen_avi_file(const char *fname, uint32_t dur_secs, uint32_t frm_per_sec, bool isr)
{

	avi_hdr_stream_t *stream_hdr = NULL;
	camera_fb_t *fb = esp_camera_fb_get();
	avi_hdr_main_t 

	hdr = {
		.dwMicroSecPerFrame = 1000000UL / frm_per_sec,
		.dwMaxBytesPerSec = fb->len * frm_per_sec * 1,
		.dwFlags = 0, /* TODO */
		.dwStreams = 1,
		.dwWidth = fb->width,
		.dwHeight = fb->height,
	};

	avii_t *avii = (avii_t *)calloc(1, sizeof(avii_t));

	esp_camera_fb_return(fb);

	if (avii == NULL)
	{
		ESP_LOGE(TAG, "No mem");
		return ESP_FAIL;
	}
	
	avii->fp = fopen(fname, "wb");
	if (avii->fp == NULL)   
	{
		free(avii);
		ESP_LOGE(TAG, "Couldn't open file");
		return ESP_FAIL;
	}

	if (fwrite(&avi_hdr_overhead, sizeof(avi_hdr_overhead_t), 1, avii->fp) != 1)
	{
		fclose(avii->fp);
		free(avii);
		ESP_LOGE(TAG, "Fwrite error");
		return ESP_FAIL;
	}

	if (fwrite(&hdr, sizeof(avi_hdr_main_t), 1, avii->fp) != 1)
	{
		ESP_LOGE(TAG, "Fwrite error");
		return ESP_FAIL;
	}

	stream_hdr = malloc(sizeof(avi_hdr_stream_t));
	if (stream_hdr == NULL)
	{
		free(avii);
		fclose(avii->fp);
		free(avii);
		return ESP_ERR_NO_MEM;
	}

	memcpy(stream_hdr, &avi_hdr_stream_def, sizeof(avi_hdr_stream_t));
	stream_hdr->biWidth     =  hdr.dwWidth;
	stream_hdr->biHeight    =  hdr.dwHeight;
	stream_hdr->dwRate      =  frm_per_sec;
	stream_hdr->biSizeImage =  ((stream_hdr->biWidth*stream_hdr->biBitCount/8 + 3)&0xFFFFFFFC)*stream_hdr->biHeight;
	if (fwrite(stream_hdr, sizeof(avi_hdr_stream_t), 1, avii->fp) != 1)
	{
		ESP_LOGE(TAG, "Fwrite error");
		free(stream_hdr);
		fclose(avii->fp);
		free(avii);
		return ESP_FAIL;
	}
	free(stream_hdr);

	ESP_LOGI(TAG, "File name: %s", fname);
	ESP_LOGI(TAG, "Per frame: %d FPS %d Duration: %d", hdr.dwMicroSecPerFrame, frm_per_sec, dur_secs);
	avii->count = frm_per_sec * dur_secs;
	avii->delay = hdr.dwMicroSecPerFrame / 1000;
	avii->fname = strdup(fname);
	avii->fps = frm_per_sec;
	ESP_LOGI(TAG, "Frames count: %d, Delay: %d", avii->count, avii->delay);
#if 0
	if (!isr)
	{
		xTaskCreatePinnedToCore(&avi_task, "avii_task", 2048, avii, 1, &avii->task, 1);
	} else
	{
		avii->timer = xTimerCreate("AviiTimerI", avii->delay / portTICK_RATE_MS,
        	pdTRUE, avii, avi_timer_work_cb);
		xTimerStart(avii->timer, 0);
	}
#else
	xTaskCreatePinnedToCore(&avi_task, "avii_task", 2048, avii, 1, &avii->task, 1);
#endif
	return ESP_OK;
}