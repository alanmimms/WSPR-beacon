// WSPR beacon

#include <string>
#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_sntp.h>
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <lwip/err.h>
#include <lwip/sys.h>


#include <JTEncode.h>
#include <si5351.h>
#include "tinyfsm.hpp"

#include "secrets.h"


static const char *TAG = "WSPR-beacon";


#define CONNECT_TIMEOUT (2 * 60 * 1000)


// Mode defines
#define JT9_TONE_SPACING        174          // ~1.74 Hz
#define JT65_TONE_SPACING       269          // ~2.69 Hz
#define JT4_TONE_SPACING        437          // ~4.37 Hz
#define WSPR_TONE_SPACING       146          // ~1.46 Hz
#define FSQ_TONE_SPACING        879          // ~8.79 Hz
#define FT8_TONE_SPACING        625          // ~6.25 Hz

#define JT9_DELAY               576          // Delay value for JT9-1
#define JT65_DELAY              371          // Delay in ms for JT65A
#define JT4_DELAY               229          // Delay value for JT4A
#define WSPR_DELAY              683          // Delay value for WSPR
#define FSQ_2_DELAY             500          // Delay value for 2 baud FSQ
#define FSQ_3_DELAY             333          // Delay value for 3 baud FSQ
#define FSQ_4_5_DELAY           222          // Delay value for 4.5 baud FSQ
#define FSQ_6_DELAY             167          // Delay value for 6 baud FSQ
#define FT8_DELAY               159          // Delay value for FT8

#define JT9_DEFAULT_FREQ        14078700UL
#define JT65_DEFAULT_FREQ       14078300UL
#define JT4_DEFAULT_FREQ        14078500UL
#define WSPR_DEFAULT_FREQ       14097200UL
#define FSQ_DEFAULT_FREQ        7105350UL     // Base freq is 1350 Hz higher than dial freq in USB
#define FT8_DEFAULT_FREQ        14075000UL

#define DEFAULT_MODE            MODE_WSPR

// Hardware defines
#define BUTTON                  12
#define LED_PIN                 13

// Enumerations
enum mode {MODE_JT9, MODE_JT65, MODE_JT4, MODE_WSPR, MODE_FSQ_2, MODE_FSQ_3,
  MODE_FSQ_4_5, MODE_FSQ_6, MODE_FT8};



static unsigned long connectTime = 0;
static unsigned long etime = 0;
static unsigned long dottime = 0;
static int count = 0;

static Si5351 si5351;
static JTEncode jtencode;

// Change these to match your own. They are used to build the strings for the message.
#define MYCALL  "WB7NAB"
#define MYGRID  "CN87"
#define MYDBM   27

static unsigned long freq;
static char message[] = MYCALL " " MYGRID;
static char call[] = MYCALL;
static char loc[] = MYGRID;
static uint8_t dbm = MYDBM;
static uint8_t tx_buffer[255];
static enum mode cur_mode = DEFAULT_MODE;
static uint8_t symbol_count;
static uint16_t tone_delay, tone_spacing;


static void event_handler(void* arg, esp_event_base_t event_base,
			  int32_t event_id, void* event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(TAG,"connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}


static void wifi_init_sta(void) {
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
						      ESP_EVENT_ANY_ID,
						      &event_handler,
						      NULL,
						      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
						      IP_EVENT_STA_GOT_IP,
						      &event_handler,
						      NULL,
						      &instance_got_ip));

  wifi_config_t wifi_config = {
    .sta = {
      .ssid = EXAMPLE_ESP_WIFI_SSID,
      .password = EXAMPLE_ESP_WIFI_PASS,
      /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
       * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
       * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
       * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
       */
      .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
      .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
      .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
    },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
  ESP_ERROR_CHECK(esp_wifi_start() );

  ESP_LOGI(TAG, "wifi_init_sta finished.");

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
   * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
					 WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
					 pdFALSE,
					 pdFALSE,
					 portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
   * happened. */
  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
	     EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
  } else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
	     EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
  } else {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }
}


static void initNVS(void) {
  //Initialize NVS
  esp_err_t ret = nvs_flash_init();

  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }

  ESP_ERROR_CHECK(ret);
}


static void initWIFI(void) {
  
  if (CONFIG_LOG_MAXIMUM_LEVEL > CONFIG_LOG_DEFAULT_LEVEL) {
    esp_log_level_set("wifi", CONFIG_LOG_MAXIMUM_LEVEL);
  }

  wifi_init_sta();
}


void app_main(void) {
  ESP_LOGI(TAG, "[start]");

  ESP_LOGI(TAG, "[init NVS]");
  initNVS();

  ESP_LOGI(TAG, "[init WiFi]");
  initWIFI();

  ESP_LOGI(TAG, "[init SI5351]");
  si5351_Init(0);		// No correction value for now.
}
