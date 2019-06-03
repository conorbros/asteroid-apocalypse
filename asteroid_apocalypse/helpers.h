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

void draw_pixels(int left, int top, int width, int height, char bitmap[], bool space_is_transparent);

bool is_opaque(int x, int y, int x0, int y0, int w0, int h0, char pixels[]);

bool pixel_collision(int x0, int y0, int w0, int h0, char pixels0[], int x1, int y1, int w1, int h1, char pixels1[]);

void print_controls();

void quit_game();

void backlight_off();

void backlight_on();

void game_over();

void intro_message();