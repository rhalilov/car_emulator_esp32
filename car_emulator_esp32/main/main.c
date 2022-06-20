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

uint8_t can_flow_queue[5][8];

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

esp_err_t configure_stdin_stdout(void)
{
    // Initialize VFS & UART so we can use std::cout/cin
    setvbuf(stdin, NULL, _IONBF, 0);
    /* Install UART driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK( uart_driver_install( (uart_port_t)CONFIG_ESP_CONSOLE_UART_NUM,
            256, 0, 0, NULL, 0) );
    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
    esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);
    return ESP_OK;
}

static cantp_rxtx_status_t cantp_ctx;

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
//	configure_stdin_stdout();
//
//	printf("\nSTART:");
//	char input_str[30];
//	fgets(input_str, 30, stdin);
//	printf("%s\n GO!\n", input_str);

	static twai_general_config_t g_config = {
										.mode = TWAI_MODE_NORMAL,
										.tx_io = TX_GPIO_NUM,
										.rx_io = RX_GPIO_NUM,
										.clkout_io = TWAI_IO_UNUSED,
										.bus_off_io = TWAI_IO_UNUSED,
										.tx_queue_len = 5,
										.rx_queue_len = 5,
										.alerts_enabled = TWAI_ALERT_NONE,
										.clkout_divider = 0,
										.intr_flags = ESP_INTR_FLAG_LEVEL1
									};
	static twai_timing_config_t t_config = {
										.brp = 8,
										.tseg_1 = 15,
										.tseg_2 = 4,
										.sjw = 3,
//										.sjw = 2,
										.triple_sampling = true
									};
	static twai_filter_config_t f_config = {
										.acceptance_code = 0,
										.acceptance_mask = 0xFFFFFFFF,
										.single_filter = false
									};

	t_config.triple_sampling = true;

	can_drv_esp32_init(&g_config, &t_config, &f_config);
	ESP_LOGI(CAN_TAG, "Driver installed");

	can_drv_esp32_start();
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

    static emulator_ctx_t ectx;
    ectx.sem = xSemaphoreCreateBinary();
    cantp_ctx.cb_ctx = (void *)&ectx;
    ectx.cantp_ctx = &cantp_ctx;

	if (cantp_rcvr_params_init(&cantp_ctx, &sndr_params, "Sender") < 0) {
//		return EXIT_FAILURE;
	}

	SemaphoreHandle_t sndr_state_sem = xSemaphoreCreateBinary();
	cantp_ctx.sndr.state_sem = (void *)sndr_state_sem;

	xTaskCreate(cantp_sndr_task, "can_task", 2 * 1024, &cantp_ctx, 1, NULL);

	cantp_rx_task((void *)&cantp_ctx);
}
