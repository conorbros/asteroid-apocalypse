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

int fragment_count = 0;
int fragments_x[12];
int fragments_y[12];

int boulder_count = 0;
int boulders_x[6];
int boulders_y[6];

int asteroid_count;
int asteroids_x[3];
int asteroids_y[3];

int plasma_max = 100;
int plasma_count = 0;
int plasma_y[100];
int plasma_x[100];
int plasma_angle[100];

int turret_base_x;
int turret_base_y;
int turret_barrel_x;
int turret_barrel_y;
int ship_width = 15;
int ship_height = 7;
int ship_xc = LCD_X / 2 - ((int)15/2);
int ship_yc = 41;
int shooter_angle = 0;

bool ship_moving = true;
bool moving_left = true;

bool paused = false;
bool quit = false;
int player_points;
int player_lives;

char * spaceship =
"               "
"               "
"               "
"               "
"oo   ooooo   oo"
"ooooooooooooooo"
"oo   ooooo   oo"
;

char * asteroid =
"   o   "
"  ooo  "
" ooooo "
"ooooooo"
" ooooo "
"  ooo  "
"   o   ";

char * boulder =
"  o  "
" ooo "
"ooooo"
" ooo "
"  o  ";

char * fragment =
" o "
"ooo"
" o ";


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

int random_int(int min, int max){
    return (rand() % (max + 1 - min)) + min;
}

bool is_opaque(int x, int y, int x0, int y0, int w0, int h0, char pixels[]){
    return x >= x0 && x < x0 + w0
        && y >= y0 && y < y0 + h0
        && pixels[(x-x0) + (y-y0)*w0] != ' ';
}

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

void remove_boulder(int index){
    for(int i = index; i < boulder_count-1; i++) {
        boulders_x[i] = boulders_x[i+1];
        boulders_y[i] = boulders_y[i+1];
    }
    boulder_count--;
    player_points=+2;
}

void remove_asteroid(int index){
    for(int i = index; i < asteroid_count-1; i++){
        asteroids_x[i] = asteroids_x[i+1];
        asteroids_y[i] = asteroids_y[i+1];
    }
    asteroid_count--;
    player_points=+1;
}

void spawn_fragment(int x, int y){
    fragments_x[fragment_count] = x;
    fragments_y[fragment_count] = y;
    fragment_count++;
}

void draw_fragments(){
    for(int i = 0; i < fragment_count; i++){
        draw_pixels(fragments_x[i], fragments_y[i], 3, 3, fragment, true);
    }
}

void remove_fragment(){
    for(int i = 0; i < fragment_count-1; i++){
        fragments_x[i] = fragments_x[i+1];
        fragments_y[i] = fragments_y[i+1];
    }
    fragment_count--;
    player_points+=4;
}

void process_fragments(){
    for(int i = 0; i < fragment_count; i++){
        fragments_y[i] = fragments_y[i]+1;
        if(fragments_y[i]+3 == 40){
            remove_fragment(i);
            player_lives--;
        }
    }
}

void spawn_boulder(int x, int y){
    boulders_x[boulder_count] = x;
    boulders_y[boulder_count] = y;
    boulder_count++;
}

void draw_boulders(){
    for(int i = 0; i < boulder_count; i++){
        draw_pixels(boulders_x[i], boulders_y[i], 5, 5, boulder, true);
    }
}

void process_boulders(){
    for(int i = 0; i < boulder_count; i++){
        boulders_y[i] = boulders_y[i]+1;
        if(boulders_y[i]+5 == 40){
            remove_boulder(i);
            player_lives--;
        }
    }
}

void spawn_asteroids(){
    asteroid_count = 3;
    for(int i = 0; i < asteroid_count; i++){
        asteroids_x[i] = random_int(0, LCD_X-7);
        asteroids_y[i] = -7;
    }
}

void draw_asteriods(){
    for(int i = 0; i < asteroid_count; i++){
        draw_pixels(asteroids_x[i], asteroids_y[i], 7, 7, asteroid, true);
    }
}

void process_asteroids(){
    for(int i = 0; i < asteroid_count; i++){
        asteroids_y[i] = asteroids_y[i]+1;

        //if asteroid hits shield
        if(asteroids_y[i]+7 == 40){
            remove_asteroid(i);
            player_lives--;
        }
    }

    if(asteroid_count == 0) spawn_asteroids();
}

void draw_ship() {
	draw_pixels(ship_xc, ship_yc, ship_width, ship_height, spaceship, true);
    draw_line(turret_base_x, turret_base_y, turret_barrel_x, turret_barrel_y, FG_COLOUR);
}

