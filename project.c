/*
 * project.c
 *
 * Main file
 *
 * Author: Peter Sutton. Modified by <YOUR NAME HERE>
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>

#include "ledmatrix.h"
#include "scrolling_char_display.h"
#include "buttons.h"
#include "serialio.h"
#include "terminalio.h"
#include "score.h"
#include "timer0.h"
#include "game.h"

#define F_CPU 8000000L
#include <util/delay.h>

// Function prototypes - these are defined below (after main()) in the order
// given here
void initialise_hardware(void);
void splash_screen(void);
void new_game(void);
void play_game(void);
void handle_game_over(void);

// ASCII code for Escape character
#define ESCAPE_CHAR 27

// List of SSD values from 0 to 9.
uint8_t seven_seg[10] = {63, 6, 91,79, 102, 109, 125, 7, 127, 111};

/////////////////////////////// main //////////////////////////////////
int main(void) {
	// Setup hardware and call backs. This will turn on 
	// interrupts.
	initialise_hardware();
	
	// Show the splash screen message. Returns when display
	// is complete
	splash_screen();
	
	while(1) {
		new_game();
		play_game();
		handle_game_over();
	}
}

void initialise_hardware(void) {
	ledmatrix_setup();
	init_button_interrupts();
	// Setup serial port for 19200 baud communication with no echo
	// of incoming characters
	init_serial_stdio(19200,0);
	
	init_timer0();
	
	// Turn on global interrupts
	sei();
}

void splash_screen(void) {
	// Clear terminal screen and output a message
	clear_terminal();
	move_cursor(10,10);
	printf_P(PSTR("Asteroids"));
	move_cursor(10,12);
	printf_P(PSTR("CSSE2010/7201 project by Tean-louise, 42637460"));
	
	// Output the scrolling message to the LED matrix
	// and wait for a push button to be pushed.
	ledmatrix_clear();
	while(1) {
		set_scrolling_display_text("ASTEROIDS by 42637460", COLOUR_GREEN);
		// Scroll the message until it has scrolled off the 
		// display or a button is pushed
		while(scroll_display()) {
			_delay_ms(150);
			if(button_pushed() != NO_BUTTON_PUSHED) {
				return;
			}
		}
	}
}

void new_game(void) {
	// Initialise the game and display
	initialise_game();
	
	// Clear the serial terminal and set score display parameters.
	clear_terminal();
	move_cursor(10,10);	
	// Initialise the score to 0.
	init_score();
	// Display starting score on terminal.
	printf_P(PSTR("Score:         0"));
	// Display starting score on SSD on the right digit.
	DDRA = 0xFF;
	DDRC = 1;
	PORTC = 0;
	PORTA = seven_seg[0];
		
	// Clear a button push or serial input if any are waiting
	// (The cast to void means the return value is ignored.)
	(void)button_pushed();
	clear_serial_input_buffer();
}

void play_game(void) {
	uint32_t current_time, last_move_time;
	int8_t button;
	char serial_input, escape_sequence_char;
	uint8_t characters_into_escape_sequence = 0;
	uint32_t old_score = 0;
	
	// Get the current time and remember this as the last time the projectiles
    // were moved.
	current_time = get_current_time();
	last_move_time = current_time;
	
	// We play the game until it's over
	while(!is_game_over()) {
		
		// Check for input - which could be a button push or serial input.
		// Serial input may be part of an escape sequence, e.g. ESC [ D
		// is a left cursor key press. At most one of the following three
		// variables will be set to a value other than -1 if input is available.
		// (We don't initalise button to -1 since button_pushed() will return -1
		// if no button pushes are waiting to be returned.)
		// Button pushes take priority over serial input. If there are both then
		// we'll retrieve the serial input the next time through this loop
		serial_input = -1;
		escape_sequence_char = -1;
		button = button_pushed();
		
		// Check if the score has changed. 
		// Only update if the score has changed. Display new score.
		if (get_score() != old_score) {
			old_score = get_score();
			clear_terminal();
			move_cursor(10,10);
			printf_P(PSTR("Score:%10u"), old_score);
		}
		
		// Display score on SSD. Score displayed starting on the right.
		PORTA = seven_seg[old_score % 10];
		PORTC = 0;	
		if (old_score > 9) {
			PORTA = seven_seg[old_score / 10];
			PORTC = 1;			
		}
				
		if(button == NO_BUTTON_PUSHED) {
			// No push button was pushed, see if there is any serial input
			if(serial_input_available()) {
				// Serial data was available - read the data from standard input
				serial_input = fgetc(stdin);
				// Check if the character is part of an escape sequence
				if(characters_into_escape_sequence == 0 && serial_input == ESCAPE_CHAR) {
					// We've hit the first character in an escape sequence (escape)
					characters_into_escape_sequence++;
					serial_input = -1; // Don't further process this character
				} else if(characters_into_escape_sequence == 1 && serial_input == '[') {
					// We've hit the second character in an escape sequence
					characters_into_escape_sequence++;
					serial_input = -1; // Don't further process this character
				} else if(characters_into_escape_sequence == 2) {
					// Third (and last) character in the escape sequence
					escape_sequence_char = serial_input;
					serial_input = -1;  // Don't further process this character - we
										// deal with it as part of the escape sequence
					characters_into_escape_sequence = 0;
				} else {
					// Character was not part of an escape sequence (or we received
					// an invalid second character in the sequence). We'll process 
					// the data in the serial_input variable.
					characters_into_escape_sequence = 0;
				}
			}
		}
		
		// Process the input. 
		if(button==3 || escape_sequence_char=='D' || serial_input=='L' || serial_input=='l') {
			// Button 3 pressed OR left cursor key escape sequence completed OR
			// letter L (lowercase or uppercase) pressed - attempt to move left
			move_base(MOVE_LEFT);
		} else if(button==2 || escape_sequence_char=='A' || serial_input==' ') {
			// Button 2 pressed or up cursor key escape sequence completed OR
			// space bar pressed - attempt to fire projectile
			fire_projectile();
		} else if(button==1 || escape_sequence_char=='B') {
			// Button 1 pressed OR down cursor key escape sequence completed
			// Ignore at present
		} else if(button==0 || escape_sequence_char=='C' || serial_input=='R' || serial_input=='r') {
			// Button 0 pressed OR right cursor key escape sequence completed OR
			// letter R (lowercase or uppercase) pressed - attempt to move right
			move_base(MOVE_RIGHT);
		} else if(serial_input == 'p' || serial_input == 'P') {
			// Unimplemented feature - pause/unpause the game until 'p' or 'P' is
			// pressed again
		} 
		// else - invalid input or we're part way through an escape sequence -
		// do nothing
		
		current_time = get_current_time();
		if(!is_game_over() && current_time >= last_move_time + 500) {
			// 500ms (0.5 second) has passed since the last time we moved
			// the projectiles - move them - and keep track of the time we 
			// moved them
			advance_projectiles();
			
			last_move_time = current_time;
		}
	}
	// We get here if the game is over.
}

void handle_game_over() {
	move_cursor(10,14);
	printf_P(PSTR("GAME OVER"));
	move_cursor(10,15);
	printf_P(PSTR("Press a button to start again"));
	while(button_pushed() == NO_BUTTON_PUSHED) {
		; // wait
	}
	
}
