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

int get_turret_barrel_x();

void set_turret_barrel_x(int i);

int get_turret_barrel_y();

void set_turret_barrel_y(int i);

int get_starfighter_x();

void set_starfighter_x(int i);

double get_turret_angle();

void set_turret_angle(double d);

bool get_starfighter_moving();

void set_starfigher_moving(bool b);

bool get_moving_left();

void set_moving_left(bool b);

char * get_starfighter();

void process_starfighter();

void ship_left();

void ship_right();

void process_ship_control();

void send_game_status(char * time, char * lives, char * score, int asteroid_count, int boulder_count, int fragment_count, int plasma_count, double velocity);