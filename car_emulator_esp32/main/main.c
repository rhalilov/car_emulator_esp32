/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_vfs_dev.h"
#include "esp_timer.h"
#include "esp_sleep.h"

#include "driver/uart.h"
#include "driver/twai.h"
#include "hal/twai_ll.h"
#include "can_drv_esp32.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "can_drv_esp32.h"
#include "cantp_esp32.h"
#include "can-tp.h"
#include "obd.h"
#include "car_emulator.h"

#define TX_GPIO_NUM             21
#define RX_GPIO_NUM             22

#define CAN_TAG             "CAN"

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      "refo-gery"
#define EXAMPLE_ESP_WIFI_PASS      "dosadnik"
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *cantp_result_enum_str[] = {
		FOREACH_CANTP_RESULT(GENERATE_STRING)
};

static const char *TAG = "wifi station";

static int s_retry_num = 0;

static cantp_rxtx_status_t cantp_ctx;

#if 1//STDIN
#include "esp_vfs_dev.h"
#include "driver/uart.h"

static inline int stdin_getch(char *ch, TickType_t ticks_to_wait)
{
	return uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, ch, 1, ticks_to_wait);
}

int stdin_getstr(char *str, uint16_t len)
{
	char c;
	uint16_t idx = 0;
	while (idx < len) {
		if (stdin_getch(&c, 1) <= 0) {
			continue;
		}
		if (c == '\n' || c == '\r') {
			str[idx] = '\0';
			break;
		} else if (c > 0 && c < 127) {
			str[idx++] = c;
			putchar(c);
			fflush(stdout);
		}
	}
	return idx;
}

