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
#include "starfighter.h"
#include "helpers.h"

/**
 * Sends a message to the serial device
 *  Parameters:
 *      msg: the message to send
 */
void send_usb_serial(char * msg){
    usb_serial_write((uint8_t *) msg, strlen(msg));
}

/**
 * Gets an input from the usb serial device
 *  Parameters:
 *      wait_msg: The message to display when asking for input
 */
int get_int(char * wait_msg){
    bool complete = false;
    int result = 0;
    int ch_code;

    send_usb_serial(wait_msg);

    while(!complete){
        char usb_buffer[10];
        ch_code = usb_serial_getchar();
        if(ch_code >= 0){
            if(!isdigit(ch_code)){
                break;
            }
            result = result * 10 + (ch_code - '0');
            snprintf(usb_buffer, sizeof(usb_buffer), "%c", ch_code);
            send_usb_serial(usb_buffer);
        }
    }
    send_usb_serial("\r\n");
    return result;
}

/**
 * Sets up the usb serial communication
 */
void setup_usb_serial(void) {
	usb_init();

	while (!usb_configured()) {
        clear_screen();
        draw_string(20, 20, "CONNECT USB", FG_COLOUR);
        show_screen();
	}
}

/**
 * Sends the game status to the serial device
 *  Parameters:
 *      time: the time output char array
 *      lives: the lives output char array
 *      score: the score output char array
 *      asteroid_count: the current asteroid count
 *      boulder_count: the current boulder count
 *      fragment_count: the current fragment count
 *      plasma_count: the current plasma count
 *      velocity: the current velocity
 */
void send_game_status(char * time, char * lives, char * score, int asteroid_count, int boulder_count, int fragment_count, int plasma_count, double velocity){

    send_usb_serial("\r\n");
    send_usb_serial("-Game status-\r\n");

    send_usb_serial(time);
    send_usb_serial("\r\n");
    send_usb_serial(lives);
    send_usb_serial("\r\n");
    send_usb_serial(score);
    send_usb_serial("\r\n");

    char formatted_string[18];

    sprintf(formatted_string, "Asteroids: %d", asteroid_count);
    send_usb_serial(formatted_string);
    send_usb_serial("\r\n");

    sprintf(formatted_string, "Boulders: %d", boulder_count);
    send_usb_serial(formatted_string);
    send_usb_serial("\r\n");

    sprintf(formatted_string, "Fragments: %d", fragment_count);
    send_usb_serial(formatted_string);
    send_usb_serial("\r\n");

    sprintf(formatted_string, "Plasma bolts: %d", plasma_count);
    send_usb_serial(formatted_string);
    send_usb_serial("\r\n");

    sprintf(formatted_string, "Aim: %d", (int)get_shooter_angle());
    send_usb_serial(formatted_string);
    send_usb_serial("\r\n");

    int game_speed = velocity * 10;
    sprintf(formatted_string, "Speed: %d", game_speed);
    send_usb_serial(formatted_string);
    send_usb_serial("\r\n");
}