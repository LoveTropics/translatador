#ifndef WHATLANG_H
#define WHATLANG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum WlError {
  WL_OK = 0,
  WL_MALFORMED_STRING = 1,
  WL_DETECTION_FAILED = 2,
} WlError;

typedef struct WlInfo {
  char lang;
  float confidence;
} WlInfo;

WlError wl_detect(const char* string, WlInfo* result);

#ifdef __cplusplus
}
#endif

#endif