void stdin_init(void)
{
    uart_driver_install( (uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM,
                256,		//UART RX buffer size
				0,			//UART TX buffer size
				0,			//queue_size UART event queue size/depth
				NULL,		//uart_queue UART event queue handle
				0);
//	esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
}
#endif

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
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

void wifi_init_sta(void)
{
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
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
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

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

void cantp_sndr_result_cb(int result)
{
	printf("CAN-TP Sender: Received result: ");
	if (result == CANTP_RESULT_N_OK) {
		printf("\033[0;32m");
	} else {
		printf("\033[0;31m");
	}
	printf("%s\033[0m\n", cantp_result_enum_str[result]);

//	msync(rcvr_end_sem, sizeof(size_t), MS_SYNC);
//	sem_post(rcvr_end_sem);
}

int cantp_rcvr_rx_ff_cb(uint32_t id, uint8_t idt, uint8_t **data, uint16_t len)
{
	printf("\033[0;35mCAN-SL Receiver: (R2.3)Received First Frame "
			"from ID=0x%06x IDT=%d CAN-TP Message LEN=%d\033[0m \n",
			id, idt, len); fflush(0);
	*data = malloc(len);
	if (*data == NULL) {
		printf("\033[0;36mCAN-SL Receiver: ERROR allocating memory\033[0m\n");
		fflush(0);
	}

	//Here maybe should be checked if the ID of the Sender is correct
	//(we are expecting sender with this ID)
	usleep(10000);
	if (id == 0x000aaa) {
		return 0;
	}
	printf("\033[0;36mCAN-SL Receiver:\033[0;31m Rejected the frame\033[0m\n");
	return -1;
//	printf("\033[0;36mCAN-SL Receiver: Memory allocated: %ld\033[0m\n", (long)(*data));
//	fflush(0);
}

void print_help()
{
	printf("\n\tCommands:\n"
						"go        start simulator\n"
						"bpr=250   sets the baudrate to 250kbps\n"
						"bpr=500   sets the baudrate to 500kbps\n"
						"idt=STD   sets the ID type to Standard (11bit)\n"
						"idt=EXT   sets the ID type to Extended (29bit)\n"
						"help      this menu\n");
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);



//    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
//    wifi_init_sta();
//

	static twai_general_config_t g_config = {
										.mode = TWAI_MODE_NORMAL,
										.tx_io = TX_GPIO_NUM,
										.rx_io = RX_GPIO_NUM,
										.clkout_io = TWAI_IO_UNUSED,
										.bus_off_io = TWAI_IO_UNUSED,
										.tx_queue_len = 0,//5,
										.rx_queue_len = 5,
										.alerts_enabled = TWAI_ALERT_TX_SUCCESS,//TWAI_ALERT_NONE,
										.clkout_divider = 0,
										.intr_flags = ESP_INTR_FLAG_LEVEL1
									};

//	static twai_timing_config_t t_config = {
//	static twai_timing_config_t t_config_500kb = {
//										.brp = 8,
//										.tseg_1 = 15,
//										.tseg_2 = 4,
//										.sjw = 3,
////										.sjw = 2,
//										.triple_sampling = false
//									};

	emulator_cfg_t ecfg;
	ecfg.boadrate = CFG_500KBPS;
	ecfg.id_type = CFG_STANDARD_ID;

	twai_timing_config_t *t_config_p;

	static twai_timing_config_t t_config_500kb = TWAI_TIMING_CONFIG_500KBITS();
	static twai_timing_config_t t_config_250kb = TWAI_TIMING_CONFIG_250KBITS();

	static twai_filter_config_t f_config = {
										.acceptance_code = 0,
										.acceptance_mask = 0xFFFFFFFF,
										.single_filter = false
									};

	static emulator_ctx_t ectx;
	ectx.cfg = &ecfg;

    stdin_init();

    print_help();

    while (1) {
		char line[30];
		printf("\n\nCMD> "); fflush(stdout);
		if (stdin_getstr(line, 30) <= 0) {
			continue;
		}
		if (strstr("go", line) != NULL) {
			printf("\nSTART\n");
			break;
		} else if (strstr("help", line) != NULL) {
			print_help();
		} else if (strstr("bpr=250", line) != NULL) {
			printf("\nBaudrate set to 250kbps\n");
			ecfg.boadrate = CFG_250KBPS;
		} else if (strstr("bpr=500", line) != NULL) {
			printf("\nBaudrate set to 500kbps\n");
			ecfg.boadrate = CFG_500KBPS;
		} else if (strstr("idt=STD", line) != NULL) {
			printf("\nID type is Standard\n");
			ecfg.id_type = CFG_STANDARD_ID;
		} else if (strstr("idt=EXT", line) != NULL) {
			printf("\nID type is Extended\n");
			ecfg.id_type = CFG_EXTENDED_ID;
		} else {
			printf("\nWrong command\n");
		}
	}

    switch (ecfg.boadrate) {
    	case CFG_250KBPS:
    		t_config_p = &t_config_250kb;
    		break;
    	case CFG_500KBPS:
    		t_config_p = &t_config_500kb;
    		break;
    	default:
    		t_config_p = &t_config_500kb;
    }

	t_config_p->triple_sampling = true;

#if ESP32_IDF_CAN_HAL
	ESP_ERROR_CHECK(twai_driver_install(&g_config, t_config_p, &f_config));
#else
	can_drv_esp32_init(&g_config, &t_config, &f_config);
#endif
	ESP_LOGI(CAN_TAG, "Driver installed");

#if ESP32_IDF_CAN_HAL
	ESP_ERROR_CHECK(twai_start());
#else
	can_drv_esp32_start();
#endif
//	can_drv_esp32_regs_print();
	ESP_LOGI(CAN_TAG, "Driver started");

	cantp_params_t sndr_params;
	sndr_params.st_min_us = 127000;
	sndr_params.block_size = 0;
	sndr_params.wft_tim_us = 0;

    const esp_timer_create_args_t sndr_timer_args = {
            .callback = &cantp_sndr_t_cb,
            .arg = (void*) &cantp_ctx,
            .name = "one-shot"
    };
    esp_timer_handle_t sndr_timer;
    ESP_ERROR_CHECK(esp_timer_create(&sndr_timer_args, &sndr_timer));
    ESP_LOGI(TAG, "Created timer (%p)\n", sndr_timer);
    cantp_set_sndr_timer_ptr(sndr_timer, &cantp_ctx);


    ectx.sem = xSemaphoreCreateBinary();
    cantp_ctx.cb_ctx = (void *)&ectx;
    ectx.cantp_ctx = &cantp_ctx;

	if (cantp_rcvr_params_init(&cantp_ctx, &sndr_params, "Sender") < 0) {
//		return EXIT_FAILURE;
	}

//	uint8_t data[8] = { 0 };
//	for (uint8_t i=0; i < 3; i++) {
//		cantp_can_tx(0xaaa, 0, 8, data, 0);
//		vTaskDelay(2000/portTICK_PERIOD_MS);
//	}

	SemaphoreHandle_t sndr_state_sem = xSemaphoreCreateBinary();
	cantp_ctx.sndr.state_sem = (void *)sndr_state_sem;

	xTaskCreate(cantp_sndr_task, "can_task", 2 * 1024, &cantp_ctx, 1, NULL);

	cantp_rx_task((void *)&cantp_ctx);
}
