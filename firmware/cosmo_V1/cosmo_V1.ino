#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <TJpg_Decoder.h>
#include <time.h>
#include <math.h>

#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>

struct Button {
  int pin;
  bool stableState;
  bool lastReading;
  unsigned long lastChange;
};

// ======================================
// CONFIGURATION
// ======================================

#define DEVICE_NAME "COSMO"

#if __has_include("secrets.h")
#include "secrets.h"
#else
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
#endif

const long GMT_OFFSET = 19800;
const int DAYLIGHT_OFFSET = 0;

// ======================================
// DISPLAY
// ======================================

#define TFT_CS  5
#define TFT_DC  32
#define TFT_RST 4

Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

const int SCREEN_W = 240;
const int SCREEN_H = 240;

// ======================================
// ONLINE WALLPAPER CONFIGURATION
// ======================================

#define COSMO_WALLPAPER_URL "https://raw.githubusercontent.com/reva-hattekar/COSMO/main/assets/wallpaper_1.png"
const char* WALLPAPER_CACHE_PATH = "/wallpaper.jpg";

bool wallpaperLoaded = false;
bool wallpaperAttempted = false;
uint32_t tjpgCallbackCount = 0;

// Clock Background Clearing Cache (eliminates overlapping text/ghosting)
#define CLOCK_BG_X 25
#define CLOCK_BG_Y 124
#define CLOCK_BG_W 190
#define CLOCK_BG_H 56

static uint16_t* s_clockBg = nullptr;
static bool s_clockBgValid = false;
static char s_lastTimeString[8] = "";
static char s_lastDateString[20] = "";

const char* GITHUB_ROOT_CA =
// ISRG Root X1 (Let's Encrypt Root)
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMHSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
"-----END CERTIFICATE-----\n"
// Root YR (Let's Encrypt 2026 Active CA cross-signed by ISRG Root X1)
"-----BEGIN CERTIFICATE-----\n"
"MIIF9DCCA9ygAwIBAgIRAPJLbRf52a18scn+p4eCaZ8wDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMjYwNTEzMDAwMDAw\n"
"WhcNMzIwOTAyMjM1OTU5WjAuMQswCQYDVQQGEwJVUzENMAsGA1UEChMESVNSRzEQ\n"
"MA4GA1UEAxMHUm9vdCBZUjCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIB\n"
"ANvGJnN78CTJdWL3+eGfsLN5TrNBJs+VH9hRXqRbwxu9sGNiB0BD1fcOxbSUQCJI\n"
"M1xE13Db+5Cw1w0s0EBYsvuIP/6joF0w8cuImbgR1OGgYbSQ4OpzI+DG8SGuTlcE\n"
"873OCS+kh3srlo6vl43M5OJg4Aeo1sfHp6kTJDoIiFBNJAY+OKfX/FUvYKuhjT+n\n"
"o49lmqmupSBI5PkBQiqrEGtWU5uxU/cQWHGu8jSjFBznZqvbNPLMXMLFxCb3WTfr\n"
"JBXXjqvWG+v4bjzxjjeAtOlU7qarRDvNOyAuQYLln904M+faKx8hnLCpJ15ZqaEg\n"
"cNlY+9MMWcC5yvL2A2j3l9+2buggZX+dOE91zYmIdawTvSZuVvlbRrAlLxIB6pwM\n"
"BjneXCjYQ8+3BCCjssbSNpZU3hTcBDdhfAlEDlYr6pEatnMdmDT5BqnKC92bd0Eh\n"
"M1fbLHioLccLCuievT8ZkPhZrq7Mii7gNXAcUEAR8+lzYal+9zTg7C5DALyVOeG/\n"
"CqfRAMn1KSHCR0NSA6P8tn/mGRlnCct5rtVCLnVySVpU6H1qGg3DgTOuskf8eahT\n"
"MiYbI5ezPJmO5ertalskQ1utp74+eDy92PI4ftHKTbq9IWhH4YZKh3WnJEIt+oQv\n"
"lYZbY8tpEroKrFB6PFGzrJIDRyts4HqvuH52RFj2zv/BAgMBAAGjgeswgegwDgYD\n"
"VR0PAQH/BAQDAgEGMBMGA1UdJQQMMAoGCCsGAQUFBwMBMA8GA1UdEwEB/wQFMAMB\n"
"Af8wHQYDVR0OBBYEFN7nW2DQIm1AKH0/DQH+pLVStFGUMB8GA1UdIwQYMBaAFHm0\n"
"WeZ7tuXkAXOACIjIGlj26ZtuMDIGCCsGAQUFBwEBBCYwJDAiBggrBgEFBQcwAoYW\n"
"aHR0cDovL3gxLmkubGVuY3Iub3JnLzATBgNVHSAEDDAKMAgGBmeBDAECATAnBgNV\n"
"HR8EIDAeMBygGqAYhhZodHRwOi8veDEuYy5sZW5jci5vcmcvMA0GCSqGSIb3DQEB\n"
"CwUAA4ICAQA8spSI95KKfn2W6GMmDpHBJSPaLbsS3W93cijJCRCYAc1fsJgL1FIL\n"
"7C0C9ecPOdcwB2fi0Dk2p94j9iTJCxmt5CFSKLRWwnXT2MMSXexVxqoVB79BdWPx\n"
"VXETkVme/qYSAuKVHh5Ps+5BixgmwS1JkjSAc+MfrUbNssVEEnH0aEiAh+rotXAV\n"
"JSP/Ye7LJPEwD9DWG72vVWbhAcuOf5OLjz57Ctk7MgQHynZ7+PlHJtajroCaIbtC\n"
"r6tcZZaAwUQm+jQyeWdV+2hv9deOYFmKeQyjjcSrN5Nadrw+L9DZJLbA1HqeNvLh\n"
"BgqpP0fvJq2N6EtD574N6eMI7uMsJTnji2UDz9el5XLSv9fqJMuDQtYVb2oTNoKp\n"
"oUqhxPVC0aq4eG5MESaIdn8b5ZGSSeAJLMHXljEdlNza+ncfkviXk1POLnnFdvx8\n"
"/gk6M374WbLWFXw8N141B/Rl/tINGfl1TxOIiqtiMYkL02RSGb1kq34BL9NPP27z\n"
"RGMuHGnzS3hFIrRTfKxrzUZ9RzQWzEG3K6fJ3r2nqSltkeytis9DIBoFY9VmVyjL\n"
"M71DMi+y1+TRSJVClEMwvA4yL++7q9XZx5r5wBRWB4kQTKH5qyoZnDw7iiuh1lID\n"
"yDFx8r7i9vIJU5HS3moZLkYWAOilMaV9N56A9Bgb6dNcHkvg3NoaYA==\n"
"-----END CERTIFICATE-----\n";

const char* RENDER_ROOT_CA =
// GTS Root R4 (Google Trust Services Root 4 - Render Host)
"-----BEGIN CERTIFICATE-----\n"
"MIICCTCCAY6gAwIBAgINAgPlwGjvYxqccpBQUjAKBggqhkjOPQQDAzBHMQswCQYD\n"
"VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIG\n"
"A1UEAxMLR1RTIFJvb3QgUjQwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAw\n"
"WjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2Vz\n"
"IExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjQwdjAQBgcqhkjOPQIBBgUrgQQAIgNi\n"
"AATzdHOnaItgrkO4NcWBMHtLSZ37wWHO5t5GvWvVYRg1rkDdc/eJkTBa6zzuhXyi\n"
"QHY7qca4R9gq55KRanPpsXI5nymfopjTX15YhmUPoYRlBtHci8nHc8iMai/lxKvR\n"
"HYqjQjBAMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQW\n"
"BBSATNbrdP9JNqPV2Py1PsVq8JQdjDAKBggqhkjOPQQDAwNpADBmAjEA6ED/g94D\n"
"9J+uHXqnLrmvT/aDHQ4thQEd0dlq7A/Cr8deVl5c1RxYIigL9zC2L7F8AjEA8GE8\n"
"p/SgguMh1YQdc4acLa/KNJvxn7kjNuK8YAOdgLOaVsjh4rsUecrNIdSUtUlD\n"
"-----END CERTIFICATE-----\n"
// GTS Root R1 (Google Trust Services Root 1)
"-----BEGIN CERTIFICATE-----\n"
"MIIFVzCCAz+gAwIBAgINAgPlk28xsBNJiGuiFzANBgkqhkiG9w0BAQwFADBHMQsw\n"
"CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU\n"
"MBIGA1UEAxMLR1RTIFJvb3QgUjEwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAw\n"
"MDAwWjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZp\n"
"Y2VzIExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjEwggIiMA0GCSqGSIb3DQEBAQUA\n"
"A4ICDwAwggIKAoICAQC2EQKLHuOhd5s73L+UPreVp0A8of2C+X0yBoJx9vaMf/vo\n"
"27xqLpeXo4xL+Sv2sfnOhB2x+cWX3u+58qPpvBKJXqeqUqv4IyfLpLGcY9vXmX7w\n"
"Cl7raKb0xlpHDU0QM+NOsROjyBhsS+z8CZDfnWQpJSMHobTSPS5g4M/SCYe7zUjw\n"
"TcLCeoiKu7rPWRnWr4+wB7CeMfGCwcDfLqZtbBkOtdh+JhpFAz2weaSUKK0Pfybl\n"
"qAj+lug8aJRT7oM6iCsVlgmy4HqMLnXWnOunVmSPlk9orj2XwoSPwLxAwAtcvfaH\n"
"szVsrBhQf4TgTM2S0yDpM7xSma8ytSmzJSq0SPly4cpk9+aCEI3oncKKiPo4Zor8\n"
"Y/kB+Xj9e1x3+naH+uzfsQ55lVe0vSbv1gHR6xYKu44LtcXFilWr06zqkUspzBmk\n"
"MiVOKvFlRNACzqrOSbTqn3yDsEB750Orp2yjj32JgfpMpf/VjsPOS+C12LOORc92\n"
"wO1AK/1TD7Cn1TsNsYqiA94xrcx36m97PtbfkSIS5r762DL8EGMUUXLeXdYWk70p\n"
"aDPvOmbsB4om3xPXV2V4J95eSRQAogB/mqghtqmxlbCluQ0WEdrHbEg8QOB+DVrN\n"
"VjzRlwW5y0vtOUucxD/SVRNuJLDWcfr0wbrM7Rv1/oFB2ACYPTrIrnqYNxgFlQID\n"
"AQABo0IwQDAOBgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4E\n"
"FgQU5K8rJnEaK0gnhS9SZizv8IkTcT4wDQYJKoZIhvcNAQEMBQADggIBAJ+qQibb\n"
"C5u+/x6Wki4+omVKapi6Ist9wTrYggoGxval3sBOh2Z5ofmmWJyq+bXmYOfg6LEe\n"
"QkEzCzc9zolwFcq1JKjPa7XSQCGYzyI0zzvFIoTgxQ6KfF2I5DUkzps+GlQebtuy\n"
"h6f88/qBVRRiClmpIgUxPoLW7ttXNLwzldMXG+gnoot7TiYaelpkttGsN/H9oPM4\n"
"7HLwEXWdyzRSjeZ2axfG34arJ45JK3VmgRAhpuo+9K4l/3wV3s6MJT/KYnAK9y8J\n"
"ZgfIPxz88NtFMN9iiMG1D53Dn0reWVlHxYciNuaCp+0KueIHoI17eko8cdLiA6Ef\n"
"MgfdG+RCzgwARWGAtQsgWSl4vflVy2PFPEz0tv/bal8xa5meLMFrUKTX5hgUvYU/\n"
"Z6tGn6D/Qqc6f1zLXbBwHSs09dR2CQzreExZBfMzQsNhFRAbd03OIozUhfJFfbdT\n"
"6u9AWpQKXCBfTkBdYiJ23//OYb2MI3jSNwLgjt7RETeJ9r/tSQdirpLsQBqvFAnZ\n"
"0E6yove+7u7Y/9waLd64NnHi/Hm3lCXRSHNboTXns5lndcEZOitHTtNCjv0xyBZm\n"
"2tIMPNuzjsmhDYAPexZ3FL//2wmUspO8IFgV6dtxQ/PeEMMA3KgqlbbC1j+Qa3bb\n"
"bP6MvPJwNQzcmRk13NfIRmPVNnGuV/u3gm3c\n"
"-----END CERTIFICATE-----\n"
// ISRG Root X1 (Let's Encrypt Root)
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMHSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
"-----END CERTIFICATE-----\n";

