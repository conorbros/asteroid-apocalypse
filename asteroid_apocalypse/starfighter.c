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
#include "z_main.h"

static int turret_barrel_x;
static int turret_barrel_y;
static int starfighter_x;
static double shooter_angle;

static bool starfighter_moving = true;
static bool moving_left;

char * starfigher =
"               "
"               "
"               "
"               "
"oo   ooooo   oo"
"ooooooooooooooo"
"oo   ooooo   oo"
;

int get_turret_barrel_x(){
    return turret_barrel_x;
}

void set_turret_barrel_x(int i){
    turret_barrel_x = i;
}

int get_turret_barrel_y(){
    return turret_barrel_y;
}

void set_turret_barrel_y(int i){
    turret_barrel_y = i;
}

int get_starfighter_x(){
    return starfighter_x;
}

void set_starfighter_x(int i){
    starfighter_x = i;
}

double get_shooter_angle(){
    return shooter_angle;
}

void set_shooter_angle(double d){
    shooter_angle = d;
}

bool get_starfighter_moving(){
    return starfighter_moving;
}

void set_starfigher_moving(bool b){
    starfighter_moving = b;
}

bool get_moving_left(){
    return moving_left;
}

void set_moving_left(bool b){
    moving_left = b;
}

char * get_starfighter(){
    return starfigher;
}

void process_starfighter(){
    long left_adc = adc_read(0);

    if(get_aim_manual_timer()-get_elapsed_time() || get_aim_manual_timer() == 0.0){
        shooter_angle = ((double)left_adc * 120.0/1023) - 60.0;
        set_aim_manual_timer(0.0);
    }

    turret_barrel_x = (starfighter_x + ((int)15/2)) + -4 * sin(M_PI * (shooter_angle *- 1) / 180);
    turret_barrel_y = 45 + -4 * cos(M_PI * (shooter_angle *- 1) / 180);

    //if ship is at the left edge, stop
    if(starfighter_x == 0 && moving_left){
        starfighter_moving = false;

    //if ship is at the right edge, stop
    }else if(starfighter_x+15 == LCD_X && !moving_left){
        starfighter_moving = false;
    }

    if(starfighter_moving && moving_left && starfighter_x > 0){
        starfighter_x -= 1;
    }else if(starfighter_moving && !moving_left && starfighter_x+15 < LCD_X){
        starfighter_x += 1;
    }
}

void ship_left(){
    //if moving right and left pressed, stop the ship
    if(!moving_left && starfighter_moving){
        starfighter_moving = false;

    //if not moving, move left
    }else if(!starfighter_moving){
        moving_left = true;
        starfighter_moving = true;
    }
}

void ship_right(){
    //if moving left and right pressed, stop the ship
    if(moving_left && starfighter_moving){
        starfighter_moving = false;

    //if not moving, move right
    }else if(!starfighter_moving){
        starfighter_moving = true;
        moving_left = false;
    }
}

void process_ship_control(){

    // joystick up
    if (BIT_IS_SET(PIND, 1)) {
        //fire cannon
        fire_cannon(shooter_angle);

    //joystick left
    } else if (BIT_IS_SET(PINB, 1)) {
        ship_left();

    //joystick right
    } else if (BIT_IS_SET(PIND, 0)) {
        ship_right();
    }

    long right_adc = adc_read(1);

    if(get_elapsed_time()-get_speed_manual_timer() >= 1.0 || get_speed_manual_timer() == 0.0){
        set_velocity(right_adc/(double)1023);
        set_speed_manual_timer(0.0);
    }
}