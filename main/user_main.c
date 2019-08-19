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

#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "esp_system.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "car.h"
#include "sc.h"

#define PORT	CONFIG_CAR_PORT

static const char *TAG = "main";

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
	
	xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
	car_init();
	ESP_ERROR_CHECK( nvs_flash_init() );
  initialise_wifi();
}