bool tjpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  tjpgCallbackCount++;
  if (y >= SCREEN_H) return 0;
  tft.drawRGBBitmap(x, y, bitmap, w, h);

  // Capture pristine wallpaper pixels for the clock/date region
  if (s_clockBg != nullptr) {
    int16_t x_start = max((int16_t)CLOCK_BG_X, x);
    int16_t x_end = min((int16_t)(CLOCK_BG_X + CLOCK_BG_W), (int16_t)(x + w));
    int16_t y_start = max((int16_t)CLOCK_BG_Y, y);
    int16_t y_end = min((int16_t)(CLOCK_BG_Y + CLOCK_BG_H), (int16_t)(y + h));

    if (x_start < x_end && y_start < y_end) {
      for (int16_t py = y_start; py < y_end; py++) {
        for (int16_t px = x_start; px < x_end; px++) {
          uint16_t pixel = bitmap[(py - y) * w + (px - x)];
          s_clockBg[(py - CLOCK_BG_Y) * CLOCK_BG_W + (px - CLOCK_BG_X)] = pixel;
        }
      }
    }
  }

  return 1;
}

#include "SnakeGame.h"
#include "TetrisGame.h"
#include "SimonGame.h"
#include "ChessApp.h"
#include "RemindersApp.h"

SnakeGame snake(tft);
TetrisGame tetris(tft);
SimonGame simon(tft);
ChessApp chess(tft);
RemindersApp reminders(tft);

// ======================================
// BUTTONS
// ======================================

#define BTN_LEFT   33
#define BTN_RIGHT  25
#define BTN_SELECT 14

Button leftButton   = {BTN_LEFT, HIGH, HIGH, 0};
Button rightButton  = {BTN_RIGHT, HIGH, HIGH, 0};
Button selectButton = {BTN_SELECT, HIGH, HIGH, 0};

const unsigned long DEBOUNCE_MS = 25;

bool leftPressed = false;
bool rightPressed = false;
bool selectPressed = false;

// ======================================
// COLOURS
// ======================================

#define BG       0x0000
#define WHITE    0xFFFF
#define CYAN     0x07FF
#define BLUE     0x001F
#define PURPLE   0x801F
#define GREY     0x8410
#define YELLOW   0xFFE0
#define DARKBLUE 0x000D

// ======================================
// APPLICATIONS
// ======================================

const char* appNames[] = {
  "MAGIC 8 BALL",
  "CHESS",
  "SNAKE",
  "TETRIS",
  "SIMON SAYS",
  "REMINDERS"
};

const int APP_COUNT = 6;

int appIndex = 0;

enum ScreenState {
  HOME,
  LAUNCHER,
  APP
};

ScreenState screen = HOME;

// ======================================
// MAGIC 8 BALL
// ======================================

const char* answers[] = {
  "It is certain.",
  "Without a doubt.",
  "You may rely on it.",
  "Absolutely!",
  "Signs point to yes.",
  "Most likely.",
  "Outlook good.",
  "Yes, definitely.",
  "As I see it, yes.",
  "Better not tell you.",
  "Ask again later.",
  "Cannot predict now.",
  "Concentrate and ask.",
  "Reply hazy, try again.",
  "Don't count on it.",
  "My reply is no.",
  "Very doubtful.",
  "Outlook not good.",
  "My sources say no.",
  "Absolutely not."
};

const int ANSWER_COUNT = 20;

// ======================================
// TIMERS
// ======================================

unsigned long lastInteraction = 0;
unsigned long lastClockUpdate = 0;
unsigned long lastWiFiAttempt = 0;

const unsigned long IDLE_TIMEOUT = 10000;
const unsigned long WIFI_RETRY = 10000;

// ======================================
// OFF-SCREEN CAROUSEL CANVAS
// ======================================

// 180 x 90 x 2 bytes = 32,400 bytes.

const int CANVAS_W = 200;
const int CANVAS_H = 130;

GFXcanvas16* carouselCanvas = nullptr;

bool canvasAvailable = false;

const int CANVAS_X = 20;
const int CANVAS_Y = 32;
// ======================================
// CAROUSEL ANIMATION
// ======================================

bool animating = false;

int animationDirection = 0;

unsigned long animationStart = 0;
unsigned long lastAnimationFrame = 0;

const unsigned long ANIMATION_DURATION = 320;
const unsigned long FRAME_INTERVAL = 25;

const float CAROUSEL_SPACING = 77.0f;
// ======================================
// BUTTON HANDLING
// ======================================

bool updateButton(Button& button) {

  bool reading = digitalRead(button.pin);

  if (reading != button.lastReading) {
    button.lastChange = millis();
    button.lastReading = reading;
  }

  if (
    millis() - button.lastChange >= DEBOUNCE_MS &&
    reading != button.stableState
  ) {

    button.stableState = reading;

    if (button.stableState == LOW) {
      return true;
    }
  }

  return false;
}

const unsigned long REPEAT_INITIAL_DELAY = 400; // 400 ms initial hold before repeat
const unsigned long REPEAT_INTERVAL      = 120; // 120 ms repeat interval

unsigned long leftHoldStart = 0;
unsigned long leftNextRepeat = 0;
unsigned long rightHoldStart = 0;
unsigned long rightNextRepeat = 0;

bool leftRepeated = false;
bool rightRepeated = false;

bool bothHeld() {
  return digitalRead(BTN_LEFT) == LOW &&
         digitalRead(BTN_RIGHT) == LOW;
}

void updateButtons() {
  leftPressed = updateButton(leftButton);
  rightPressed = updateButton(rightButton);
  selectPressed = updateButton(selectButton);

  leftRepeated = false;
  rightRepeated = false;

  // Auto-repeat for Left / Right navigation (disabled if both held)
  if (!bothHeld()) {
    if (digitalRead(BTN_LEFT) == LOW) {
      if (leftHoldStart == 0) {
        leftHoldStart = millis();
        leftNextRepeat = leftHoldStart + REPEAT_INITIAL_DELAY;
      } else if (millis() >= leftNextRepeat) {
        leftRepeated = true;
        leftNextRepeat = millis() + REPEAT_INTERVAL;
      }
    } else {
      leftHoldStart = 0;
    }

    if (digitalRead(BTN_RIGHT) == LOW) {
      if (rightHoldStart == 0) {
        rightHoldStart = millis();
        rightNextRepeat = rightHoldStart + REPEAT_INITIAL_DELAY;
      } else if (millis() >= rightNextRepeat) {
        rightRepeated = true;
        rightNextRepeat = millis() + REPEAT_INTERVAL;
      }
    } else {
      rightHoldStart = 0;
    }
  } else {
    leftHoldStart = 0;
    rightHoldStart = 0;
  }
}

void waitForButtonRelease() {
  while (digitalRead(BTN_LEFT) == LOW || digitalRead(BTN_RIGHT) == LOW || digitalRead(BTN_SELECT) == LOW) {
    delay(15);
  }
  delay(30);
  leftButton.stableState = HIGH;
  leftButton.lastReading = HIGH;
  rightButton.stableState = HIGH;
  rightButton.lastReading = HIGH;
  selectButton.stableState = HIGH;
  selectButton.lastReading = HIGH;
  leftPressed = false;
  rightPressed = false;
  selectPressed = false;
  leftRepeated = false;
  rightRepeated = false;
  leftHoldStart = 0;
  rightHoldStart = 0;
}

