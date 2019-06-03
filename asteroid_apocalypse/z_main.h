#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <util/delay.h>
#include <cpu_speed.h>
#include <macros.h>
#include <graphics.h>
#include <lcd.h>
#include "lcd_model.h"
#include "cab202_adc.h"
#include <math.h>
#include <stdint.h>
#include "usb_serial.h"
#include <ctype.h>
#include "mybool.h"
#include "serial_comms.h"
#include "starfighter.h"

double get_elapsed_time();

double get_velocity();

void set_velocity(double d);

double get_speed_manual_timer();

void set_speed_manual_timer(double d);

double get_aim_manual_timer();

void set_aim_manual_timer(double d);

void fire_cannon(int angle);

void game_status(bool p);

void start_or_reset_game();