void draw_shield() {
	for (int i = 0; i < LCD_X; i += 10) {
		draw_line(i, 39, i+3, 39, FG_COLOUR);
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
    draw_fragments();
    draw_boulders();
    draw_asteriods();
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

    //enable LED
    SET_BIT(DDRB, 2);


    // Enable input from the left thumb wheel
    adc_init();
}

bool is_plasma_offscreen(int x, int y){
    return x <= 0 || y <= 0;
}

void remove_plasma(int index){
    for(int i = index; i < plasma_count-1; i++) {
        plasma_x[i] = plasma_x[i+1];
        plasma_y[i] = plasma_y[i+1];
        plasma_angle[i] = plasma_angle[i+1];
    }
    plasma_count--;
}

void process_plasma(){
    for(int i = 0; i < plasma_count; i++){
        int x1 = plasma_x[i];
        int y1 = plasma_y[i];

        int angle = plasma_angle[i];

        int x2 = x1 + -2 * sin(M_PI * (angle*-1) / 180);
        int y2 = y1 + -2 * cos(M_PI * (angle*-1) / 180);

        if(is_plasma_offscreen(x2, y2)){
            remove_plasma(i);
        }else{
            plasma_x[i] = x2;
            plasma_y[i] = y2;
        }
    }
}

void fire_cannon(int angle){
    if(plasma_count == plasma_max) return;

    plasma_x[plasma_count] = turret_barrel_x;
    plasma_y[plasma_count] = turret_barrel_y;
    plasma_angle[plasma_count] = angle;
    plasma_count++;
}

void process_ship(){
    long left_adc = adc_read(0);

	// (V × R2 ÷ R1) + (M2 - M1)
	shooter_angle = (left_adc * 120/1023) - 60;

	char adc_status[15];
    sprintf(adc_status, "P: %d", player_lives);
	draw_string(10, 10, adc_status, FG_COLOUR);

    turret_base_x = ship_xc + ((int)15/2);
    turret_base_y = 45;
    turret_barrel_x = turret_base_x + -4 * sin(M_PI * (shooter_angle *- 1) / 180);
    turret_barrel_y = turret_base_y + -4 * cos(M_PI * (shooter_angle *- 1) / 180);


    // detect joystick up
    if (BIT_IS_SET(PIND, 1)) {
        //fire cannon
        fire_cannon(shooter_angle);

    //joystick down
    } else if (BIT_IS_SET(PINB, 7)) {
		//send and display game status


    //joystick left
    } else if (BIT_IS_SET(PINB, 1)) {

        //if moving right and left pressed, stop the ship
        if(!moving_left && ship_moving){
            ship_moving = false;

        //if not moving, move left
        }else if(!ship_moving){
            moving_left = true;
            ship_moving = true;
        }

    //joystick right
    } else if (BIT_IS_SET(PIND, 0)) {


        //if moving left and right pressed, stop the ship
        if(moving_left && ship_moving){
            ship_moving = false;

        //if not moving, move right
        }else if(!ship_moving){
            ship_moving = true;
            moving_left = false;
        }
    }

    //if ship is at the left edge, stop
    if(ship_xc == 0 && moving_left){
        ship_moving = false;

    //if ship is at the right edge, stop
    }else if(ship_xc+ship_width == LCD_X && !moving_left){
        ship_moving = false;
    }

    if(ship_moving && moving_left && ship_xc > 0){
        ship_xc -= 1;
    }else if(ship_moving && !moving_left && ship_xc+ship_width < LCD_X){
        ship_xc += 1;
    }
}

void process_collisions(){

    for(int i = 0; i < plasma_count; i++){
        for(int j = 0; j < asteroid_count; j++){
            if(pixel_collision(plasma_x[i], plasma_y[i], 1, 1, "o", asteroids_x[j], asteroids_y[j], 7, 7, asteroid)){

                spawn_boulder(asteroids_x[j], asteroids_y[j]);
                spawn_boulder(asteroids_x[j]+7, asteroids_y[j]);

                remove_asteroid(j);
                remove_plasma(i);
                i--;
                j--;
            }
        }
    }

    for(int i = 0; i < plasma_count; i++){
        for(int j = 0; j < boulder_count; j++){
            if(pixel_collision(plasma_x[i], plasma_y[i], 1, 1, "o", boulders_x[j], boulders_y[j], 7, 7, boulder)){

                spawn_fragment(boulders_x[j], boulders_y[j]);
                spawn_fragment(boulders_x[j]+5, boulders_y[j]);

                remove_boulder(j);
                remove_plasma(i);

                i--;
                j--;
            }
        }
    }

    for(int i = 0; i < plasma_count; i++){
        for(int j = 0; j < fragment_count; j++){
            if(pixel_collision(plasma_x[i], plasma_y[i], 1, 1, "o", fragments_x[j], fragments_y[j], 3, 3, fragment)){

                remove_fragment(j);
                remove_plasma(i);

                i--;
                j--;
            }
        }
    }
}

void process(void) {
	clear_screen();

    process_ship();
    process_plasma();
    process_asteroids();
    process_boulders();
    process_fragments();


    process_collisions();

	draw_everything();

    show_screen();
}

void start_or_reset_game(){
    ship_xc = LCD_X/2 - ((int)15/2);
    ship_yc = 41;
    paused = false;
    plasma_count = 0;
    boulder_count = 0;
    fragment_count = 0;
    player_points = 0;
    player_lives = 5;
    spawn_asteroids();
    ship_moving = true;
    if(random_int(0, 50) > 25){
        moving_left = false;
    }else{
        moving_left = true;
    }
}

void manage_loop(){

    //if center button pressed game is paused
    if (BIT_IS_SET(PINB, 0)) {
        paused = !paused;
    }

    //if left button pressed start or reset game
    if(BIT_IS_SET(PINF, 6)){
        start_or_reset_game();
        SET_BIT(PORTB, 2);
    }

    if(BIT_IS_SET(PINF, 5)){
        quit = true;
    }

	if (paused) {
		return;
	}
    process();
}

void setup( void ) {
    set_clock_speed(CPU_8MHz);

	enable_inputs();
    //	Initialise the LCD display using the default contrast setting.
    lcd_init(LCD_DEFAULT_CONTRAST);

	draw_everything();
}

int main(void) {
    srand(0);
    setup();
    start_or_reset_game();
    while (1) {
        manage_loop();
        _delay_ms(100);

        if(quit) break;
    }

    return 0;
}