// ======================================
// TEXT HELPERS
// ======================================

void centerText(
  const char* text,
  int y,
  const GFXfont* font,
  uint16_t colour
) {

  tft.setFont(font);
  tft.setTextColor(colour);

  int16_t x1, y1;
  uint16_t w, h;

  tft.getTextBounds(
    text,
    0,
    y,
    &x1,
    &y1,
    &w,
    &h
  );

  tft.setCursor((SCREEN_W - w) / 2, y);
  tft.print(text);
}

// ======================================
// WALLPAPER & CLOCK
// ======================================

bool checkAndUpdateWallpaper(const char* url, const char* path) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  time_t now = time(nullptr);
  if (now < 1700000000) {
    Serial.println("[WALLPAPER] NTP time not yet synchronized. Deferring HTTPS wallpaper check.");
    return false;
  }

  Serial.println("[WALLPAPER] Checking cloud wallpaper for updates...");

  WiFiClientSecure client;
  client.setCACert(GITHUB_ROOT_CA);
  client.setTimeout(10);

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);
  http.setConnectTimeout(6000);
  http.setUserAgent("COSMO-ESP32");
  http.addHeader("Accept", "image/jpeg, */*");
  http.setReuse(false);

  if (!http.begin(client, url)) {
    Serial.println("[WALLPAPER] ERROR: HTTPClient.begin failed during update check.");
    client.stop();
    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[WALLPAPER] Update check: server returned HTTP %d (%s).\n", httpCode, http.errorToString(httpCode).c_str());
    http.end();
    client.stop();
    return false;
  }

  int remoteSize = http.getSize();
  String etag = http.header("ETag");

  // Check if we already have this wallpaper cached
  size_t currentSize = 0;
  if (LittleFS.exists(path)) {
    File curF = LittleFS.open(path, FILE_READ);
    if (curF) {
      currentSize = curF.size();
      curF.close();
    }
  }

  // Check cached etag
  String cachedEtag = "";
  if (LittleFS.exists("/wallpaper.etag")) {
    File etagF = LittleFS.open("/wallpaper.etag", FILE_READ);
    if (etagF) {
      cachedEtag = etagF.readString();
      cachedEtag.trim();
      etagF.close();
    }
  }

  // If local file exists, is valid (> 1000 bytes), and matches remote size and ETag, skip download
  if (currentSize > 1000 && remoteSize > 0 && (size_t)remoteSize == currentSize && (etag.length() == 0 || etag == cachedEtag)) {
    Serial.printf("[WALLPAPER] Cache is up to date (%u bytes). No download needed.\n", (unsigned int)currentSize);
    http.end();
    client.stop();
    return false;
  }

  Serial.println("[WALLPAPER] ========================================");
  Serial.printf("[WALLPAPER] Wallpaper update available (remote: %d bytes vs local: %u bytes). Downloading...\n",
                remoteSize, (unsigned int)currentSize);

  // Staging file in LittleFS to protect existing cache
  const char* tempPath = "/wallpaper.tmp";
  File file = LittleFS.open(tempPath, FILE_WRITE);
  if (!file) {
    Serial.println("[WALLPAPER] ERROR: Failed to create staging file /wallpaper.tmp in LittleFS!");
    http.end();
    client.stop();
    return false;
  }

  int bytesWritten = http.writeToStream(&file);
  file.flush();
  file.close();
  http.end();
  client.stop();

  Serial.printf("[WALLPAPER] Actual downloaded byte count: %d bytes\n", bytesWritten);

  if (bytesWritten < 1000) {
    Serial.printf("[WALLPAPER] ERROR: Download incomplete (%d bytes). Preserving existing cache.\n", bytesWritten);
    LittleFS.remove(tempPath);
    return false;
  }

  // Verify staging file
  File verifyFile = LittleFS.open(tempPath, FILE_READ);
  if (!verifyFile) {
    Serial.println("[WALLPAPER] ERROR: Could not open staging file for verification!");
    return false;
  }
  size_t storedSize = verifyFile.size();
  uint8_t header[2] = {0, 0};
  verifyFile.read(header, 2);
  verifyFile.close();

  Serial.printf("[WALLPAPER] File write result: Stored size = %u bytes\n", (unsigned int)storedSize);

  if (header[0] != 0xFF || header[1] != 0xD8) {
    Serial.printf("[WALLPAPER] JPEG validation result: FAILED (magic bytes 0x%02X 0x%02X, expected 0xFF 0xD8). Preserving cache.\n", header[0], header[1]);
    LittleFS.remove(tempPath);
    return false;
  }

  uint16_t jw = 0, jh = 0;
  JRESULT jres = TJpgDec.getFsJpgSize(&jw, &jh, tempPath, LittleFS);
  Serial.printf("[WALLPAPER] JPEG validation result: %s (code=%d, dimensions=%ux%u)\n", 
                (jres == JDR_OK ? "VALID JPEG" : "CORRUPT JPEG"), (int)jres, jw, jh);

  if (jres != JDR_OK) {
    Serial.printf("[WALLPAPER] ERROR: TJpg_Decoder header parse failed with code %d. Preserving cache.\n", (int)jres);
    LittleFS.remove(tempPath);
    return false;
  }

  // Atomically promote to final cache path
  if (LittleFS.exists(path)) {
    LittleFS.remove(path);
  }
  if (!LittleFS.rename(tempPath, path)) {
    Serial.printf("[WALLPAPER] ERROR: Failed to rename %s to %s!\n", tempPath, path);
    return false;
  }

  // Save new ETag if provided
  if (etag.length() > 0) {
    File etagF = LittleFS.open("/wallpaper.etag", FILE_WRITE);
    if (etagF) {
      etagF.print(etag);
      etagF.close();
    }
  }

  Serial.printf("[WALLPAPER] Final file size: %s is %u bytes. Successfully cached!\n", path, (unsigned int)storedSize);
  Serial.println("[WALLPAPER] ========================================");
  wallpaperLoaded = true;
  return true;
}

bool downloadWallpaper(const char* url, const char* path) {
  return checkAndUpdateWallpaper(url, path);
}

bool renderWallpaper() {
  tjpgCallbackCount = 0;
  Serial.println("[WALLPAPER] Starting render from LittleFS...");

  if (!LittleFS.exists(WALLPAPER_CACHE_PATH)) {
    Serial.printf("[WALLPAPER] Render FAILED: File %s not found in LittleFS.\n", WALLPAPER_CACHE_PATH);
    return false;
  }

  File f = LittleFS.open(WALLPAPER_CACHE_PATH, FILE_READ);
  if (!f) {
    Serial.printf("[WALLPAPER] Render FAILED: Cannot open %s for reading.\n", WALLPAPER_CACHE_PATH);
    return false;
  }
  size_t fSize = f.size();
  f.close();

  Serial.printf("[WALLPAPER] Cached file size: %u bytes\n", (unsigned int)fSize);
  if (fSize < 1000) {
    Serial.printf("[WALLPAPER] Render FAILED: File size %u too small. Purging corrupted file.\n", (unsigned int)fSize);
    LittleFS.remove(WALLPAPER_CACHE_PATH);
    return false;
  }

  uint16_t w = 0, h = 0;
  JRESULT szRes = TJpgDec.getFsJpgSize(&w, &h, WALLPAPER_CACHE_PATH, LittleFS);
  if (szRes != JDR_OK) {
    Serial.printf("[WALLPAPER] Render FAILED: getFsJpgSize error %d. Purging corrupted file.\n", (int)szRes);
    LittleFS.remove(WALLPAPER_CACHE_PATH);
    return false;
  }

  Serial.printf("[WALLPAPER] Decoded dimensions: %ux%u. Decoding JPEG blocks directly to GC9A01...\n", w, h);

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(tjpgOutput);
  JRESULT decRes = TJpgDec.drawFsJpg(0, 0, WALLPAPER_CACHE_PATH, LittleFS);
  Serial.printf("[WALLPAPER] JPEG decoder result: %d (%s)\n", (int)decRes, (decRes == JDR_OK ? "JDR_OK" : "DECODE_ERROR"));
  Serial.printf("[WALLPAPER] Number of drawing callback invocations: %u\n", tjpgCallbackCount);

  if (decRes == JDR_OK && tjpgCallbackCount > 0) {
    Serial.println("[WALLPAPER] SUCCESS: Wallpaper rendered completely on display.");
    s_clockBgValid = true;
    return true;
  }

  Serial.printf("[WALLPAPER] Render FAILED: decRes=%d, callbacks=%u. Falling back.\n", (int)decRes, tjpgCallbackCount);
  s_clockBgValid = false;
  return false;
}

void drawClock(bool force = false) {
  struct tm timeInfo;

  if (getLocalTime(&timeInfo, 10)) {
    char timeString[8];
    strftime(timeString, sizeof(timeString), "%H:%M", &timeInfo);

    char dateString[20];
    strftime(dateString, sizeof(dateString), "%a, %b %d", &timeInfo);
    for (int i = 0; dateString[i]; i++) {
      dateString[i] = toupper((unsigned char)dateString[i]);
    }

    // Skip redrawing if text has not changed and not forced
    if (!force && strcmp(timeString, s_lastTimeString) == 0 && strcmp(dateString, s_lastDateString) == 0) {
      lastClockUpdate = millis();
      return;
    }

    // Erase previous text and restore background before redrawing
    if (wallpaperLoaded && s_clockBg != nullptr && s_clockBgValid) {
      tft.drawRGBBitmap(CLOCK_BG_X, CLOCK_BG_Y, s_clockBg, CLOCK_BG_W, CLOCK_BG_H);
    } else {
      tft.fillRect(CLOCK_BG_X, CLOCK_BG_Y, CLOCK_BG_W, CLOCK_BG_H, BG);
    }

    centerText(timeString, 146, &FreeSansBold9pt7b, WHITE);
    centerText(dateString, 170, &FreeSans9pt7b, WHITE);

    strncpy(s_lastTimeString, timeString, sizeof(s_lastTimeString) - 1);
    s_lastTimeString[sizeof(s_lastTimeString) - 1] = '\0';
    strncpy(s_lastDateString, dateString, sizeof(s_lastDateString) - 1);
    s_lastDateString[sizeof(s_lastDateString) - 1] = '\0';
  }

  lastClockUpdate = millis();
}

