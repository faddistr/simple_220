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
#include <esp_log.h>
#include "avi.h"
#include <esp_camera.h>

static const char *TAG = "AVI";

const char avi_video_chunk_hdr[4] = {'0','0','w','b'};
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

esp_err_t gen_avi_file(const char *fname, uint32_t dur_secs, uint32_t frm_per_sec)
{
	FILE *fp;
	avi_hdr_stream_t *stream_hdr = NULL;
	camera_fb_t *fb = esp_camera_fb_get();
	avi_hdr_main_t hdr = {
		.dwMicroSecPerFrame = 1000000UL / frm_per_sec,
		.dwMaxBytesPerSec = fb->len * frm_per_sec * 2,
		.dwFlags = 16, /* TODO */
		.dwStreams = 1,
		.dwWidth = fb->width,
		.dwHeight = fb->height,
	};
	
	fp = fopen(fname, "wb");
	if (fp == NULL)   
	{
		esp_camera_fb_return(fb);
		ESP_LOGE(TAG, "Couldn't open file");
		return ESP_FAIL;
	}

	if (fwrite(&avi_hdr_overhead, sizeof(avi_hdr_overhead_t), 1, fp) != 1)
	{
		esp_camera_fb_return(fb);
		fclose(fp);
		ESP_LOGE(TAG, "Fwrite error");
		return ESP_FAIL;
	}

	if (fwrite(&hdr, sizeof(avi_hdr_main_t), 1, fp) != 1)
	{
		ESP_LOGE(TAG, "Fwrite error");
		return ESP_FAIL;
	}

	stream_hdr = malloc(sizeof(avi_hdr_stream_t));
	if (stream_hdr == NULL)
	{
		esp_camera_fb_return(fb);
		fclose(fp);
		return ESP_ERR_NO_MEM;
	}

	memcpy(stream_hdr, &avi_hdr_stream_def, sizeof(avi_hdr_stream_t));
	stream_hdr->biWidth     =  fb->width;
	stream_hdr->biHeight    =  fb->height;
	stream_hdr->dwRate      =  frm_per_sec;
	stream_hdr->biSizeImage =  ((stream_hdr->biWidth*stream_hdr->biBitCount/8 + 3)&0xFFFFFFFC)*stream_hdr->biHeight;
	esp_camera_fb_return(fb);
	if (fwrite(stream_hdr, sizeof(avi_hdr_stream_t), 1, fp) != 1)
	{
		ESP_LOGE(TAG, "Fwrite error");
		free(stream_hdr);
		fclose(fp);
		return ESP_FAIL;
	}

	free(stream_hdr);
	fclose(fp);

	return 0;
}