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

double velocity;

int fragment_count = 0;
int fragments_x[20];
double fragments_y[20];

int boulder_count = 0;
int boulders_x[10];
double boulders_y[10];

int asteroid_count;
int asteroids_x[5];
double asteroids_y[5];

int plasma_count = 0;
double plasma_x[20];
double plasma_y[20];
double plasma_angle[20];

int turret_barrel_x;
int turret_barrel_y;
int starfighter_x = LCD_X / 2 - ((int)15/2);
double shooter_angle = 0;
double last_plasma_time;

bool starfighter_moving = true;
bool moving_left = true;

bool flash_left_led = false;
double led_timer;
bool flash_right_led = false;
double last_flash;
bool spawning_asteroids;

bool new_game = true;
bool paused = false;
int player_points;
int player_lives;

double aim_manual_timer = 0.0;
double speed_manual_timer = 0.0;

volatile uint32_t cycle_count;

char * starfigher =
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
    return (int) get_elapsed_time() % 60;
}

int get_minutes_running(){
    return (int) get_elapsed_time() / 60;
}

void start_timer(){
    cycle_count = 0;
}

void send_usb_serial(char * msg){
    usb_serial_write((uint8_t *) msg, strlen(msg));
}

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

void setup_usb_serial(void) {
	usb_init();

	while (!usb_configured()) {
        clear_screen();
        draw_string(20, 20, "CONNECT USB", FG_COLOUR);
        show_screen();
	}
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
    if(x+3 > LCD_X){
        x = LCD_X - (x + 3 - LCD_X);
    }else if(x < 0) {
        x = 0;
    }

    fragments_x[fragment_count] = x;
    fragments_y[fragment_count] = y;
    fragment_count++;
}

void remove_fragment(int index){
    for(int i = index; i < fragment_count-1; i++){
        fragments_x[i] = fragments_x[i+1];
        fragments_y[i] = fragments_y[i+1];
    }
    fragment_count--;
    player_points+=4;
}

void spawn_boulder(int x, int y){
    //ensure boulder spawns within the bounds of the screen
    if(x+5 > LCD_X){
        x = LCD_X - (x+3 - LCD_X);
    }else if(x < 0) {
        x = 0;
    }

    boulders_x[boulder_count] = x;
    boulders_y[boulder_count] = y;
    boulder_count++;
}

void flash_warning_lights(){
    int left_asteroids = 0;
    int right_asteroids = 0;

    for(int i = 0; i < asteroid_count; i++){
        if(asteroids_x[i]+3 <= LCD_X/2){
            left_asteroids++;
        }else{
            right_asteroids++;
        }
    }

    if(left_asteroids > right_asteroids){
        flash_left_led = true;
        flash_right_led = false;
    }else{
        flash_right_led = true;
        flash_left_led = false;
    }
}

void spawn_asteroids(){
    spawning_asteroids = true;
    asteroid_count = 3;
    int x = 0;
    for(int i = 0; i < asteroid_count; i++){

        bool overlap = true;
        while(overlap && i > 0){
            x = random_int(0, LCD_X-7);
            int x_right = x+7.0;

            for(int l = 0; l < i; l++){
                int x1 = asteroids_x[l];
                int x2 = asteroids_x[l]+7.0;

                if((x >= x1 && x <= x2) || (x_right >= x1 && x_right <= x2)){
                    overlap = true;
                }
            }
            overlap = false;
        }

        asteroids_x[i] = x;
        asteroids_y[i] = -8;
    }

    flash_warning_lights();
}

void process_objects(){
    if(spawning_asteroids) return;

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

    for(int i = 0; i < boulder_count; i++){
        boulders_y[i] = boulders_y[i]+velocity;
        if(boulders_y[i]+5.0 >= 40.0){
            remove_boulder(i);
            player_lives--;
        }
    }

    for(int i = 0; i < fragment_count; i++){
        fragments_y[i] = fragments_y[i]+velocity;
        if(fragments_y[i]+3.0 >= 40.0){
            remove_fragment(i);
            player_lives--;
        }
    }
}

void left_LED_flash(){
    //clear the left led variables ready for the next flash
    if(get_elapsed_time()-led_timer >= 2.0){
        flash_left_led = false;
        led_timer = 0;
        last_flash = 0;
        CLEAR_BIT(PORTB, 2);
        spawning_asteroids = false;
        return;
    }

    if(get_elapsed_time()-last_flash >= 0.5){
        CLEAR_BIT(PORTB, 2);
        last_flash = get_elapsed_time();
    }else{
        SET_BIT(PORTB, 2);
    }
}

void right_LED_flash(){
    //clear the right led variables ready for the next flash
    if(get_elapsed_time()-led_timer >= 2.0){
        flash_right_led = false;
        led_timer = 0;
        last_flash = 0;
        CLEAR_BIT(PORTB, 3);
        spawning_asteroids = false;
        return;
    }

    if(get_elapsed_time()-last_flash >= 0.5){
        CLEAR_BIT(PORTB, 3);
        last_flash = get_elapsed_time();
    }else{
        SET_BIT(PORTB, 3);
    }
}

