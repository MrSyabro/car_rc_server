/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "esp_wifi.h"
#include "esp_event.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "car.h"

#define PORT	CONFIG_CAR_PORT

#define WIFI_SSID		CONFIG_ESP_WIFI_SSID
#define WIFI_PASS		CONFIG_ESP_WIFI_PASSWORD
#define MAX_STA_CONN	CONFIG_ESP_MAX_STA_CONN

static const int CONNECTED_BIT = BIT0;

static const char *TAG = "main";

static const char *PROTO = "{services={control={proto=\"raw\"}}\n";

static EventGroupHandle_t wifi_event_group;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
		xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wait_for_ip()
{
    uint32_t bits = BIT0;

    ESP_LOGI(TAG, "Waiting for AP connection...");
    xEventGroupWaitBits(wifi_event_group, bits, false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to AP");
}


void wifi_init_softap()
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .password = WIFI_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s",
             WIFI_SSID, WIFI_PASS);
}

/******************************************************************************
 * FunctionName : read_receive_message
 * Description  : функция разбирает сообщение и выполняет соответствующие команды
 * Parameters   : буфер сообщения, длина буфера
 * Returns      : нихуя
*******************************************************************************/
static void read_receive_message (char *rx_buffer, int len)
{
	for (int i = 0; i < len; i++){
		switch (rx_buffer[i])
			{
			case '1':
				ESP_LOGI(TAG, "Car go forward: %d", rx_buffer[i++]);
				car_go (CAR_GO_FORWARD, (uint8_t)rx_buffer[i]);
				break;
			case '2':
				ESP_LOGI(TAG, "Car go back: %d", rx_buffer[i++]);
				car_go (CAR_GO_BACK, (uint8_t)rx_buffer[i]);
				break;
			case '3':
				ESP_LOGI(TAG, "Car turn left...");
				car_turn (CAR_TURN_LEFT);
				break;
			case '4':
				ESP_LOGI(TAG, "Car turn right...");
				car_turn (CAR_TURN_RIGHT);
				break;
			case '5':
				ESP_LOGI(TAG, "Car stop.");
				car_go (CAR_GO_STOP, 0);
				break;
			default:
				ESP_LOGI(TAG, "Car turn off.");
				car_turn (CAR_TURN_STOP);
				break;
			}
	}
}


/******************************************************************************
 * FunctionName : tcp_server_task
 * Description  : функция запускающая TCP сервер
 * Parameters   : чет какие-то параметры
 * Returns      : нихуя
*******************************************************************************/
static void tcp_server_task(void *pvParameters)
{
	char rx_buffer[128];
	char addr_str[128];
	int addr_family;
	int ip_protocol;

	wait_for_ip ();
	while (1) {
		struct sockaddr_in destAddr;
		destAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		destAddr.sin_family = AF_INET;
		destAddr.sin_port = htons(PORT);
		addr_family = AF_INET;
		ip_protocol = IPPROTO_IP;
		inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

		int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
		if (listen_sock < 0) {
			ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
			break;
		}
		ESP_LOGI(TAG, "Socket created");

		int err = bind(listen_sock, (struct sockaddr *)&destAddr, sizeof(destAddr));
		if (err != 0) {
			ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
			break;
		}
		ESP_LOGI(TAG, "Socket binded");

		err = listen(listen_sock, 1);
		if (err != 0) {
			ESP_LOGE(TAG, "Error occured during listen: errno %d", errno);
			break;
		}
		ESP_LOGI(TAG, "Socket listening");

		struct sockaddr_in sourceAddr;

		uint addrLen = sizeof(sourceAddr);
		int sock = accept(listen_sock, (struct sockaddr *)&sourceAddr, &addrLen);
		if (sock < 0) {
			ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
			break;
		}
		ESP_LOGI(TAG, "Socket accepted");
    
    err = send(sock, PROTO, strlen(PROTO), 0);
    if (err < 0) {
        ESP_LOGE(TAG, "Error occured during sending: errno %d", errno);
        break;
    }else{ ESP_LOGI(TAG, "Protocol data sended"); }
    

		while (1) {
			int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
			// Error occured during receiving
			if (len < 0) {
				ESP_LOGE(TAG, "recv failed: errno %d", errno);
				break;
			}
			// Connection closed
			else if (len == 0) {
				ESP_LOGI(TAG, "Connection closed");
				break;
			}
			// Data received
			else {
				inet_ntoa_r(((struct sockaddr_in *)&sourceAddr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);

				rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
				ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
				ESP_LOGI(TAG, "%s", rx_buffer);
				
				read_receive_message (rx_buffer, len);
			}
		}

		if (sock != -1) {
			ESP_LOGE(TAG, "Shutting down socket and restarting...");
			shutdown(sock, 0);
			close(sock);
		}
	}
	vTaskDelete(NULL);
}



/******************************************************************************
 * FunctionName : app_main
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void app_main(void)
{
	ESP_LOGI(TAG, "Starting RC_CAR...");
	wifi_event_group = xEventGroupCreate();
	xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
	car_init();
	ESP_ERROR_CHECK( nvs_flash_init() );
	wifi_init_softap();
}
