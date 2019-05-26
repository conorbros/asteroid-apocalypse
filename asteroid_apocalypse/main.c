#include <stdbool.h>
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

char * spaceship =
"       o       "
"       o       "
"       o       "
"     ooooo     "
"oo   ooooo   oo"
"ooooooooooooooo"
"oo   ooooo   oo"
;

int ship_width = 15;
int ship_height = 7;

int ship_xc = LCD_X / 2;
int ship_yc = LCD_Y - 20;

int shooter_angle = 0;

bool paused = false;

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

void draw_ship() {
	draw_pixels(ship_xc, ship_yc, ship_width, ship_height, spaceship, true);
}

void draw_shield() {
	for (int i = 0; i < LCD_X; i += 10) {
		draw_line(i, 20, i + 5, 20, FG_COLOUR);
	}
}

void draw_everything() {
	draw_ship();
	draw_shield();
}

void enable_inputs() {
	//	Enable input from the Center, Left, Right, Up, and Down switches of the joystick.
	CLEAR_BIT(DDRB, 0);
    CLEAR_BIT(DDRB, 1);
    CLEAR_BIT(DDRD, 0);
    CLEAR_BIT(DDRD, 1);
    CLEAR_BIT(DDRB, 7);

    // Input from the left and right buttons
	CLEAR_BIT(DDRF, 5);
	CLEAR_BIT(DDRF, 6);

	// Enable input from the left thumb wheel
	adc_init();
}


void setup( void ) {
    set_clock_speed(CPU_8MHz);

	enable_inputs();
    //	Initialise the LCD display using the default contrast setting.
    lcd_init(LCD_DEFAULT_CONTRAST);

	draw_everything();
}

void process(void) {
	clear_screen();

    // detect joystick up, down, left right
    if (BIT_IS_SET(PIND, 1) && ship_yc > 0) {
        ship_yc -= 1;
    } else if (BIT_IS_SET(PINB, 7) && ship_yc + ship_height < LCD_Y) {
		// TODO change this to send and display game status
        ship_yc += 1;
    } else if (BIT_IS_SET(PINB, 1) && ship_xc > 0) {
        ship_xc -= 1;
    } else if (BIT_IS_SET(PIND, 0) && ship_xc + ship_width < LCD_X) {
        ship_xc += 1;
    }

	int left_adc = adc_read(0);


	// (V × R2 ÷ R1) + (M2 - M1)
	shooter_angle = (left_adc * (int)(120 / 1024)) - 60;

	char adc_status[15];
    sprintf(adc_status, "A: %d", shooter_angle);
	draw_string(10, 10, adc_status, FG_COLOUR);

	draw_everything();
    show_screen();
}

void manage_loop() {
	if (BIT_IS_SET(PINB, 0)) {
        paused = !paused;
    }
	if (paused) {
		return;
	}
	process();
}


int main(void) {
	setup();

	for ( ;; ) {
		manage_loop();
		_delay_ms(100);
	}

	return 0;
}