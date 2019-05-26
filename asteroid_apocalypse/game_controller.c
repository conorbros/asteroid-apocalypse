#include <avr/io.h>
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
#include <math.h>


int plasma_count = 0;
int plasma_y[20];
int plasma_x[20];
int plasma_angle[20];

int turret_barrel_x;
int turret_barrel_y;

int ship_width = 15;
int ship_height = 7;

int ship_xc = LCD_X / 2 - ((int)15/2);
int ship_yc = 41;

int shooter_angle = 0;

bool paused = false;
bool quit = false;

char * spaceship =
"               "
"               "
"               "
"               "
"oo   ooooo   oo"
"ooooooooooooooo"
"oo   ooooo   oo"
;

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

void draw_turret(){
    int x1 = ship_xc + ((int)15/2);
    int y1 = 45;

    int y2 = y1 + -4 * cos(M_PI * shooter_angle / 180);
    int x2 = x1 + -4 * sin(M_PI * shooter_angle / 180);

    turret_barrel_x = x2;
    turret_barrel_y = y2;

    draw_line(x1, y1, x2, y2, FG_COLOUR);
}

void draw_ship() {
	draw_pixels(ship_xc, ship_yc, ship_width, ship_height, spaceship, true);
    draw_turret();
}

void draw_shield() {
	for (int i = 0; i < LCD_X; i += 10) {
		draw_line(i, 39, i + 3, 39, FG_COLOUR);
	}
}

void draw_plasma(){
    for(int i = 0; i < plasma_count; i++){
        draw_pixel(plasma_x[i], plasma_y[i], FG_COLOUR);
    }
}

void draw_everything() {
	draw_ship();
	draw_shield();
    draw_plasma();
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

bool is_plasma_offscreen(int x, int y){
    return x < 0 || y < 0;
}

void remove_plasma(int index){
    for(int i = index; i < plasma_count - 1; i++) {
        plasma_x[i] = plasma_x[i + 1];
        plasma_y[i] = plasma_y[i + 1];
    }
    plasma_count--;
}

void process_plasma(){
    for(int i = 0; i < plasma_count; i++){
        int x1 = plasma_x[i];
        int y1 = plasma_y[i];

        int y2 = y1 + -1 * cos(M_PI * plasma_angle[i] / 180);
        int x2 = x1 + -1 * sin(M_PI * plasma_angle[i] / 180);

        if(is_plasma_offscreen(x2, y2)){
            remove_plasma(i);
        }else{
            plasma_x[i] = x2;
            plasma_y[i] = y2;
        }
    }
}

void fire_cannon(){
    plasma_x[plasma_count] = turret_barrel_x;
    plasma_y[plasma_count] = turret_barrel_y;
    plasma_angle[plasma_count] = shooter_angle;
    plasma_count++;
}

void process(void) {
	clear_screen();

    // detect joystick up
    if (BIT_IS_SET(PIND, 1) && ship_yc > 0) {
        //fire cannon
        fire_cannon();
    //joystick down
    } else if (BIT_IS_SET(PINB, 7) && ship_yc + ship_height < LCD_Y) {
		//send and display game status

    //joystick left
    } else if (BIT_IS_SET(PINB, 1) && ship_xc > 0) {
        ship_xc -= 1;

    //joystick right
    } else if (BIT_IS_SET(PIND, 0) && ship_xc + ship_width < LCD_X) {
        ship_xc += 1;
    }

    long left_adc = adc_read(0);

	// (V × R2 ÷ R1) + (M2 - M1)
	shooter_angle = (left_adc * 120 / 1023) - 60;

	char adc_status[15];
    sprintf(adc_status, "A: %d", shooter_angle);
	draw_string(10, 10, adc_status, FG_COLOUR);

    process_plasma();
	draw_everything();
    show_screen();
}

void start_or_reset_game(){
    ship_xc = LCD_X / 2 - ((int)15/2);
    ship_yc = 41;

    paused = false;
}

void manage_loop(){

    //if center button pressed game is paused
    if (BIT_IS_SET(PINB, 0)) {
        paused = !paused;
    }

    //if left button pressed start or reset game
    if(BIT_IS_SET(PINF, 6)){
        start_or_reset_game();
    }

    if(BIT_IS_SET(PINF, 5)){
        quit = true;
    }

	if (paused) {
		return;
	}

    process();
}


int main(void) {
    setup();

    while (1) {
        manage_loop();
        _delay_ms(100);

        if(quit) break;
    }

    return 0;
}