void drawHome() {
  Serial.println("[HOME] Drawing home screen...");
  bool rendered = false;

  // 1. Try rendering from existing LittleFS cache
  if (LittleFS.exists(WALLPAPER_CACHE_PATH)) {
    rendered = renderWallpaper();
    if (!rendered) {
      Serial.println("[HOME] Existing wallpaper cache failed to render. Will attempt fresh download if Wi-Fi available.");
    }
  }

  // 2. If not rendered and Wi-Fi is connected, download and render
  if (!rendered && WiFi.status() == WL_CONNECTED) {
    Serial.println("[HOME] No valid wallpaper cached. Downloading from cloud...");
    if (downloadWallpaper(COSMO_WALLPAPER_URL, WALLPAPER_CACHE_PATH)) {
      rendered = renderWallpaper();
    }
  }

  // 3. Fallback or success handling
  if (!rendered) {
    wallpaperLoaded = false;
    Serial.print("[HOME] Drawing dark fallback background. Exact reason: ");
    if (WiFi.status() != WL_CONNECTED && !LittleFS.exists(WALLPAPER_CACHE_PATH)) {
      Serial.println("Wi-Fi not connected and no local cache exists.");
    } else if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Download or JPEG decoding encountered an error.");
    } else {
      Serial.println("Local cache corrupted and Wi-Fi unavailable.");
    }

    tft.fillScreen(BG);
    tft.drawCircle(120, 120, 114, DARKBLUE);
    tft.drawCircle(120, 120, 100, 0x10A2);
  } else {
    wallpaperLoaded = true;
  }

  drawClock(true);
}

// ======================================
// APP COLOURS
// ======================================

uint16_t getAppColour(int index) {

  switch (index) {

    case 0: return BLUE;     // Magic 8 Ball
    case 1: return 0x2965;   // Chess (Slate Blue)
    case 2: return 0x07E0;   // Snake (Green)
    case 3: return CYAN;     // Tetris (Cyan)
    case 4: return DARKBLUE; // Simon Says (Dark Blue)
    case 5: return YELLOW;   // Reminders (Yellow)

    default: return 0x2965;
  }
}

// ======================================
// COLOR DIMMING (RGB565)
// ======================================

uint16_t dimColor(uint16_t color, float brightness) {
  if (brightness >= 0.99f) return color;
  if (brightness <= 0.01f) return 0;

  uint16_t r = (color >> 11) & 0x1F;
  uint16_t g = (color >> 5) & 0x3F;
  uint16_t b = color & 0x1F;

  r = (uint16_t)(r * brightness);
  g = (uint16_t)(g * brightness);
  b = (uint16_t)(b * brightness);

  if (r > 0x1F) r = 0x1F;
  if (g > 0x3F) g = 0x3F;
  if (b > 0x1F) b = 0x1F;

  return (r << 11) | (g << 5) | b;
}

// ======================================
// APPLICATION ICONS (LITTLEFS & TJPG)
// ======================================

const char* iconFileNames[APP_COUNT] = {
  "8ball_pix.jpg",
  "chess_pix.jpg",
  "snake_pix.jpg",
  "tetris_pix.jpg",
  "simon_pix.jpg",
  "reminders_pix.jpg"
};

const char* iconCachePaths[APP_COUNT] = {
  "/icon_8ball_pix.jpg",
  "/icon_chess_pix.jpg",
  "/icon_snake_pix.jpg",
  "/icon_tetris_pix.jpg",
  "/icon_simon_pix.jpg",
  "/icon_reminders_pix.jpg"
};

static uint16_t* s_iconRawBuf = nullptr;
static int s_cachedRawIndex = -1;
static uint32_t s_iconCallbackCount = 0;

void auditIconCache() {
  Serial.println("[STORAGE] ================= ICON CACHE AUDIT =================");

  // Clean up obsolete legacy cache files if present to reclaim LittleFS space
  const char* legacyPaths[APP_COUNT] = {
    "/icon_8ball.jpg",
    "/icon_chess.jpg",
    "/icon_snake.jpg",
    "/icon_tetris.jpg",
    "/icon_simon.jpg",
    "/icon_reminders.jpg"
  };
  for (int i = 0; i < APP_COUNT; i++) {
    if (LittleFS.exists(legacyPaths[i])) {
      Serial.printf("[STORAGE] Removing obsolete legacy icon cache: %s\n", legacyPaths[i]);
      LittleFS.remove(legacyPaths[i]);
    }
  }

  for (int i = 0; i < APP_COUNT; i++) {
    const char* path = iconCachePaths[i];
    if (!LittleFS.exists(path)) {
      Serial.printf("[STORAGE] Icon [%d] %s: MISSING from LittleFS\n", i, path);
    } else {
      File f = LittleFS.open(path, FILE_READ);
      if (!f) {
        Serial.printf("[STORAGE] Icon [%d] %s: FAILED TO OPEN\n", i, path);
      } else {
        size_t sz = f.size();
        uint8_t hdr[2] = {0, 0};
        f.read(hdr, 2);
        f.close();
        bool validJpg = (hdr[0] == 0xFF && hdr[1] == 0xD8);
        Serial.printf("[STORAGE] Icon [%d] %s: %u bytes, header=0x%02X 0x%02X (%s)\n",
                      i, path, (unsigned int)sz, hdr[0], hdr[1],
                      validJpg ? "VALID JPEG" : "INVALID HEADER");
      }
    }
  }
  Serial.println("[STORAGE] =====================================================");
}

bool tjpgIconDecodeCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  s_iconCallbackCount++;
  if (!s_iconRawBuf) return 0;
  for (int j = 0; j < h; j++) {
    int py = y + j;
    if (py < 0 || py >= 80) continue;
    int row = py * 80;
    for (int i = 0; i < w; i++) {
      int px = x + i;
      if (px < 0 || px >= 80) continue;
      s_iconRawBuf[row + px] = bitmap[j * w + i];
    }
  }
  return 1;
}

// ======================================
// NETWORK DIAGNOSTICS & PROBE
// ======================================

