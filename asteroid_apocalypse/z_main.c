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
#include "starfighter.h"
#include "helpers.h"

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

double last_plasma_time;

bool flash_left_led;
double led_timer;
bool flash_right_led;
double last_flash;
bool spawning_asteroids;

bool new_game;
bool paused;
int player_points;
int player_lives;

double aim_manual_timer = 0.0;
double speed_manual_timer = 0.0;

volatile uint32_t cycle_count;

#define BIT(x) (1 << (x))

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


int random_int(int min, int max){
    return (rand() % (max + 1 - min)) + min;
}

double get_velocity(){
    return velocity;
}

void set_velocity(double d){
    velocity = d;
}

double get_speed_manual_timer(){
    return speed_manual_timer;
}

void set_speed_manual_timer(double d){
    speed_manual_timer = d;
}

double get_aim_manual_timer(){
    return speed_manual_timer;
}

void set_aim_manual_timer(double d){
    speed_manual_timer = d;
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
    if(asteroid_count == 0 && fragment_count == 0 && boulder_count == 0) {
        spawn_asteroids();
    }

    if(spawning_asteroids) return;
    for(int i = 0; i < asteroid_count; i++){
        asteroids_y[i] = asteroids_y[i]+velocity;
        //if asteroid hits shield
        if(asteroids_y[i]+7.0 >= 40.0){
            remove_asteroid(i);
            player_lives--;
        }
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
    double diff = get_elapsed_time()-led_timer;

    //clear the left led variables ready for the next flash
    if(diff > 2.0){
        flash_left_led = false;
        led_timer = 0.0;
        last_flash = 0.0;
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
    double diff = get_elapsed_time()-led_timer;

    //clear the right led variables ready for the next flash
    if(diff > 2.0){
        flash_right_led = false;
        led_timer = 0.0;
        last_flash = 0.0;
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
    draw_pixels(get_starfighter_x(), 41, 15, 7, get_starfighter(), true);
    draw_line(get_starfighter_x()+((int)15/2), 45, get_turret_barrel_x(), get_turret_barrel_y(), FG_COLOUR);

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

    if(diff >= 0.2){
        plasma_x[plasma_count] = get_turret_barrel_x();
        plasma_y[plasma_count] = get_turret_barrel_y();
        plasma_angle[plasma_count] = angle;
        plasma_count++;
        last_plasma_time = get_elapsed_time();
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
    send_game_status(time_output, lives_output, score_output, asteroid_count, boulder_count, fragment_count, plasma_count, velocity);
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

void start_or_reset_game(){
    clear_screen();
    CLEAR_BIT(PORTB, 2);
    CLEAR_BIT(PORTB, 3);

    set_starfighter_x(LCD_X/2 - ((int)15/2));
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
    last_flash = 0.0;
    cycle_count = 0;

    spawn_asteroids();

    set_starfigher_moving(true);
    last_plasma_time = 1.0;

    set_shooter_angle(0.0);

    set_turret_barrel_x(get_starfighter_x() + ((int)15/2));
    set_turret_barrel_y(45 + -4);

    aim_manual_timer = 0.0;
    speed_manual_timer = 0.0;

    if(random_int(0, 50) > 25){
        set_moving_left(false);
    }else{
        set_moving_left(true);
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

    set_starfighter_x(new_x);
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

    set_shooter_angle(((double)aim * 120.0/1023) - 60.0);
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
            fire_cannon(get_shooter_angle());
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

	draw_all();
    show_screen();
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
        if(led_timer == 0.0){
            led_timer = get_elapsed_time();
        }
        left_LED_flash();
    }

    if(flash_right_led && !paused){
        //if the led is starting to flash start its timer
        if(led_timer == 0.0){
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

    //enable backlight
    TC4H = 1023 >> 8;
    OCR4C = 1023 & 0xff;
    TCCR4A = BIT(COM4A1) | BIT(PWM4A);
    SET_BIT(DDRC, 7);
    TCCR4B = BIT(CS42) | BIT(CS41) | BIT(CS40);
    TCCR4D = 0;
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

void setup( void ) {
    set_clock_speed(CPU_8MHz);

	enable_inputs();
    //	Initialise the LCD display using the default contrast setting.
    lcd_init(LCD_DEFAULT_CONTRAST);
    setup_timer();
    setup_usb_serial();
	draw_all();
}

int main(void) {
    srand(0);
    setup();
    intro_message();
    start_or_reset_game();
    srand(adc_read(0));
    while (1) {
        manage_loop();
        _delay_ms(100);
    }
    return 0;
}
