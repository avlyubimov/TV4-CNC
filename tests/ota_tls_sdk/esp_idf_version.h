#pragma once
#define ESP_IDF_VERSION_VAL(major,minor,patch) (((major)<<16)|((minor)<<8)|(patch))
#if TLS_TEST_SDK == 5
#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(5,3,2)
#else
#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(6,1,1)
#endif
