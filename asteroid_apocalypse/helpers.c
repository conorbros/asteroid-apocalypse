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

/**
 *  Draws the pixel array supplied on the terminal window, starting at the left and top coordinates
 *
 *  Parameters:
 *      left: the left most x coordinate of the pixel array
 *      top: the top most y coorindate of the pixel array
 *      width: the width of the pixel array
 *      height: the height of the pixel array
 *      pixels: the pixel array to be array
 */
void draw_pixels(int left, int top, int width, int height, char bitmap[], bool space_is_transparent) {
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            if (bitmap[i + j * width] != ' ') {
                draw_pixel(left + i, top + j, FG_COLOUR);
            } else if (!space_is_transparent) {
                draw_pixel(left + i, top + j, FG_COLOUR);
            }
        }
    }
};

/**
 *  Determines whether the coordinate in the supplied pixel array is opaque or not
 */
bool is_opaque(int x, int y, int x0, int y0, int w0, int h0, char pixels[]){
    return x >= x0 && x < x0 + w0
        && y >= y0 && y < y0 + h0
        && pixels[(x-x0) + (y-y0)*w0] != ' ';
}

/**
 *  Determines whether there is a collision between any pixels in the supplied pixel arrays.
 *
 *  Parameters:
 *      x0: the left most x coordinate of the first pixel array
 *      y0: the top most y coordinate of the first pixel array
 *      w0: the width of the first pixel array
 *      h0: the height of the first pixel array
 *      pixels0: the first pixel array
 *
 *      x1: the left most x coordinate of the second pixel array
 *      y1: the top most y coordinate of the second pixel array
 *      w1: the width of the second pixel array
 *      h1: the height of the second pixel array
 *      pixels1: the second pixel array
 *
 *  Returns:
 *      true if collision, false if not
 */
bool pixel_collision(int x0, int y0, int w0, int h0, char pixels0[], int x1, int y1, int w1, int h1, char pixels1[]){
    for ( int j = 0; j < h0; j++ ){
        for (int i = 0; i < w0; i++){
            int x = x0 + i;
            int y = y0 + j;

            if ( is_opaque(x, y, x0, y0, w0, h0, pixels0) && is_opaque(x, y, x1, y1, w1, h1, pixels1)){
                return true;
            }
        }
    }

    return false;
}

/**
 * Prints the controls to the usb serial device
 */
void print_controls(){
    send_usb_serial("\r\n");
    send_usb_serial("Teensy controls: \r\n");
    send_usb_serial("left: move left\r\n");
    send_usb_serial("right: move right\r\n");
    send_usb_serial("up: fire\r\n");
    send_usb_serial("down: display game status\r\n");
    send_usb_serial("centre: pause game\r\n");

    send_usb_serial("Left button: start/restart game\r\n");
    send_usb_serial("Right button: quit\r\n");
    send_usb_serial("Left wheel: set the aim of the turret\r\n");
    send_usb_serial("Right wheel: set the speed of the game\r\n");

    send_usb_serial("\r\n");
    send_usb_serial("Computer controls: \r\n");
    send_usb_serial("a: move left\r\n");
    send_usb_serial("d: move right\r\n");
    send_usb_serial("w: fire \r\n");
    send_usb_serial("s: display game status\r\n");
    send_usb_serial("r: start/reset game\r\n");
    send_usb_serial("p: pause \r\n");
    send_usb_serial("q: quit\r\n");

    send_usb_serial("m: set speed\r\n");
    send_usb_serial("l: set lives\r\n");
    send_usb_serial("g: set score\r\n");
    send_usb_serial("?: print controls\r\n");
    send_usb_serial("h: move ship\r\n");
    send_usb_serial("j: drop asteroid\r\n");

    send_usb_serial("k: drop boulder\r\n");
    send_usb_serial("i: drop fragment\r\n");
}

/**
 * Quits the game and waits for teensy to be turned off
 */
void quit_game(){
    clear_screen();
    for(int i = 0; i <= LCD_Y; i++){
        draw_line(0, i, LCD_X, i, FG_COLOUR);
    }
    draw_string(20, 10, "n10009671", BG_COLOUR);
    show_screen();
    while(1){

    }
}

/**
 * Turns off the teensy backlight
 */
void backlight_off(){
    TC4H = 0 >> 8;
    OCR4A = 0 & 0xff;
}

/**
 * Turns on the teensy backlight
 */
void backlight_on(){
    TC4H = 1023 >> 8;
    OCR4A = 1023 & 0xff;
}

/**
 * Displays the game over screen and waits for input
 */
void game_over(){
    double led_start_time = get_elapsed_time();
    int count = 0;

    //display game status and say game over on computer screen
    game_status(false);
    send_usb_serial("\r\nGAME OVER");

    //turn on left LED
    SET_BIT(PORTB, 2);
    //turn on right LED
    SET_BIT(PORTB, 3);
    backlight_off();
    while(1){

        clear_screen();
        double diff = get_elapsed_time() - led_start_time;

        //turn off leds after 2 seconds
        if(diff >= 2.0){
            CLEAR_BIT(PORTB, 2);
            CLEAR_BIT(PORTB, 3);
            backlight_on();
        }

        //if left button pressed start or reset the game
        if(BIT_IS_SET(PINF, 6)){
            start_or_reset_game();
            return;

        //if right button pressed quit the game
        }else if(BIT_IS_SET(PINF, 5)){
            quit_game();
        }

        draw_string(10, 10, "GAME OVER", FG_COLOUR);

        if(count % 2 == 0){
            draw_string(10, 20, "RESTART OR QUIT!", FG_COLOUR);
        }

        _delay_ms(100);
        count++;
        show_screen();
    }
}

/**
 * Displays the intro message
 */
void intro_message(){
    backlight_on();
    char * smiley =
    "ooooooooooooooooooooo"
	"o                   o"
	"o   o          o    o"
    "o                   o"
    "o                   o"
    "o   oooooooooooo    o"
    "o                   o"
    "ooooooooooooooooooooo";


    int count = 0;
    int smiley_x[5] = {-21, 0, 22, 43, 65};
    while(count < 1000){
        clear_screen();

        //if left button pressed skip intro
        if(BIT_IS_SET(PINF, 6) || usb_serial_getchar() =='r'){
            return;
        }

        draw_string(0, 0, "n10009671", FG_COLOUR);
        draw_string(0, 10, "Asteroid shooter", FG_COLOUR);

        for(int i = 0; i < 5; i++){
            draw_pixels(smiley_x[i], 20,21, 8, smiley, true);
            smiley_x[i]++;
            if(smiley_x[i] > LCD_X){
                smiley_x[i] = -22;
            }
        }
        show_screen();
        count++;
        _delay_ms(100);
    }
}