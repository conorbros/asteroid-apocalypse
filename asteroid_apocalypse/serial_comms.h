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

void send_usb_serial(char * msg);

int get_int(char * wait_msg);

void setup_usb_serial(void);