bool runNetworkDiagnostics(const char* host, IPAddress& outIP) {
  Serial.println("[NET-DIAG] ========================================");
  Serial.println("[NET-DIAG] NETWORK & HTTPS DIAGNOSTIC PROBE");
  Serial.println("[NET-DIAG] ========================================");

  // 1. Wi-Fi Status
  wl_status_t wfStatus = WiFi.status();
  Serial.printf("[NET-DIAG] 1. Wi-Fi Status: %d (%s)\n",
                wfStatus, (wfStatus == WL_CONNECTED ? "WL_CONNECTED" : "DISCONNECTED"));
  if (wfStatus != WL_CONNECTED) {
    Serial.println("[NET-DIAG] CRITICAL ERROR: Wi-Fi is disconnected! Aborting HTTPS probe.");
    Serial.println("[NET-DIAG] ========================================");
    return false;
  }

  Serial.printf("[NET-DIAG]    - Local IP:    %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("[NET-DIAG]    - Gateway IP:  %s\n", WiFi.gatewayIP().toString().c_str());
  Serial.printf("[NET-DIAG]    - Subnet Mask: %s\n", WiFi.subnetMask().toString().c_str());
  Serial.printf("[NET-DIAG]    - DNS Server:  %s\n", WiFi.dnsIP().toString().c_str());
  Serial.printf("[NET-DIAG]    - RSSI:        %d dBm\n", WiFi.RSSI());
  Serial.printf("[NET-DIAG]    - Free Heap:   %u bytes (Max contiguous block: %u bytes)\n",
                ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  // 2. System Time & NTP Check
  time_t now = time(nullptr);
  struct tm timeinfo;
  bool timeValid = getLocalTime(&timeinfo, 100);
  char timeBuf[32] = {0};
  if (timeValid) {
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  } else {
    snprintf(timeBuf, sizeof(timeBuf), "UNSYNCHRONIZED (epoch=%ld)", (long)now);
  }
  Serial.printf("[NET-DIAG] 2. System Clock: %s (NTP epoch: %ld, valid: %s)\n",
                timeBuf, (long)now, (now >= 1700000000 ? "YES" : "NO"));

  // 3. DNS Resolution
  Serial.printf("[NET-DIAG] 3. Probing DNS resolution for '%s'...\n", host);
  unsigned long dnsStart = millis();
  int dnsResult = WiFi.hostByName(host, outIP);
  unsigned long dnsDuration = millis() - dnsStart;

  if (dnsResult == 1 && outIP != INADDR_NONE) {
    Serial.printf("[NET-DIAG]    - DNS SUCCESS: %s -> %s (took %lu ms)\n",
                  host, outIP.toString().c_str(), dnsDuration);
  } else {
    Serial.printf("[NET-DIAG]    - DNS FAILED (code=%d, took %lu ms). Attempting fallback to Google DNS (8.8.8.8)...\n",
                  dnsResult, dnsDuration);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, IPAddress(8, 8, 8, 8), IPAddress(1, 1, 1, 1));
    dnsResult = WiFi.hostByName(host, outIP);
    if (dnsResult == 1 && outIP != INADDR_NONE) {
      Serial.printf("[NET-DIAG]    - Fallback DNS SUCCESS: %s -> %s\n", host, outIP.toString().c_str());
    } else {
      Serial.println("[NET-DIAG] CRITICAL ERROR: DNS resolution completely failed! Cannot route to host.");
      Serial.println("[NET-DIAG] ========================================");
      return false;
    }
  }

  // 4. Raw TCP Socket Probe to Port 443
  Serial.printf("[NET-DIAG] 4. Probing raw TCP handshake to %s:443...\n", outIP.toString().c_str());
  WiFiClient tcpProbe;
  tcpProbe.setTimeout(4);
  unsigned long tcpStart = millis();
  bool tcpOk = tcpProbe.connect(outIP, 443);
  unsigned long tcpDuration = millis() - tcpStart;

  if (tcpOk) {
    Serial.printf("[NET-DIAG]    - Raw TCP SUCCESS: Connected to port 443 in %lu ms.\n", tcpDuration);
    tcpProbe.stop();
  } else {
    Serial.printf("[NET-DIAG]    - Raw TCP FAILED: Cannot connect to %s:443 (took %lu ms).\n",
                  outIP.toString().c_str(), tcpDuration);
    Serial.println("[NET-DIAG] CRITICAL ERROR: TCP port 443 connection failed or timed out! Router/ISP blocking HTTPS port.");
    Serial.println("[NET-DIAG] ========================================");
    return false;
  }

  // 5. TLS Handshake Probe (WiFiClientSecure)
  Serial.println("[NET-DIAG] 5. Probing TLS handshake with Root CA...");
  if (now < 1700000000) {
    Serial.println("[NET-DIAG]    - NTP time not yet synchronized. TLS verification deferred.");
  } else {
    WiFiClientSecure secProbe;
    secProbe.setTimeout(8);
    secProbe.setCACert(GITHUB_ROOT_CA);

    unsigned long tlsStart = millis();
    bool tlsOk = secProbe.connect(host, 443);
    unsigned long tlsDuration = millis() - tlsStart;

    if (tlsOk) {
      Serial.printf("[NET-DIAG]    - TLS Handshake SUCCESS with Root CA in %lu ms.\n", tlsDuration);
      secProbe.stop();
    } else {
      char lastErr[128] = {0};
      secProbe.lastError(lastErr, sizeof(lastErr));
      Serial.printf("[NET-DIAG]    - TLS Handshake FAILED with Root CA in %lu ms. Error: %s\n", tlsDuration, lastErr);
    }
  }

  Serial.println("[NET-DIAG] ========================================");
  return true;
}

bool downloadIcon(int index) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[ICON-DOWNLOAD] App [%d]: Skipped (Wi-Fi not connected)\n", index);
    return false;
  }
  if (index < 0 || index >= APP_COUNT) return false;

  char url[128];
  snprintf(url, sizeof(url), "https://raw.githubusercontent.com/reva-hattekar/COSMO/main/assets/%s", iconFileNames[index]);
  const char* destPath = iconCachePaths[index];

  Serial.println("[ICON-DOWNLOAD] ----------------------------------------");
  Serial.printf("[ICON-DOWNLOAD] App [%d]: %s\n", index, appNames[index]);
  Serial.printf("[ICON-DOWNLOAD] URL: %s\n", url);
  Serial.printf("[ICON-DOWNLOAD] Free Heap before request: %u bytes\n", ESP.getFreeHeap());

  time_t now = time(nullptr);
  if (now < 1700000000) {
    Serial.println("[ICON-DOWNLOAD] NTP time not yet synchronized. Deferring HTTPS icon download.");
    return false;
  }

  WiFiClientSecure client;
  client.setCACert(GITHUB_ROOT_CA);
  client.setTimeout(10);

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(10000);
  http.setConnectTimeout(8000);
  http.setUserAgent("COSMO-ESP32");
  http.addHeader("Accept", "image/jpeg, */*");
  http.setReuse(false);

  if (!http.begin(client, url)) {
    Serial.printf("[ICON-DOWNLOAD] ERROR: HTTPClient.begin failed for %s\n", url);
    return false;
  }

  int httpCode = http.GET();
  Serial.printf("[ICON-DOWNLOAD] HTTP Status: %d (%s)\n", httpCode, http.errorToString(httpCode).c_str());

  if (httpCode < 0) {
    char sslErr[128] = {0};
    client.lastError(sslErr, sizeof(sslErr));
    Serial.printf("[ICON-DOWNLOAD] Connection failure detail: %s\n", (sslErr[0] ? sslErr : "No mbedTLS error reported"));
  }

  if (httpCode != HTTP_CODE_OK) {
    http.end();
    client.stop();
    return false;
  }

  const char* tempPath = "/icon_tmp.jpg";
  if (LittleFS.exists(tempPath)) {
    LittleFS.remove(tempPath);
  }

  File f = LittleFS.open(tempPath, FILE_WRITE);
  if (!f) {
    Serial.printf("[ICON-DOWNLOAD] ERROR: Could not open %s for writing\n", tempPath);
    http.end();
    client.stop();
    return false;
  }

  int written = http.writeToStream(&f);
  f.flush();
  f.close();
  http.end();
  client.stop();

  Serial.printf("[ICON-DOWNLOAD] Downloaded bytes: %d\n", written);

  if (written < 500) {
    Serial.printf("[ICON-DOWNLOAD] ERROR: Download incomplete (%d bytes < 500)\n", written);
    LittleFS.remove(tempPath);
    return false;
  }

  File vf = LittleFS.open(tempPath, FILE_READ);
  if (!vf) {
    Serial.printf("[ICON-DOWNLOAD] ERROR: Could not reopen %s for verification\n", tempPath);
    return false;
  }
  size_t storedSize = vf.size();
  uint8_t hdr[2] = {0, 0};
  vf.read(hdr, 2);
  vf.close();

  Serial.printf("[ICON-DOWNLOAD] LittleFS file size: %u bytes\n", (unsigned int)storedSize);
  bool validHdr = (hdr[0] == 0xFF && hdr[1] == 0xD8);
  Serial.printf("[ICON-DOWNLOAD] JPEG validation: magic=0x%02X 0x%02X (%s)\n",
                hdr[0], hdr[1], validHdr ? "VALID JPEG" : "INVALID HEADER");

  if (!validHdr) {
    Serial.println("[ICON-DOWNLOAD] ERROR: Staged file is not a valid JPEG! Purging staging file.");
    LittleFS.remove(tempPath);
    return false;
  }

  uint16_t jw = 0, jh = 0;
  JRESULT jres = TJpgDec.getFsJpgSize(&jw, &jh, tempPath, LittleFS);
  Serial.printf("[ICON-DOWNLOAD] TJpg_Decoder test: code=%d (%s), dimensions=%ux%u\n",
                (int)jres, (jres == JDR_OK ? "JDR_OK" : "HEADER_ERROR"), jw, jh);

  if (jres != JDR_OK) {
    Serial.printf("[ICON-DOWNLOAD] ERROR: TJpg_Decoder cannot parse JPEG header (code=%d)\n", (int)jres);
    LittleFS.remove(tempPath);
    return false;
  }

  if (LittleFS.exists(destPath)) {
    LittleFS.remove(destPath);
  }
  if (!LittleFS.rename(tempPath, destPath)) {
    Serial.printf("[ICON-DOWNLOAD] ERROR: Failed to rename %s to %s\n", tempPath, destPath);
    return false;
  }
  if (s_cachedRawIndex == index) {
    s_cachedRawIndex = -1;
  }

  Serial.printf("[ICON-DOWNLOAD] Final Status: SUCCESS -> %s is %u bytes (Cached successfully)\n",
                destPath, (unsigned int)storedSize);
  Serial.println("[ICON-DOWNLOAD] ----------------------------------------");
  return true;
}

bool syncAppIcons() {
  if (WiFi.status() != WL_CONNECTED) return false;

  // 1. Run full network diagnostics probe
  IPAddress resolvedIP;
  if (!runNetworkDiagnostics("raw.githubusercontent.com", resolvedIP)) {
    Serial.println("[ICON-SYNC] Network diagnostics failed! Postponing icon downloads.");
    return false;
  }

  // 2. Test one connection first: icon 0
  Serial.printf("[ICON-SYNC] STEP 1: Probing single test connection on %s (App 0)...\n", iconFileNames[0]);
  bool testSuccess = false;

  bool icon0AlreadyValid = false;
  if (LittleFS.exists(iconCachePaths[0])) {
    File f = LittleFS.open(iconCachePaths[0], FILE_READ);
    if (f && f.size() >= 500) {
      uint8_t hdr[2] = {0, 0};
      f.read(hdr, 2);
      f.close();
      if (hdr[0] == 0xFF && hdr[1] == 0xD8) {
        icon0AlreadyValid = true;
      }
    } else if (f) {
      f.close();
    }
  }

  if (icon0AlreadyValid) {
    Serial.printf("[ICON-SYNC] %s is already cached and valid in LittleFS.\n", iconFileNames[0]);
    testSuccess = true;
  } else {
    testSuccess = downloadIcon(0);
    if (!testSuccess) {
      Serial.printf("[ICON-SYNC] SINGLE TEST CONNECTION FAILED on %s!\n", iconFileNames[0]);
      Serial.println("[ICON-SYNC] Halting download sequence to prevent resource exhaustion. Will retry later.");
      return false;
    }
    Serial.printf("[ICON-SYNC] SINGLE TEST CONNECTION SUCCEEDED on %s!\n", iconFileNames[0]);
  }

  // 3. Close connection cleanly, wait briefly, then proceed with remaining five icons sequentially
  Serial.println("[ICON-SYNC] STEP 2: Proceeding with remaining icons (1 to 5) sequentially...");
  delay(200);

  bool anyDownloaded = !icon0AlreadyValid && testSuccess;
  for (int i = 1; i < APP_COUNT; i++) {
    bool needDownload = true;
    if (LittleFS.exists(iconCachePaths[i])) {
      File f = LittleFS.open(iconCachePaths[i], FILE_READ);
      if (f && f.size() >= 500) {
        uint8_t hdr[2] = {0, 0};
        f.read(hdr, 2);
        f.close();
        if (hdr[0] == 0xFF && hdr[1] == 0xD8) {
          needDownload = false;
        }
      } else if (f) {
        f.close();
      }
    }

    if (needDownload) {
      if (downloadIcon(i)) {
        anyDownloaded = true;
      } else {
        Serial.printf("[ICON-SYNC] Warning: App [%d] download failed. Continuing with next icon.\n", i);
      }
      delay(150); // Clean wait between connections
    }
  }

  if (anyDownloaded && screen == LAUNCHER) {
    Serial.println("[ICON-SYNC] Redrawing carousel with newly cached icons...");
    renderCarousel(1.0f);
    presentCarousel();
  }

  return anyDownloaded;
}