void draw_all() {

    //draw starfighter
    draw_pixels(starfighter_x, 41, 15, 7, starfigher, true);
    draw_line(starfighter_x+((int)15/2), 45, turret_barrel_x, turret_barrel_y, FG_COLOUR);

    //draw shield
    for (int i = 0; i < LCD_X; i += 10) {
		draw_line(i, 39, i+3, 39, FG_COLOUR);
	}

    //draw plasma
    for(int i = 0; i < plasma_count; i++){
        draw_pixels(plasma_x[i], plasma_y[i], 2, 2, plasma, true);
    }

    //draw fragments
    for(int i = 0; i < fragment_count; i++){
        draw_pixels(fragments_x[i], fragments_y[i], 3, 3, fragment, true);
    }

    //draw boulders
    for(int i = 0; i < boulder_count; i++){
        draw_pixels(boulders_x[i], boulders_y[i], 5, 5, boulder, true);
    }

    //draw asteroids
    for(int i = 0; i < asteroid_count; i++){
        draw_pixels(asteroids_x[i], asteroids_y[i], 7, 7, asteroid, true);
    }
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
        double x1 = plasma_x[i];
        double y1 = plasma_y[i];

        double angle = plasma_angle[i];

        double x2 = x1 + -1 * sin(M_PI * (angle*-1) / 180);
        double y2 = y1 + -1 * cos(M_PI * (angle*-1) / 180);

        if(is_plasma_offscreen(x1, y1)){
            remove_plasma(i);
        }else{
            plasma_x[i] = x2;
            plasma_y[i] = y2;
        }
    }
}

void fire_cannon(int angle){
    if(plasma_count == 20) return;

    double diff = get_elapsed_time() - last_plasma_time;

    if(diff > 0.2){
        plasma_x[plasma_count] = turret_barrel_x;
        plasma_y[plasma_count] = turret_barrel_y;
        plasma_angle[plasma_count] = angle;
        plasma_count++;
        last_plasma_time = get_elapsed_time();
    }
}

