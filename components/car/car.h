#include <stdio.h>
#include "esp_system.h"

#ifndef CAR_H
#define CAR_H

#define CAR_GO_FORWARD	1
#define CAR_GO_BACK		2
#define CAR_GO_STOP		0
#define CAR_TURN_LEFT	1
#define CAR_TURN_RIGHT	2
#define CAR_TURN_STOP	0

esp_err_t car_init();
esp_err_t car_go(uint8_t  dir,
                 uint8_t streng);
esp_err_t car_turn(uint8_t dir);

#endif
