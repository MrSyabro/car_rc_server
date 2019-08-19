#include "car.h"
#include "driver/pwm.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define PWM_PERIOD				(1000)
#define CAR_PIN_FORWARD		4
#define CAR_PIN_BACK			5
#define CAR_PIN_LEFT			14
#define CAR_PIN_RIGHT			13
#define CAR_PIN_MASK			((1ULL<<CAR_PIN_LEFT) | (1ULL<<CAR_PIN_RIGHT))

static const char *TAG = "car";

const uint32_t pin_num[2] = {CAR_PIN_FORWARD, CAR_PIN_BACK};

// duties table, real_duty = duties[x]/PERIOD
uint32_t duties[2] = { 0, 0 };

// phase table, delay = (phase[x]/360)*PERIOD
int16_t phases[2] = {0, 0};

esp_err_t car_init()
{
	ESP_LOGI(TAG, "Initializing car drivers...");
	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pin_bit_mask = CAR_PIN_MASK;
	io_conf.pull_down_en = 0;
	io_conf.pull_up_en = 0;
	ESP_ERROR_CHECK(gpio_config(&io_conf));
	
	ESP_ERROR_CHECK(pwm_init(PWM_PERIOD, duties, 2, pin_num));
	//pwm_set_channel_invert(0x1 << 0);
	pwm_set_phases(phases);
	pwm_start();
	//pwm_stop(0x0);

	return ESP_OK;
}

esp_err_t car_go(uint8_t  dir,
									uint8_t streng)
{
	ESP_LOGI(TAG, "Motor %d set duty to %d!", dir, streng*4);
	switch (dir)
		{
		case CAR_GO_FORWARD:
			ESP_ERROR_CHECK(pwm_set_duty(0, (uint32_t)streng*4));
			pwm_start();
			break;
		case CAR_GO_BACK:
			ESP_ERROR_CHECK(pwm_set_duty(1, (uint32_t)streng*4));
			pwm_start();
			break;
		case CAR_GO_STOP:
			pwm_stop(0x0);
			break;
		}

	return ESP_OK;
}

esp_err_t car_turn(uint8_t dir)
{
	switch (dir)
		{
		case CAR_TURN_LEFT:
			gpio_set_level(CAR_PIN_LEFT, 1);
			break;
		case CAR_TURN_RIGHT:
			gpio_set_level(CAR_PIN_RIGHT, 1);
			break;
		case CAR_TURN_STOP:
			gpio_set_level(CAR_PIN_LEFT, 0);
			gpio_set_level(CAR_PIN_RIGHT, 0);
			break;
		}
	
	return ESP_OK;
}
