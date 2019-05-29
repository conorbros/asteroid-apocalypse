#include <avr/io.h>
#include <avr/interrupt.h>
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
#include <stdint.h>

double velocity;

int fragment_count = 0;
double fragments_x[12];
double fragments_y[12];

int boulder_count = 0;
double boulders_x[6];
double boulders_y[6];

int asteroid_count;
double asteroids_x[3];
double asteroids_y[3];

int plasma_max = 100;
int plasma_count = 0;
int plasma_x[100];
int plasma_y[100];
double plasma_angle[100];

int turret_base_x;
int turret_base_y;
int turret_barrel_x;
int turret_barrel_y;
int ship_width = 15;
int ship_height = 7;
int ship_xc = LCD_X / 2 - ((int)15/2);
int ship_yc = 41;
double shooter_angle = 0;
double last_plasma_time;

bool ship_moving = true;
bool moving_left = true;

bool paused = false;
bool quit = false;
int player_points;
int player_lives;

volatile uint32_t cycle_count;

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

char * plasma =
"oo"
"oo";


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

double get_elapsed_time(){
    return (cycle_count * 65536.0 + TCNT3 ) * 256.0 / 8000000.0; 
}

int get_seconds_running(){
    double diff = get_elapsed_time();
    int seconds = (int) diff % 60;
    return seconds;
}

int get_minutes_running(){
    double diff = get_elapsed_time();
    int minutes = (int) diff / 60;
    return minutes;
}

void start_timer(){
    cycle_count = 0;
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

    //ensure fragment spawns within the bounds of the screen
    if(x+3 == LCD_X){
        x = LCD_X - (x+3 - LCD_X);
    }else if(x < 0) {
        x = 0;
    }

    fragments_x[fragment_count] = x;
    fragments_y[fragment_count] = y;
    fragment_count++;
}

void draw_fragments(){
    for(int i = 0; i < fragment_count; i++){
        draw_pixels(fragments_x[i], fragments_y[i], 3, 3, fragment, true);
    }
}

void remove_fragment(int index){
    for(int i = index; i < fragment_count-1; i++){
        fragments_x[i] = fragments_x[i+1];
        fragments_y[i] = fragments_y[i+1];
    }
    fragment_count--;
    player_points+=4;
}

void process_fragments(){
    for(int i = 0; i < fragment_count; i++){
        fragments_y[i] = fragments_y[i]+velocity;
        if(fragments_y[i]+3.0 >= 40.0){
            remove_fragment(i);
            player_lives--;
        }
    }
}

void spawn_boulder(int x, int y){

    //ensure boulder spawns within the bounds of the screen
    if(x+5 == LCD_X){
        x = LCD_X - (x+3 - LCD_X);
    }else if(x < 0) {
        x = 0;
    }

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
        boulders_y[i] = boulders_y[i]+velocity;
        if(boulders_y[i]+5.0 >= 40.0){
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
        asteroids_y[i] = asteroids_y[i]+velocity;

        //if asteroid hits shield
        if(asteroids_y[i]+7.0 >= 40.0){
            remove_asteroid(i);
            player_lives--;
        }
    }

    if(asteroid_count == 0 && fragment_count == 0 && boulder_count == 0) {
        spawn_asteroids();
    }
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
        draw_pixels(plasma_x[i], plasma_y[i], 2, 2, plasma, true);
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

        if(is_plasma_offscreen(x1, y1)){
            remove_plasma(i);
        }else{
            plasma_x[i] = x2;
            plasma_y[i] = y2;
        }
    }
}

void fire_cannon(int angle){
    if(plasma_count == plasma_max) return;

    double diff = get_elapsed_time() - last_plasma_time;

    if(diff > 0.2){
        plasma_x[plasma_count] = turret_barrel_x;
        plasma_y[plasma_count] = turret_barrel_y;
        plasma_angle[plasma_count] = angle;
        plasma_count++;
        last_plasma_time = get_elapsed_time();
    }

}