// ======================================
// APP ICON DRAWING
// ======================================

void drawAppIcon(
  Adafruit_GFX& g,
  int index,
  int x,
  int y,
  int radius,
  float brightness = 1.0f
) {
  index = (index + APP_COUNT) % APP_COUNT;

  uint16_t baseColour = dimColor(getAppColour(index), brightness);
  uint16_t whiteColour = dimColor(WHITE, brightness);

  g.fillCircle(x, y, radius, baseColour);
  g.drawCircle(x, y, radius, whiteColour);

  const char* cachePath = iconCachePaths[index];
  bool imageRendered = false;
  const char* failReason = "Unknown";
  int cbCount = 0;
  JRESULT decRes = JDR_INP;

  if (!LittleFS.exists(cachePath)) {
    failReason = "File not found in LittleFS";
  } else {
    // If not already in raw buffer for this index, decode once from LittleFS
    if (s_cachedRawIndex != index) {
      s_iconCallbackCount = 0;
      uint16_t jpgW = 0, jpgH = 0;
      JRESULT szRes = TJpgDec.getFsJpgSize(&jpgW, &jpgH, cachePath, LittleFS);
      if (szRes != JDR_OK || jpgW != 80 || jpgH != 80) {
        failReason = (szRes != JDR_OK) ? "Failed to read JPEG dimensions" : "Invalid dimensions (must be 80x80)";
        Serial.printf("[ICON-RENDER] App [%d] %s: dimension check failed (%ux%u, res=%d)\n",
                      index, appNames[index], jpgW, jpgH, (int)szRes);
      } else {
        TJpgDec.setJpgScale(1);
        TJpgDec.setSwapBytes(false);
        TJpgDec.setCallback(tjpgIconDecodeCallback);
        decRes = TJpgDec.drawFsJpg(0, 0, cachePath, LittleFS);
        cbCount = s_iconCallbackCount;
        if (decRes == JDR_OK && cbCount > 0) {
          s_cachedRawIndex = index;
        } else {
          failReason = (decRes != JDR_OK) ? "TJpgDec decode error" : "Zero callbacks";
        }
      }
    }

    if (s_cachedRawIndex == index && s_iconRawBuf != nullptr) {
      uint16_t* canvasBuf = carouselCanvas ? carouselCanvas->getBuffer() : nullptr;
      if (canvasBuf) {
        // Inner clipping boundary
        int targetR = radius - (radius < 35 ? 3 : 5);
        if (targetR < 4) targetR = 4;
        int r2 = targetR * targetR;

        // Zoom artwork: In the 80x80 asset, center is at (40, 40).
        // The core artwork spans ~27.5 pixels radius.
        // By mapping source radius 27.5 to targetR, the artwork fills ~90%
        // of the circle, appearing prominently zoomed-in and beautifully centered.
        const float R_SRC = 27.5f;
        int invScaleFP = (int)((R_SRC / (float)targetR) * 256.0f);

        for (int dy = -targetR; dy <= targetR; dy++) {
          int cy = y + dy;
          if (cy < 0 || cy >= CANVAS_H) continue;
          int canvasRow = cy * CANVAS_W;
          int dy2 = dy * dy;

          int sy = 40 + ((dy * invScaleFP) >> 8);
          if (sy < 0) sy = 0;
          if (sy >= 80) sy = 79;
          int srcRow = sy * 80;

          for (int dx = -targetR; dx <= targetR; dx++) {
            if (dx * dx + dy2 <= r2) {
              int cx = x + dx;
              if (cx < 0 || cx >= CANVAS_W) continue;

              int sx = 40 + ((dx * invScaleFP) >> 8);
              if (sx < 0) sx = 0;
              if (sx >= 80) sx = 79;

              uint16_t pixel = s_iconRawBuf[srcRow + sx];
              if (brightness < 0.99f) {
                pixel = dimColor(pixel, brightness);
              }
              canvasBuf[canvasRow + cx] = pixel;
            }
          }
        }
        imageRendered = true;
      }
    }
  }

  const char initials[] = {'8', 'C', 'S', 'T', 'M', 'R'};

  // Fallback emblem if not yet cached or if decode failed
  if (!imageRendered) {
    static unsigned long s_lastFailLog[APP_COUNT] = {0};
    if (millis() - s_lastFailLog[index] > 6000) {
      s_lastFailLog[index] = millis();
      Serial.printf("[ICON-RENDER] App [%d] %s: FALLBACK ('%c') - Reason: %s (decRes=%d, callbacks=%d)\n",
                    index, appNames[index], initials[index], failReason, (int)decRes, cbCount);
    }

    g.drawCircle(x, y, radius - 4, whiteColour);
    g.setFont(&FreeSansBold12pt7b);
    g.setTextColor(whiteColour);
    g.setCursor(x - 7, y + 8);
    g.print(initials[index]);
  } else {
    static unsigned long s_lastSuccessLog[APP_COUNT] = {0};
    if (millis() - s_lastSuccessLog[index] > 10000) {
      s_lastSuccessLog[index] = millis();
      Serial.printf("[ICON-RENDER] App [%d] %s: SUCCESS - Rendered zoomed artwork to carousel (brightness=%.2f)\n",
                    index, appNames[index], brightness);
    }
  }
}

// ======================================
// EASING
// ======================================

float easeInOutCubic(float t) {

  if (t < 0.5f) {
    return 4.0f * t * t * t;
  }

  float p = -2.0f * t + 2.0f;

  return 1.0f - (p * p * p) / 2.0f;
}

// ======================================
// DRAW CAROUSEL INTO CANVAS
// ======================================

void renderCarousel(float progress) {
  if (!carouselCanvas) return;

  carouselCanvas->fillScreen(BG);

  float offset = 0;

  if (animating) {

    float remaining = 1.0f - progress;

    offset =
      animationDirection *
      remaining *
      CAROUSEL_SPACING;
  }

  for (int slot = -1; slot <= 1; slot++) {

    int index =
      (appIndex + slot + APP_COUNT) % APP_COUNT;

    float x =
      100.0f +
      slot * CAROUSEL_SPACING +
      offset;

    float distance = fabs(x - 100.0f);

    float scale = distance / CAROUSEL_SPACING;

    if (scale > 1.0f) {
      scale = 1.0f;
    }

    // Centre icon: radius 55.
    // Side icons: radius 22.

    int radius =
      55 - (int)(33.0f * scale);

    if (x < -radius || x > CANVAS_W + radius) {
      continue;
    }

    // Center icon has scale = 0 -> brightness = 1.0 (100% full brightness)
    // Side icons have scale = 1 -> brightness = 0.40 (40% brightness, recessed depth)
    float brightness = 1.0f - (0.60f * scale);
    if (brightness < 0.40f) brightness = 0.40f;
    if (brightness > 1.0f) brightness = 1.0f;

    drawAppIcon(
      *carouselCanvas,
      index,
      (int)x,
      65,
      radius,
      brightness
    );
  }
}
// ======================================
// TRANSFER CANVAS TO TFT
// ======================================

void presentCarousel() {

  if (!canvasAvailable || !carouselCanvas) {
    return;
  }

  tft.drawRGBBitmap(
    CANVAS_X,
    CANVAS_Y,
    carouselCanvas->getBuffer(),
    CANVAS_W,
    CANVAS_H
  );
}

// ======================================
// CAROUSEL DETAILS
// ======================================

void drawCarouselDetails() {

  tft.fillRect(43, 164, 154, 26, BG);

  centerText(
  appNames[appIndex],
  181,
  &FreeSans9pt7b,
  WHITE
);
    

  tft.fillRect(82, 190, 76, 17, BG);

  for (int i = 0; i < APP_COUNT; i++) {

    uint16_t colour =
      (i == appIndex) ? CYAN : GREY;

    int radius =
      (i == appIndex) ? 3 : 2;

    tft.fillCircle(
      92 + i * 11,
      198,
      radius,
      colour
    );
  }
}

// ======================================
// DRAW LAUNCHER
// ======================================

void drawLauncher() {

  tft.fillScreen(BG);

  animating = false;

  renderCarousel(1.0f);
  presentCarousel();

  drawCarouselDetails();
}

// ======================================
// START ANIMATION
// ======================================

void moveCarousel(int direction) {

  if (animating) {
    return;
  }

  animationDirection = direction;

  appIndex =
    (appIndex + direction + APP_COUNT) % APP_COUNT;

  animating = true;

  animationStart = millis();
  lastAnimationFrame = 0;

  lastInteraction = millis();

  drawCarouselDetails();

  renderCarousel(0.0f);
  presentCarousel();
}

// ======================================
// UPDATE ANIMATION
// ======================================

void updateCarouselAnimation() {

  if (!animating) {
    return;
  }

  unsigned long now = millis();

  if (
    now - lastAnimationFrame <
    FRAME_INTERVAL
  ) {
    return;
  }

  lastAnimationFrame = now;

  float progress =
    (float)(now - animationStart) /
    (float)ANIMATION_DURATION;

  if (progress >= 1.0f) {

    progress = 1.0f;

    animating = false;

    renderCarousel(1.0f);
    presentCarousel();

    return;
  }

  float eased = easeInOutCubic(progress);

  renderCarousel(eased);
  presentCarousel();
}

