/*
 * project.c
 *
 * Main file
 *
 * Authors: Peter Sutton, Luke Kamols
 * Modified by <YOUR NAME HERE>
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>
// #include <pthread.h> 

#include "game.h"
#include "display.h"
#include "ledmatrix.h"
#include "buttons.h"
#include "serialio.h"
#include "terminalio.h"
#include "timer0.h"

#define F_CPU 8000000L
#include <util/delay.h>

// Function prototypes - these are defined below (after main()) in the order
// given here
void initialise_hardware(void);
void start_screen(void);
void new_game(void);
void play_game(void);
void handle_game_over(void);

/////////////////////////////// main //////////////////////////////////
int main(void) {
	// Setup hardware and call backs. This will turn on 
	// interrupts.
	initialise_hardware();
	
	// Show the splash screen message. Returns when display
	// is complete
	start_screen();
	
	// Loop forever,
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

void start_screen(void) {
	// Clear terminal screen and output a message
	clear_terminal();
	move_terminal_cursor(10,10);
	printf_P(PSTR("Reversi"));
	move_terminal_cursor(10,12);
	printf_P(PSTR("CSSE2010/7201 project by Yebai He s4546681"));
	
	// Output the static start screen and wait for a push button 
	// to be pushed or a serial input of 's'
	start_display();
	
	// Wait until a button is pressed, or 's' is pressed on the terminal
	while(1) {
		// First check for if a 's' is pressed
		// There are two steps to this
		// 1) collect any serial input (if available)
		// 2) check if the input is equal to the character 's'
		char serial_input = -1;
		if (serial_input_available()) {
			serial_input = fgetc(stdin);
		}
		// If the serial input is 's', then exit the start screen
		if (serial_input == 's' || serial_input == 'S') {
			break;
		}
		// Next check for any button presses
		int8_t btn = button_pushed();
		if (btn != NO_BUTTON_PUSHED) {
			break;
		}
	}
}

void new_game(void) {
	// Clear the serial terminal
	clear_terminal();

	// Initialise the game and display
	initialise_board();

	score_in_terminal();

	// Clear a button push or serial input if any are waiting
	// (The cast to void means the return value is ignored.)
	(void)button_pushed();
	clear_serial_input_buffer();
}

void play_game(void) {
	
	uint32_t last_flash_time, current_time, last_time_timed_game;
	uint8_t btn; //the button pushed

	uint32_t last_current_time, delta_time;
	last_current_time = 0;
	delta_time = 0;
	
	last_flash_time = get_current_time();
	last_time_timed_game = get_current_time();

	// uint32_t last_time_seven, current_time_seven;
	// last_time_seven = get_current_time();

	uint8_t is_game_pause = 0;
	uint8_t is_timed_game = 0;
	
	// We play the game until it's over
	while(!is_game_over()) {
		
		// We need to check if any button has been pushed, this will be
		// NO_BUTTON_PUSHED if no button has been pushed
		btn = button_pushed();
		
		if (btn == BUTTON2_PUSHED && is_game_pause == 0) {
			// If button 2 is pushed, move left, 
			// i.e decrease x by 1 and leave y the same
			move_display_cursor(-1, 0);
		}
		if (btn == BUTTON1_PUSHED && is_game_pause == 0) {
			move_display_cursor(0, 1);
		}
		// move with keyboard
		char serial_input = -1;
		if (serial_input_available()) {
			serial_input = fgetc(stdin);
		}
		if ((serial_input == 'w' || serial_input == 'W') && is_game_pause == 0) {
			move_display_cursor(0, 1);
		}
		if ((serial_input == 'a' || serial_input == 'A') && is_game_pause == 0) {
			move_display_cursor(-1, 0);
		}
		if ((serial_input == 's' || serial_input == 'S') && is_game_pause == 0) {
			move_display_cursor(0, -1);
		}
		if ((serial_input == 'd' || serial_input == 'D') && is_game_pause == 0) {
			move_display_cursor(1, 0);
		}

		// place a piece starting from red
		if ((btn == BUTTON0_PUSHED || serial_input == ' ') && is_game_pause == 0) {
			place_a_piece();
		}

		// switch between timed game and none timed game
		if ((serial_input == 't' || serial_input == 'T') && is_game_pause == 0) {
			if (is_timed_game == 0) {
				is_timed_game = 1;
			} else {
				is_timed_game = 0;
				cancel_timed_game();
			}
		}
		
		
		led_turn_display();
		if (is_game_pause == 0) {
			movement_control();
		}
		// if (is_timed_game == 1) {
		// 	turn_timing();
		// }
	
		current_time = get_current_time();
		// printf_P(PSTR("%d "), current_time);
		uint32_t temp_current_time = current_time - delta_time;
		if(temp_current_time >= last_flash_time + 500 && is_game_pause == 0) {
			// 500ms (0.5 second) has passed since the last time we
			// flashed the cursor, so flash the cursor
			flash_cursor();
			
			// Update the most recent time the cursor was flashed
			last_flash_time = temp_current_time;
		}
		if(current_time >= last_time_timed_game + 4) {
			if (is_timed_game == 1) {
				turn_timing();
			} else {
				score_in_seven_seg();
			}
			last_time_timed_game = current_time;
		}

		// game pause
		if (serial_input == 'p' || serial_input == 'P') {
			if (is_game_pause == 0) {
				game_pause();
				is_game_pause = 1;
				last_current_time = current_time;
			} else {
				game_pause();
				is_game_pause = 0;
				delta_time = get_current_time() - last_current_time;
			}
		}
	}
	// We get here if the game is over.
}

void handle_game_over() {
	move_terminal_cursor(10,14);
	printf_P(PSTR("GAME OVER"));
	move_terminal_cursor(10,15);
	printf_P(PSTR("Press a button to start again"));
	
	while(button_pushed() == NO_BUTTON_PUSHED) {
		; // wait
	}
	
}