void process_ship(){
    long left_adc = adc_read(0);

	// (V × R2 ÷ R1) + (M2 - M1)
	shooter_angle = ((double)left_adc * 120.0/1023.0) - 60.0;

    turret_base_x = ship_xc + ((int)15/2);
    turret_base_y = 45;
    turret_barrel_x = turret_base_x + -4 * sin(M_PI * (shooter_angle *- 1) / 180);
    turret_barrel_y = turret_base_y + -4 * cos(M_PI * (shooter_angle *- 1) / 180);

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

void process_asteroid_collisions(){
    for(int i = 0; i < plasma_count; i++){
        for(int j = 0; j < asteroid_count; j++){
            if(pixel_collision(plasma_x[i], plasma_y[i], 2, 2, plasma, asteroids_x[j], asteroids_y[j], 7, 7, asteroid)){

                spawn_boulder(asteroids_x[j], asteroids_y[j]);
                spawn_boulder(asteroids_x[j]+7, asteroids_y[j]);

                remove_asteroid(j);
                remove_plasma(i);
                i--;
                j--;
            }
        }
    }
}

void process_boulder_collisions(){
    for(int i = 0; i < plasma_count; i++){
        for(int j = 0; j < boulder_count; j++){
            if(pixel_collision(plasma_x[i], plasma_y[i], 2, 2, plasma, boulders_x[j], boulders_y[j], 7, 7, boulder)){

                spawn_fragment(boulders_x[j], boulders_y[j]);
                spawn_fragment(boulders_x[j]+5, boulders_y[j]);

                remove_boulder(j);
                remove_plasma(i);

                i--;
                j--;
            }
        }
    }
}

void process_fragment_collisions(){
    for(int i = 0; i < plasma_count; i++){
        for(int j = 0; j < fragment_count; j++){
            if(pixel_collision(plasma_x[i], plasma_y[i], 2, 2, plasma, fragments_x[j], fragments_y[j], 3, 3, fragment)){

                remove_fragment(j);
                remove_plasma(i);

                i--;
                j--;
            }
        }
    }
}

void process_collisions(){

    process_asteroid_collisions();    
    process_boulder_collisions();
    process_fragment_collisions();
    
}

void game_status(){
    if(!paused) return;
    
    char time_output[20];
    int minutes = get_minutes_running();
    int seconds = get_seconds_running();

    if (minutes < 10 && seconds < 10){
        sprintf(time_output, "Time: 0%d:0%d", minutes, seconds);
    }else if (minutes > 10 && seconds > 10){
        sprintf(time_output, "Time: %d:%d", minutes, seconds);
    } else if (minutes < 10){
        sprintf(time_output, "Time: 0%d:%d", minutes, seconds);
    } else if (seconds < 10){
        sprintf(time_output, "Time: %d:0%d", minutes, seconds);
    } else if (minutes == 0){
        sprintf(time_output, "Time: 00:%d", seconds);
    }

    char lives_output[20];
    sprintf(lives_output, "Lives: %d", player_lives);

    char score_output[20];
    sprintf(score_output, "Points: %d", player_points);

    while(!BIT_IS_SET(PIND, 1)){
        clear_screen();
        draw_string(0, 0, time_output, FG_COLOUR);
        draw_string(0, 20, lives_output, FG_COLOUR);
        draw_string(0, 30, score_output, FG_COLOUR);
        _delay_ms(100);
        show_screen();
    }
    clear_screen();
    draw_everything();
    show_screen();
}

void process_input(){

    // joystick up
    if (BIT_IS_SET(PIND, 1)) {
        //fire cannon
        fire_cannon(shooter_angle);

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

    long right_adc = adc_read(1);
    velocity = right_adc/(double)1023;
}

void process(void) {
	clear_screen();

    process_input();

    process_ship();
    process_plasma();
    process_asteroids();
    process_boulders();
    process_fragments();

    process_collisions();

    //char adc_status[15];
    //sprintf(adc_status, "A: %f", shooter_angle);
	//draw_string(10, 10, adc_status, FG_COLOUR);

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
    last_plasma_time = 1.0;

    start_timer();

    if(random_int(0, 50) > 25){
        moving_left = false;
    }else{
        moving_left = true;
    }
}

void manage_loop(){

    //if center joystick pressed game is paused
    if (BIT_IS_SET(PINB, 0)) {
        paused = !paused;
    }

    if (BIT_IS_SET(PINB, 7)) {
		//send and display game status
        game_status();
    }

    //if left button pressed start or reset game
    if(BIT_IS_SET(PINF, 6)){
        start_or_reset_game();
        //SET_BIT(PORTB, 2);
    }

    //if right button pressed quit the game
    if(BIT_IS_SET(PINF, 5)){
        quit = true;
    }

	if (paused) {
		return;
	}
    process();
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

void setup_timer(void){

    //Initialise Timer 3 in normal mode so that it overflows with a period of approximately 2.1 seconds.
	TCCR3A = 0;
	TCCR3B = 4;

    //Enable timer overflow for Timer 3
	TIMSK3 = 1;

    //turn on interupts
	sei();
}

ISR(TIMER3_OVF_vect){
    if(!paused){
        cycle_count++;
    }
}

void intro_message(){
    char * smiley =
    "ooooooooooooooooooooo"
	"o                   o"
	"o   ooooo	  ooooo  o"
    "o     o        o    o" 
    "o   ooooo    ooooo  o"
    "o                   o"
    "o        o          o"
    "o         o         o"
    "o          o        o"
    "o        oooo       o"
    "o                   o"
    "o   oooooooooooooo  o"
    "o    o          o   o"
    "o     o        o    o"
    "o      ooooooo      o"
	"o                   o"
	"ooooooooooooooooooooo";

    
    int count = 0;
    int smiley_x[5] = {-21, 0, 22, 43, 65};
    while(count < 1000){
        clear_screen();

        //if left button pressed skip intro
        if(BIT_IS_SET(PINF, 6)){
            return;
        }

        draw_string(0, 0, "n10009671", FG_COLOUR);
        draw_string(0, 10, "ASS APOCALYSE!", FG_COLOUR);

        for(int i = 0; i < 5; i++){
            draw_pixels(smiley_x[i], 20,21, 17, smiley, true);
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

void setup( void ) {
    set_clock_speed(CPU_8MHz);

	enable_inputs();
    //	Initialise the LCD display using the default contrast setting.
    lcd_init(LCD_DEFAULT_CONTRAST);
    setup_timer();
	draw_everything();
}

int main(void) {
    srand(0);
    setup();
    intro_message();
    start_or_reset_game();

    while (1) {
        manage_loop();
        _delay_ms(100);

        if(quit) break;
    }
    return 0;
}
