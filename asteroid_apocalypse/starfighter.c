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
static double turret_angle;

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

/**
 * Returns the turret barrel x position
 */
int get_turret_barrel_x(){
    return turret_barrel_x;
}

/**
 * Sets the turret barrel x position
 *  Parameters:
 *      i: new value to set the position to
 */
void set_turret_barrel_x(int i){
    turret_barrel_x = i;
}

/**
 * Returns the turret barrel y position
 */
int get_turret_barrel_y(){
    return turret_barrel_y;
}

/**
 * Sets the turret barrel y position
 *  Parameters:
 *      i: new value to set the position to
 */
void set_turret_barrel_y(int i){
    turret_barrel_y = i;
}

/**
 * Returns the starfighter x position
 */
int get_starfighter_x(){
    return starfighter_x;
}

/**
 * Sets the starfighter x position
 *  Parameters:
 *      i: new value to set the position to
 */
void set_starfighter_x(int i){
    starfighter_x = i;
}

/**
 * Returns the starfighter x position
 */
double get_turret_angle(){
    return turret_angle;
}

/**
 * Sets the turret angle
 *  Parameters:
 *      d: new value to set the angle to
 */
void set_turret_angle(double d){
    turret_angle = d;
}

/**
 * Returns starfighter_moving
 */
bool get_starfighter_moving(){
    return starfighter_moving;
}

/**
 * Sets starfighter_moving
 *  Parameters:
 *      b: new starfighter moving value
 */
void set_starfigher_moving(bool b){
    starfighter_moving = b;
}

/**
 * Returns moving_left
 */
bool get_moving_left(){
    return moving_left;
}

/**
 * Sets moving_left
 *  Parameters:
 *      b: new moving_left value
 */
void set_moving_left(bool b){
    moving_left = b;
}

/**
 * Returns the starfighter char array
 */
char * get_starfighter(){
    return starfigher;
}

/**
 * Processes the starfighter movement and aiming controls
 */
void process_starfighter(){
    long left_adc = adc_read(0);

    if(get_aim_manual_timer()-get_elapsed_time() || get_aim_manual_timer() == 0.0){
        turret_angle = ((double)left_adc * 120.0/1023) - 60.0;
        set_aim_manual_timer(0.0);
    }

    turret_barrel_x = (starfighter_x + ((int)15/2)) + -4 * sin(M_PI * (turret_angle *- 1) / 180);
    turret_barrel_y = 45 + -4 * cos(M_PI * (turret_angle *- 1) / 180);

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

/**
 * Processes a ship left input
 */
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

/**
 * Processes a ship right input
 */
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

/**
 * Processes the ship controls from input
 */
void process_ship_control(){

    // joystick up
    if (BIT_IS_SET(PIND, 1)) {
        //fire cannon
        fire_cannon(turret_angle);

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