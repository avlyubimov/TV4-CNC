#pragma once
#include <stdbool.h>
#include <stdint.h>
#define WIFI_SAVED_MAX 8u
typedef struct { char ssid[33],password[64]; } wifi_profile_t;
typedef struct { uint32_t version,count; wifi_profile_t entries[WIFI_SAVED_MAX]; } wifi_profiles_t;
void wifi_profiles_init(wifi_profiles_t *profiles);
bool wifi_profiles_valid(const wifi_profiles_t *profiles);
int wifi_profiles_find(const wifi_profiles_t *profiles,const char *ssid);
bool wifi_profiles_remember(wifi_profiles_t *profiles,const char *ssid,const char *password);
bool wifi_profiles_forget(wifi_profiles_t *profiles,const char *ssid);

/* Visible names are ordered by descending signal strength. */
int wifi_profiles_choose(const wifi_profiles_t *profiles,const char visible[][33],unsigned count);