void process_starfighter(){
    long left_adc = adc_read(0);

    if(aim_manual_timer-get_elapsed_time() || aim_manual_timer == 0.0){
        shooter_angle = ((double)left_adc * 120.0/1023) - 60.0;
        aim_manual_timer = 0.0;
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

void send_game_status(char * time, char * lives, char * score){

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

    sprintf(formatted_string, "Aim: %d", (int)shooter_angle);
    send_usb_serial(formatted_string);
    send_usb_serial("\r\n");

    int game_speed = velocity * 10;
    sprintf(formatted_string, "Speed: %d", game_speed);
    send_usb_serial(formatted_string);
    send_usb_serial("\r\n");
}


void game_status(bool p){
    char time_output[12];
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

    char lives_output[10];
    sprintf(lives_output, "Lives: %d", player_lives);

    char score_output[11];
    sprintf(score_output, "Points: %d", player_points);

    //send game status to computer and display on teensy if game is paused
    send_game_status(time_output, lives_output, score_output);
    if(!p) return;

    //wait until center joystick pressed to continue game
    while(!BIT_IS_SET(PINB, 0)){
        clear_screen();
        draw_string(0, 0, time_output, FG_COLOUR);
        draw_string(0, 20, lives_output, FG_COLOUR);
        draw_string(0, 30, score_output, FG_COLOUR);
        _delay_ms(100);
        show_screen();
    }
    paused = false;
    clear_screen();
    draw_all();
    show_screen();
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

    if(get_elapsed_time()-speed_manual_timer >= 1.0 || speed_manual_timer == 0.0){
        velocity = right_adc/(double)1023;
        speed_manual_timer = 0.0;
    }
}

void start_or_reset_game(){
    clear_screen();
    CLEAR_BIT(PORTB, 2);
    CLEAR_BIT(PORTB, 3);

    starfighter_x = LCD_X/2 - ((int)15/2);
    paused = true;
    new_game = true;
    flash_left_led = false;
    flash_right_led = false;
    led_timer = 0.0;

    plasma_count = 0;
    boulder_count = 0;
    fragment_count = 0;
    player_points = 0;
    player_lives = 5;

    spawn_asteroids();

    starfighter_moving = true;
    last_plasma_time = 1.0;
    last_flash = 0.0;
    shooter_angle = 0.0;

    turret_barrel_x = starfighter_x + ((int)15/2);
    turret_barrel_y = 45 + -4;

    aim_manual_timer = 0.0;
    speed_manual_timer = 0.0;

    if(random_int(0, 50) > 25){
        moving_left = false;
    }else{
        moving_left = true;
    }

    draw_all();
    show_screen();
}

void set_player_points(){
    player_points = get_int("Set the player's score: ");
}

void set_player_lives(){
    player_lives = get_int("Set the player's lives: ");
}

void move_ship(){
    int new_x = get_int("Move the ship to coorindate: ");

    //if ship goes out of bounds move it in bounds;
    if(new_x < 0){
        new_x = 0;
    }else if(new_x+15 > LCD_X){
        new_x = LCD_X-15;
    }

    starfighter_x = new_x;
}

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

void drop_fragment(){
    if(fragment_count >= 20) return;

    int x = get_int("\r\nFragment x: ");
    int y = get_int("\r\nFragment y: ");

    fragments_x[fragment_count] = x;
    fragments_y[fragment_count] = (double)y;
    fragment_count++;
}

void drop_boulder(){
    if(boulder_count >= 10) return;

    boulders_x[boulder_count] = get_int("\r\nBoulder x: ");
    boulders_y[boulder_count] = (double)get_int("\r\nBoulder y: ");
    boulder_count++;
}

void drop_asteroid(){
    if(asteroid_count >= 5) return;

    asteroids_x[asteroid_count] = get_int("\r\nAsteroid x: ");
    asteroids_y[asteroid_count] = (double)get_int("\r\nAsteroid y: ");
    asteroid_count++;
}

void set_turrent_aim(){
    int aim = get_int("\r\nAim: ");

    if(aim < 0){
        aim = 0;
    }else if(aim > 1023){
        aim = 1023;
    }

    shooter_angle = ((double)aim * 120.0/1023) - 60.0;
    aim_manual_timer = get_elapsed_time();
}

void set_game_speed(){
    int speed = get_int("\r\nSpeed: ");

    if(speed < 0){
        speed = 0;
    }else if(speed > 1023){
        speed = 1023;
    }

    velocity = (double)speed/(double)1023;
    speed_manual_timer = get_elapsed_time();
}

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

void serial_input(int16_t input){
    switch (input){
        //move ship left
        case 'a':
            ship_left();
            break;

        //move ship right
        case 'd':
            ship_right();
            break;

        //fire cannon
        case 'w':
            fire_cannon(shooter_angle);
            break;

        //send and display game status
        case 's':
            game_status(paused);
            break;

        //start/reset game
        case 'r':
            start_or_reset_game();
            break;

        //pause game
        case 'p':
            paused = !paused;
            break;

        //quit game
        case 'q':
            quit_game();
            break;

        //set aim of turret
        case 't':
            set_turrent_aim();
            break;

        //set the speed of the game
        case 'm':
            set_game_speed();
            break;

        //set player lives
        case 'l':
            set_player_lives();
            break;

        //set the player score
        case 'g':
            set_player_points();
            break;

        //display help instructions on screen
        case '?':
            print_controls();
            break;

        //move starfigher to coordinate
        case 'h':
            move_ship();
            break;

        //place asteroid at coordinate
        case 'j':
            drop_asteroid();
            break;

        //place boulder at coordinate
        case 'k':
            drop_boulder();
            break;

        //place fragment at coordinate
        case 'i':
            drop_fragment();
            break;

        default:
            break;
        }
}

void process_serial_input(){
    int16_t input = usb_serial_getchar();
	if (input >= 0) {
        serial_input(input);
	}
}

void process(void) {
	clear_screen();

    if(new_game && !paused){
        start_timer();
        new_game = false;
        send_usb_serial("\r\nGAME STARTED");
    }

    if(!paused){
        process_ship_control();
        process_starfighter();
        process_plasma();
        process_objects();
        process_collisions();
    }

    //char adc_status[15];
    //sprintf(adc_status, "A: %f", shooter_angle);
	//draw_string(10, 10, adc_status, FG_COLOUR);

	draw_all();
    show_screen();
}

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

    while(1){

        clear_screen();
        double diff = get_elapsed_time() - led_start_time;

        //turn off leds after 2 seconds
        if(diff >= 2.0){
            CLEAR_BIT(PORTB, 2);
            CLEAR_BIT(PORTB, 3);
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

void manage_loop(){
    process_serial_input();

    //if center joystick pressed toggle paused
    if (BIT_IS_SET(PINB, 0)) {
        paused = !paused;
    }

    //if joystick down
    if (BIT_IS_SET(PINB, 7)) {
		//send and display game status
        game_status(paused);
    }

    //if left button pressed start or reset game
    if(BIT_IS_SET(PINF, 6)){
        start_or_reset_game();
        //SET_BIT(PORTB, 2);
    }

    //if right button pressed quit the game
    if(BIT_IS_SET(PINF, 5)){
        quit_game();
    }

    if(flash_left_led && !paused){
        //if the led is starting to flash start its timer
        if(!led_timer){
            led_timer = get_elapsed_time();
        }
        left_LED_flash();
    }

    if(flash_right_led && !paused){
        //if the led is starting to flash start its timer
        if(!led_timer){
            led_timer = get_elapsed_time();
        }
        right_LED_flash();
    }

    if(player_lives <= 0){
        game_over();
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

void setup( void ) {
    set_clock_speed(CPU_8MHz);

	enable_inputs();
    //	Initialise the LCD display using the default contrast setting.
    lcd_init(LCD_HIGH_CONTRAST);
    setup_timer();
    setup_usb_serial();
	draw_all();
}

int main(void) {
    srand(0);
    setup();
    intro_message();
    start_or_reset_game();

    while (1) {
        manage_loop();
        _delay_ms(100);
    }
    return 0;
}