// ======================================
// MAGIC 8 BALL
// ======================================

struct MagicAnswer {
  const char* line1;
  const char* line2;
  const char* line3;
};

const MagicAnswer MAGIC_ANSWERS[20] = {
  {"IT IS", "CERTAIN", nullptr},              // 0: It is certain.
  {"WITHOUT A", "DOUBT", nullptr},            // 1: Without a doubt.
  {"YOU MAY", "RELY ON", "IT"},               // 2: You may rely on it.
  {"ABSOLUTELY!", nullptr, nullptr},          // 3: Absolutely!
  {"SIGNS POINT", "TO YES", nullptr},         // 4: Signs point to yes.
  {"MOST", "LIKELY", nullptr},                // 5: Most likely.
  {"OUTLOOK", "GOOD", nullptr},               // 6: Outlook good.
  {"YES,", "DEFINITELY", nullptr},            // 7: Yes, definitely.
  {"AS I SEE IT,", "YES", nullptr},           // 8: As I see it, yes.
  {"BETTER NOT", "TELL YOU", nullptr},        // 9: Better not tell you.
  {"ASK AGAIN", "LATER", nullptr},            // 10: Ask again later.
  {"CANNOT", "PREDICT", "NOW"},               // 11: Cannot predict now.
  {"CONCENTRATE", "AND ASK", nullptr},        // 12: Concentrate and ask.
  {"REPLY HAZY,", "TRY AGAIN", nullptr},      // 13: Reply hazy, try again.
  {"DON'T COUNT", "ON IT", nullptr},          // 14: Don't count on it.
  {"MY REPLY", "IS NO", nullptr},             // 15: My reply is no.
  {"VERY", "DOUBTFUL", nullptr},              // 16: Very doubtful.
  {"OUTLOOK", "NOT GOOD", nullptr},           // 17: Outlook not good.
  {"MY SOURCES", "SAY NO", nullptr},          // 18: My sources say no.
  {"ABSOLUTELY", "NOT", nullptr}              // 19: Absolutely not.
};

void drawMagic8BallBase() {
  tft.fillScreen(BG);

  // 3D Spherical Specular Sheen (top-left glossy reflection of physical billiard ball)
  tft.drawFastHLine(68, 38, 26, 0x2124);
  tft.drawFastHLine(64, 40, 34, 0x3186);
  tft.drawFastHLine(62, 42, 38, 0x4208);
  tft.drawFastHLine(64, 44, 34, 0x3186);
  tft.drawFastHLine(68, 46, 26, 0x2124);

  // Raised Circular Bezel Lip framing central fluid window (center 120, 120)
  tft.drawCircle(120, 120, 80, 0x18E3); // Outer dark bezel ring
  tft.drawCircle(120, 120, 79, 0x2965); // Charcoal bezel edge
  tft.drawCircle(120, 120, 78, 0x39E7); // Highlight metallic ring
  tft.drawCircle(120, 120, 77, 0x10A2); // Bezel inner groove
  tft.drawCircle(120, 120, 76, 0x0841); // Shadow inside bezel

  // Murky Dark Blue Fluid Reservoir
  tft.fillCircle(120, 120, 75, 0x0027); // Deep indigo fluid (RGB ~ 0, 4, 39)
  tft.drawCircle(120, 120, 75, 0x0012); // Deep liquid shadow edge
  tft.drawCircle(120, 120, 74, 0x001B);
}

void drawInvertedTriangle(int offsetY = 0, uint16_t faceColor = 0x09F4) {
  // Iconic Inverted Triangle Die Face pointing DOWN
  // Vertices: Top-Left (64, 70), Top-Right (176, 70), Bottom Apex (120, 168)
  int y0 = 70 + offsetY;
  int yApex = 168 + offsetY;

  // Solid die face
  tft.fillTriangle(64, y0, 176, y0, 120, yApex, faceColor);

  // 3D Bevel Highlights & Shading:
  // Top edge (light from above): bright cyan-blue highlight
  tft.drawLine(64, y0, 176, y0, 0x5DDF);
  tft.drawLine(66, y0 + 1, 174, y0 + 1, 0x247A);

  // Left bevel: medium cyan-blue edge
  tft.drawLine(64, y0, 120, yApex, 0x1CF8);

  // Right bevel: deep navy shadow edge
  tft.drawLine(176, y0, 120, yApex, 0x0188);

  // Subtle ambient liquid glow outline
  tft.drawTriangle(63, y0 - 1, 177, y0 - 1, 120, yApex + 1, 0x0258);
}

void drawAnswerLine(const char* text, int y, int maxAllowedWidth) {
  if (!text || strlen(text) == 0) return;

  tft.setFont(&FreeSans9pt7b);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, 0, y, &x1, &y1, &w, &h);

  if (w <= maxAllowedWidth) {
    tft.setTextColor(WHITE);
    tft.setCursor((SCREEN_W - w) / 2, y);
    tft.print(text);
  } else {
    // Crisp bitmap font guarantees zero clipping for longer phrases
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(WHITE);
    int bw = strlen(text) * 6;
    tft.setCursor((SCREEN_W - bw) / 2, y - 5);
    tft.print(text);
  }
}

void drawMagicScreen() {
  drawMagic8BallBase();
  drawInvertedTriangle();

  // Initial prompt inside the inverted blue triangle
  drawAnswerLine("ASK A", 102, 76);
  drawAnswerLine("QUESTION", 122, 54);

  // Top Title
  centerText("MAGIC 8 BALL", 34, &FreeSansBold9pt7b, CYAN);

  // Bottom action prompt
  centerText("PRESS SELECT", 214, &FreeSans9pt7b, WHITE);
}

// ======================================
// MAGIC 8 BALL THINKING
// ======================================

void thinkingAnimation() {
  // Clear bottom prompt
  tft.fillRect(30, 202, 180, 24, BG);
  centerText("THINKING...", 214, &FreeSans9pt7b, CYAN);

  // Frame 1: Fluid ripples, die starts sinking into murky liquid
  tft.fillCircle(120, 120, 75, 0x0027);
  tft.fillTriangle(66, 76, 174, 76, 120, 172, 0x0150); // Sinking dimmer die
  tft.drawCircle(120, 120, 42, 0x00AB);
  tft.drawCircle(120, 120, 62, 0x0067);
  delay(160);

  // Frame 2: Murky swirl, die obscured, bubbles emerge
  tft.fillCircle(120, 120, 75, 0x001B); // Darker swirling fluid
  tft.drawCircle(120, 120, 26, 0x012F);
  tft.drawCircle(120, 120, 52, 0x00EB);
  tft.fillCircle(106, 96, 3, 0x037F);
  tft.fillCircle(136, 112, 4, 0x037F);
  tft.fillCircle(114, 136, 2, 0x021B);
  delay(170);

  // Frame 3: Bubbles float up, die rises back to surface
  tft.fillCircle(120, 120, 75, 0x0027);
  tft.fillTriangle(65, 73, 175, 73, 120, 170, 0x0492); // Emerging die
  tft.drawCircle(120, 120, 36, 0x016E);
  tft.fillCircle(108, 86, 3, 0x04BF);
  tft.fillCircle(132, 98, 3, 0x04BF);
  delay(160);

  // Frame 4: Die breaches surface, settling into position
  drawMagic8BallBase();
  drawInvertedTriangle();
  delay(80);
}

// ======================================
// MAGIC 8 BALL ANSWER
// ======================================

void showAnswer(int index) {
  if (index < 0 || index >= ANSWER_COUNT) index = 0;

  drawMagic8BallBase();
  drawInvertedTriangle();

  const MagicAnswer& ans = MAGIC_ANSWERS[index];

  if (ans.line2 == nullptr && ans.line3 == nullptr) {
    // 1-line answer
    drawAnswerLine(ans.line1, 108, 70);
  } else if (ans.line3 == nullptr) {
    // 2-line answer
    drawAnswerLine(ans.line1, 100, 78);
    drawAnswerLine(ans.line2, 120, 56);
  } else {
    // 3-line answer
    drawAnswerLine(ans.line1, 94, 84);
    drawAnswerLine(ans.line2, 112, 64);
    drawAnswerLine(ans.line3, 130, 44);
  }

  // Top subtle title
  centerText("MAGIC 8 BALL", 34, &FreeSansBold9pt7b, CYAN);

  // Action hint below viewing window
  centerText("SELECT: ASK AGAIN", 214, &FreeSans9pt7b, CYAN);
}

void showAnswer(const char* answer) {
  int found = 0;
  for (int i = 0; i < ANSWER_COUNT; i++) {
    if (strcmp(answers[i], answer) == 0) {
      found = i;
      break;
    }
  }
  showAnswer(found);
}

// ======================================
// PLACEHOLDER APPS
// ======================================

void drawPlaceholder(
  const char* title,
  const char* subtitle
) {

  tft.fillScreen(BG);

  centerText(
    title,
    98,
    &FreeSansBold12pt7b,
    CYAN
  );

  centerText(
    subtitle,
    130,
    &FreeSans9pt7b,
    WHITE
  );

  centerText(
    "IN DEVELOPMENT",
    160,
    &FreeSans9pt7b,
    PURPLE
  );
}

// ======================================
// OPEN APP
// ======================================

void openApp() {

  screen = APP;
  lastInteraction = millis();

  switch (appIndex) {

    case 0:
      drawMagicScreen();
      break;

    case 1:
      chess.resetExit();
      chess.init();
      break;

    case 2:
      snake.init();
      break;

    case 3:
      tetris.init();
      break;

    case 4:
      simon.resetExit();
      simon.init();
      break;

    case 5:
      reminders.resetExit();
      reminders.init();
      break;
  }
}

// ======================================
// NAVIGATION
// ======================================

void goHome() {

  screen = HOME;
  animating = false;

  lastInteraction = millis();

  drawHome();
}

void goLauncher() {

  screen = LAUNCHER;
  animating = false;

  lastInteraction = millis();

  drawLauncher();
}

// ======================================
// WIFI
// ======================================

void connectWiFi() {

  Serial.println("Starting WiFi...");

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - start < 12000
  ) {

    delay(300);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("WiFi connected!");
    Serial.println(WiFi.localIP());

    configTime(
      GMT_OFFSET,
      DAYLIGHT_OFFSET,
      "pool.ntp.org",
      "time.google.com"
    );

  } else {

    Serial.println("WiFi connection failed.");
  }

  lastWiFiAttempt = millis();
}

void maintainWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if (
    millis() - lastWiFiAttempt >=
    WIFI_RETRY
  ) {

    lastWiFiAttempt = millis();

    Serial.println("Reconnecting WiFi...");

    WiFi.reconnect();
  }
}

// ======================================
// SETUP
// ======================================

void setup() {

  Serial.begin(115200);

  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);

  Serial.println("Starting COSMO...");

#ifdef REMINDERS_BACKEND_URL
  reminders.setBackendUrl(REMINDERS_BACKEND_URL);
#endif

  tft.begin();
  tft.setRotation(2);
  tft.setTextWrap(false);

  tft.fillScreen(BG);

  // Allocate carousel canvas dynamically after FreeRTOS kernel has started
  carouselCanvas = new GFXcanvas16(CANVAS_W, CANVAS_H);
  canvasAvailable = (carouselCanvas != nullptr && carouselCanvas->getBuffer() != nullptr);

  if (!canvasAvailable) {
    Serial.println("ERROR: Carousel canvas allocation failed!");
    while (true) {
      delay(1000);
    }
  }

  Serial.printf("[DISPLAY] Carousel canvas ready (%dx%d). Free Heap: %u bytes\n",
                CANVAS_W, CANVAS_H, ESP.getFreeHeap());

  // Allocate clock background restoration buffer
  if (!s_clockBg) {
    s_clockBg = (uint16_t*)malloc(CLOCK_BG_W * CLOCK_BG_H * sizeof(uint16_t));
    if (s_clockBg) {
      Serial.printf("[DISPLAY] Allocated %u bytes for clock background clearing cache. Free Heap: %u bytes\n",
                    (unsigned int)(CLOCK_BG_W * CLOCK_BG_H * sizeof(uint16_t)), ESP.getFreeHeap());
    } else {
      Serial.println("[DISPLAY] WARNING: Failed to allocate clock background clearing cache. Fallback active.");
    }
  }

  // Allocate launcher icon decoding buffer
  if (!s_iconRawBuf) {
    s_iconRawBuf = (uint16_t*)malloc(80 * 80 * sizeof(uint16_t));
    if (s_iconRawBuf) {
      Serial.printf("[DISPLAY] Allocated %u bytes for icon decode buffer. Free Heap: %u bytes\n",
                    (unsigned int)(80 * 80 * sizeof(uint16_t)), ESP.getFreeHeap());
    } else {
      Serial.println("[DISPLAY] WARNING: Failed to allocate icon decode buffer. Fallback active.");
    }
  }

  if (!LittleFS.begin(true)) {
    Serial.println("[STORAGE] LittleFS mount failed, formatting...");
  } else {
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    Serial.printf("[STORAGE] LittleFS mount SUCCESS. Total: %u KB, Used: %u KB\n",
                  (unsigned int)(totalBytes / 1024), (unsigned int)(usedBytes / 1024));
  }

  // Audit and print current LittleFS icon cache status
  auditIconCache();

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(tjpgOutput);

  randomSeed(esp_random());

  connectWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[NTP] Waiting for clock synchronization...");
    unsigned long ntpStart = millis();
    while (time(nullptr) < 1700000000 && millis() - ntpStart < 4000) {
      delay(100);
    }
    if (time(nullptr) >= 1700000000) {
      Serial.println("[NTP] Clock synchronized successfully.");
    } else {
      Serial.println("[NTP] Clock sync pending; HTTPS requests will be deferred until NTP completes.");
    }

    // Check remote wallpaper for any cloud updates
    checkAndUpdateWallpaper(COSMO_WALLPAPER_URL, WALLPAPER_CACHE_PATH);
    delay(150);
    syncAppIcons();
  }

  lastInteraction = millis();

  drawHome();

  Serial.printf("[SYSTEM] COSMO READY! Post-boot Free Heap: %u bytes, Min Ever Free Heap: %u bytes\n",
                ESP.getFreeHeap(), ESP.getMinFreeHeap());
}

// ======================================
// MAIN LOOP
// ======================================

void loop() {

  maintainWiFi();
  reminders.maintain(WiFi.status() == WL_CONNECTED);

  updateButtons();

  // ==================================
  // RETURN HOME / EXIT APP: BOTH BUTTONS
  // ==================================

  if (screen == LAUNCHER && bothHeld()) {
    waitForButtonRelease();
    goHome();
    return;
  }

  if (screen == APP) {
    if (appIndex == 4) {
      // Simon Says: ONLY exit on Game Over screen if Left + Right is pressed
      if (simon.wantsToExit()) {
        simon.resetExit();
        waitForButtonRelease();
        goLauncher();
        return;
      }
    } else {
      // Chess (1), Magic 8 Ball (0), Snake (2), Tetris (3), Reminders (5)
      if (bothHeld()) {
        if (appIndex == 1) chess.resetExit();
        if (appIndex == 5) reminders.resetExit();
        waitForButtonRelease();
        goLauncher();
        return;
      }
      if (appIndex == 1 && chess.wantsToExit()) {
        chess.resetExit();
        waitForButtonRelease();
        goLauncher();
        return;
      }
    }
  }

  // ==================================
  // BACKGROUND ICON SYNC (IF MISSING)
  // ==================================

  static unsigned long lastIconSyncCheck = 0;
  if (WiFi.status() == WL_CONNECTED && millis() - lastIconSyncCheck >= 10000) {
    lastIconSyncCheck = millis();
    bool missingAny = false;
    for (int i = 0; i < APP_COUNT; i++) {
      if (!LittleFS.exists(iconCachePaths[i])) {
        missingAny = true;
        break;
      }
    }
    if (missingAny) {
      syncAppIcons();
    }
  }

  // ==================================
  // HOME
  // ==================================

  if (screen == HOME) {

    static unsigned long lastWallpaperCheck = 0;
    if (WiFi.status() == WL_CONNECTED && (millis() - lastWallpaperCheck >= 30000 || !wallpaperLoaded)) {
      lastWallpaperCheck = millis();
      if (checkAndUpdateWallpaper(COSMO_WALLPAPER_URL, WALLPAPER_CACHE_PATH)) {
        drawHome();
        return;
      }
      syncAppIcons();
    }

    if (selectPressed) {

      goLauncher();
      return;
    }

    if (
      millis() - lastClockUpdate >= 1000
    ) {

      drawClock();
    }

    return;
  }

  // ==================================
  // LAUNCHER
  // ==================================

  if (screen == LAUNCHER) {

    if (animating) {

      updateCarouselAnimation();
      return;
    }

    if (leftPressed) {

      moveCarousel(-1);
      return;
    }

    if (rightPressed) {

      moveCarousel(1);
      return;
    }

    if (selectPressed) {

      lastInteraction = millis();

      openApp();
      return;
    }
  }

  // ==================================
  // APPS
  // ==================================

  if (screen == APP) {

    // Magic 8 Ball (0)
    if (
      appIndex == 0 &&
      selectPressed
    ) {

      lastInteraction = millis();

      thinkingAnimation();

      int index = random(ANSWER_COUNT);

      showAnswer(index);

      return;
    }

    // Chess (1)
    if (appIndex == 1) {
      bool leftAct = leftPressed || leftRepeated;
      bool rightAct = rightPressed || rightRepeated;
      chess.update(leftAct, rightAct, selectPressed);
      if (chess.wantsToExit()) {
        chess.resetExit();
        waitForButtonRelease();
        goLauncher();
        return;
      }
      if (leftAct || rightAct || selectPressed || chess.isActive()) {
        lastInteraction = millis();
      }
      return;
    }

    // Snake (2)
    if (appIndex == 2) {
      snake.update(leftPressed, rightPressed, selectPressed);
      if (leftPressed || rightPressed || selectPressed || snake.isActive()) {
        lastInteraction = millis();
      }
      return;
    }

    // Tetris (3)
    if (appIndex == 3) {
      tetris.update(leftPressed, rightPressed, selectPressed);
      if (leftPressed || rightPressed || selectPressed || tetris.isActive()) {
        lastInteraction = millis();
      }
      return;
    }

    // Simon Says (4)
    if (appIndex == 4) {
      simon.update(leftPressed, rightPressed, selectPressed);
      if (simon.wantsToExit()) {
        simon.resetExit();
        waitForButtonRelease();
        goLauncher();
        return;
      }
      if (leftPressed || rightPressed || selectPressed || simon.isActive()) {
        lastInteraction = millis();
      }
      return;
    }

    // Reminders (5)
    if (appIndex == 5) {
      reminders.update(leftPressed, rightPressed, selectPressed);
      if (reminders.wantsToExit()) {
        reminders.resetExit();
        waitForButtonRelease();
        goLauncher();
        return;
      }
      if (leftPressed || rightPressed || selectPressed || reminders.isActive()) {
        lastInteraction = millis();
      }
      return;
    }
  }

  // ==================================
  // IDLE TIMEOUT
  // ==================================

  bool inActiveGame = (screen == APP && (
    (appIndex == 1 && chess.isActive()) ||
    (appIndex == 2 && snake.isActive()) ||
    (appIndex == 3 && tetris.isActive()) ||
    (appIndex == 4 && simon.isActive()) ||
    (appIndex == 5 && reminders.isActive())
  ));

  if (
    screen != HOME &&
    !inActiveGame &&
    millis() - lastInteraction >= IDLE_TIMEOUT
  ) {

    goHome();
  }
}