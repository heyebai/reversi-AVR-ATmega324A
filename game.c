/*
 * game.c
 *
 * Contains functions relating to the play of the game Reversi
 *
 * Author: Luke Kamols
 */ 
#include <stdio.h>
#include <stdint.h>

#include <avr/pgmspace.h>
#include <avr/io.h>

#include "game.h"
#include "display.h"
#include "terminalio.h"
#include "timer0.h"



// #include "buttons.h"
// #include "serialio.h"

#define CURSOR_X_START 5
#define CURSOR_Y_START 3

#define START_PIECES 2
static const uint8_t p1_start_pieces[START_PIECES][2] = { {3, 3}, {4, 4} };
static const uint8_t p2_start_pieces[START_PIECES][2] = { {3, 4}, {4, 3}}; // 


uint8_t board[WIDTH][HEIGHT];
uint8_t cursor_x;
uint8_t cursor_y;
uint8_t cursor_visible;
uint8_t current_player;

uint32_t last_flash_time, current_time;

void initialise_board(void) {
	
	// initialise the display we are using
	initialise_display();
	
	// initialise the board to be all empty
	for (uint8_t x = 0; x < WIDTH; x++) {
		for (uint8_t y = 0; y < HEIGHT; y++) {
			board[x][y] = EMPTY_SQUARE;
		}
	}
	
	// now load in the starting pieces for player 1
	for (uint8_t i = 0; i < START_PIECES; i++) {
		uint8_t x = p1_start_pieces[i][0];
		uint8_t y = p1_start_pieces[i][1];
		board[x][y] = PLAYER_1; // place in array
		update_square_colour(x, y, PLAYER_1); // show on board
	}
	
	// and for player 2
	for (uint8_t i = 0; i < START_PIECES; i++) {
		uint8_t x = p2_start_pieces[i][0];
		uint8_t y = p2_start_pieces[i][1];
		board[x][y] = PLAYER_2;
		update_square_colour(x, y, PLAYER_2);		
	}
	
	// set the starting player
	current_player = PLAYER_1;
	
	// also set where the cursor starts
	cursor_x = CURSOR_X_START;
	cursor_y = CURSOR_Y_START;
	cursor_visible = 0;

	last_flash_time = get_current_time();

	// joystick
	ADMUX = (1<<REFS0);
	ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1);
}


// joystick
uint8_t up_flag = 0;  //0: do not move 1:move 2: have moved
uint8_t down_flag = 0;
uint8_t left_flag = 0;
uint8_t right_flag = 0;
// uint8_t call_movement_helper_times = 0;
int run_times_x = 0;
int run_times_y = 0;
void movement_helper(void) {
	if (up_flag == 1 && right_flag == 1) {
		move_display_cursor(1, 1);
		up_flag = 2;
		right_flag = 2;
	} else if (up_flag == 1 && left_flag == 1) {
		move_display_cursor(-1, 1);
		up_flag = 2;
		left_flag = 2;
	} else if (down_flag == 1 && right_flag == 1) {
		move_display_cursor(1, -1);
		down_flag = 2;
		right_flag = 2;
	} else if (down_flag == 1 && left_flag == 1) {
		move_display_cursor(-1, -1);
		down_flag = 2;
		left_flag = 2;
	} else if (up_flag == 1) {
		move_display_cursor(0, 1);
		up_flag = 2;
	} else if (down_flag == 1) {
		move_display_cursor(0, -1);
		down_flag = 2;
	} else if (right_flag == 1) {
		move_display_cursor(1, 0);
		right_flag = 2;
	} else if (left_flag == 1) {
		move_display_cursor(-1, 0);
		left_flag = 2;
	}
}

uint8_t x_or_y = 0;	/* 0 = x, 1 = y */
void movement_control(void) {
	uint16_t value;
	
	if(x_or_y == 0) {
		ADMUX &= ~1;
	} else {
		ADMUX |= 1;
	}
	// Start the ADC conversion
	ADCSRA |= (1<<ADSC);
	
	while(ADCSRA & (1<<ADSC)) {
		; /* Wait until conversion finished */
	}
	value = ADC; // read the value
	// up right >850 >850 left down <300 <300 left up <200 >850 right down >700 <300
	if(x_or_y == 0) {
		if (value > 800) {
			run_times_x++;
			if (run_times_x == 1) {
				right_flag = 1;
			}
		} else if (value < 300) {
			run_times_x++;
			if (run_times_x == 1) {
				left_flag = 1;
			}
		} else if(value > 412 && value < 612) {       
			right_flag = 0;
			left_flag = 0;
			run_times_x = 0;
		}
	} else {
		if (value > 800) {
			run_times_y++;
			if (run_times_y == 1) {
				up_flag = 1;
			}
		} else if (value < 300) {
			run_times_y++;
			if (run_times_y == 1) {
				down_flag = 1;
			}
		} else if(value > 412 && value < 612) {    
			up_flag = 0;
			down_flag = 0;
			run_times_y = 0;
		}
	}
	movement_helper();
	// Next time through the loop, do the other direction
	x_or_y ^= 1;
}




uint8_t get_piece_at(uint8_t x, uint8_t y) {
	// check the bounds, anything outside the bounds
	// will be considered empty
	if (x < 0 || x >= WIDTH || y < 0 || y >= WIDTH) {
		return EMPTY_SQUARE;
	} else {
		//if in the bounds, just index into the array
		return board[x][y];
	}
}



//0:do not need to loop,1:need to loop,2:loop to change color
uint8_t need_to_check[8];

uint8_t is_valid_position(uint8_t px, uint8_t py) {
	
	uint8_t cursor_surrounding[8][2] = {
		{px - 1, py + 1}, {px, py + 1}, {px + 1, py + 1}, 
		{px + 1, py}, {px + 1, py - 1}, {px, py - 1}, 
		{px - 1, py - 1}, {px - 1, py}
	};
	if (board[px][py] != EMPTY_SQUARE) {
		return 0;
	} else {
		uint8_t opponent;
		if (current_player == PLAYER_1) {
			opponent = PLAYER_2;
		} else {
			opponent = PLAYER_1;
		}
		// init the need to check list
		for (uint8_t i = 0; i < 8; i++) {
			need_to_check[i] = 0;
		}
		// check whether there is a opponent in the surrounding
		for (uint8_t i = 0; i < 8; i++) {
			uint8_t x = cursor_surrounding[i][0];
			uint8_t y = cursor_surrounding[i][1];
			uint8_t obj = get_piece_at(x, y);
			if (obj == opponent) {
				need_to_check[i] = 1;
			}
		}
		// check whether there is a piece in the current player union in the line
		uint8_t valid_position_flag = 0;
		for (uint8_t i = 0; i < 8; i++) {
			if (need_to_check[i] == 1) {
				uint8_t x = px;
				uint8_t y = py;
				if (i == 0) {
					do {
						x -= 1;
						y += 1;
						uint8_t obj = get_piece_at(x, y);
						if (obj == EMPTY_SQUARE) {
							break;
						} else if (obj == current_player) {
							need_to_check[i] = 2;
							valid_position_flag = 1;
						} 
					} while (x >= 0 && y <= 7); 

				} else if (i == 1) {
					do {
						y += 1;
						uint8_t obj = get_piece_at(x, y);
						if (obj == EMPTY_SQUARE) {
							break;
						} else if (obj == current_player) {
							need_to_check[i] = 2;
							valid_position_flag = 1;
						} 
					} while (y <= 7);

				} else if (i == 2) {
					do {
						x += 1;
						y += 1;
						uint8_t obj = get_piece_at(x, y);
						if (obj == EMPTY_SQUARE) {
							break;
						} else if (obj == current_player) {
							need_to_check[i] = 2;
							valid_position_flag = 1;
						} 
					} while (x <= 7 && y <= 7);

				} else if (i == 3) {
					do {
						x += 1;
						uint8_t obj = get_piece_at(x, y);
						if (obj == EMPTY_SQUARE) {
							break;
						} else if (obj == current_player) {
							need_to_check[i] = 2;
							valid_position_flag = 1;
						} 
					} while (x <= 7);

				} else if (i == 4) {
					do {
						x += 1;
						y -= 1;
						uint8_t obj = get_piece_at(x, y);
						if (obj == EMPTY_SQUARE) {
							break;
						} else if (obj == current_player) {
							need_to_check[i] = 2;
							valid_position_flag = 1;
						} 
					} while (x <= 7 && y >= 0);

				} else if (i == 5) {
					do {
						y -= 1;
						uint8_t obj = get_piece_at(x, y);
						if (obj == EMPTY_SQUARE) {
							break;
						} else if (obj == current_player) {
							need_to_check[i] = 2;
							valid_position_flag = 1;
						} 
					} while (y >= 0);

				} else if (i == 6) {
					do {
						x -= 1;
						y -= 1;
						uint8_t obj = get_piece_at(x, y);
						if (obj == EMPTY_SQUARE) {
							break;
						} else if (obj == current_player) {
							need_to_check[i] = 2;
							valid_position_flag = 1;
						} 
					} while (x >= 0 && y >= 0);

				} else if (i == 7) {
					do {
						x -= 1;
						uint8_t obj = get_piece_at(x, y);
						if (obj == EMPTY_SQUARE) {
							break;
						} else if (obj == current_player) {
							need_to_check[i] = 2;
							valid_position_flag = 1;
						} 
					} while (x >= 0);

				}
			}
		}
		return valid_position_flag;
	}
	return 0;
}

uint8_t valid_position_flag; 
void flash_cursor(void) {
	valid_position_flag = is_valid_position(cursor_x, cursor_y);
	if (cursor_visible) {
		// we need to flash the cursor off, it should be replaced by
		// the colour of the piece which is at that location
		uint8_t piece_at_cursor = get_piece_at(cursor_x, cursor_y);
		update_square_colour(cursor_x, cursor_y, piece_at_cursor);
		
	} else {
		// we need to flash the cursor on
		if (valid_position_flag == 1) {
			update_square_colour(cursor_x, cursor_y, CURSOR);
		} else {
			update_square_colour(cursor_x, cursor_y, INVALID_CURSOR);
		}
	}
	cursor_visible = 1 - cursor_visible; //alternate between 0 and 1
}





//check the header file game.h for a description of what this function should do
// (it may contain some hints as to how to move the pieces)
void move_display_cursor(uint8_t dx, uint8_t dy) {
	//YOUR CODE HERE
	uint8_t piece_at_cursor = get_piece_at(cursor_x, cursor_y);
	update_square_colour(cursor_x, cursor_y, piece_at_cursor);

	cursor_x = (cursor_x + dx) % WIDTH;
	cursor_y = (cursor_y + dy) % HEIGHT;
	cursor_visible = 0;

	
	/*suggestions for implementation:
	 * 1: remove the display of the cursor at the current location
	 *		(and replace it with whatever piece is at that location)
	 * 2: update the positional knowledge of the cursor, this will include
	 *		variables cursor_x, cursor_y and cursor_visible
	 * 3: display the cursor at the new location
	 */
}


uint8_t game_over = 0;
void set_game_over(void) {
	game_over = 1;
}

uint8_t test_valid_position(void) {
	for (uint8_t i = 0; i < 8; i++) {
		
		for (uint8_t j = 0; j < 8; j++) {
			uint8_t valid_position = is_valid_position(i, j);
			if (valid_position == 1) {
				return 1;
			}
		}
	}
	
	if (current_player == PLAYER_1) {
		current_player = PLAYER_2;
		return 0;
	} else {
		current_player = PLAYER_1;
		return 0;
	}
}
uint8_t turn_timing_flag = 0; // for turning timing
void place_a_piece(void) {
	if (valid_position_flag == 1) {
		board[cursor_x][cursor_y] = current_player;
		update_square_colour(cursor_x, cursor_y, current_player);
		for (uint8_t i = 0; i < 8; i++) {
			if (need_to_check[i] == 2) {
				uint8_t x = cursor_x;
				uint8_t y = cursor_y;
				if (i == 0) {
					do {
						x -= 1;
						y += 1;
						uint8_t obj = get_piece_at(x, y);
						if (obj == current_player) {
							break;
						} 
						board[x][y] = current_player;
						update_square_colour(x, y, current_player);
					} while (x >= 0 && y <= 7); 

				} else if (i == 1) {
					do {
						y += 1;
						uint8_t obj = get_piece_at(x, y);
						if (obj == current_player) {
							break;
						} 
						board[x][y] = current_player;
						update_square_colour(x, y, current_player);
					} while (y <= 7);

				} else if (i == 2) {
					do {
						x += 1;
						y += 1;
						uint8_t obj = get_piece_at(x, y);
						if (obj == current_player) {
							break;
						} 
						board[x][y] = current_player;
						update_square_colour(x, y, current_player);
					} while (x <= 7 && y <= 7);

				} else if (i == 3) {
					do {
						x += 1;
						uint8_t obj = get_piece_at(x, y);
						if (obj == current_player) {
							break;
						} 
						board[x][y] = current_player;
						update_square_colour(x, y, current_player);
					} while (x <= 7);

				} else if (i == 4) {
					do {
						x += 1;
						y -= 1;
						uint8_t obj = get_piece_at(x, y);
						if (obj == current_player) {
							break;
						} 
						board[x][y] = current_player;
						update_square_colour(x, y, current_player);
					} while (x <= 7 && y >= 0);

				} else if (i == 5) {
					do {
						y -= 1;
						uint8_t obj = get_piece_at(x, y);
						if (obj == current_player) {
							break;
						} 
						board[x][y] = current_player;
						update_square_colour(x, y, current_player);
					} while (y >= 0);

				} else if (i == 6) {
					do {
						x -= 1;
						y -= 1;
						uint8_t obj = get_piece_at(x, y);
						if (obj == current_player) {
							break;
						} 
						board[x][y] = current_player;
						update_square_colour(x, y, current_player);
					} while (x >= 0 && y >= 0);

				} else if (i == 7) {
					do {
						x -= 1;
						uint8_t obj = get_piece_at(x, y);
						if (obj == current_player) {
							break;
						} 
						board[x][y] = current_player;
						update_square_colour(x, y, current_player);
					} while (x >= 0);

				}
			}
		}
		if (current_player == PLAYER_1) {
			current_player = PLAYER_2;
			if (turn_timing_flag == 1) {
				cancel_timed_game();
				turn_timing();
			}
		} else {
			current_player = PLAYER_1;
			if (turn_timing_flag == 1) {
				cancel_timed_game();
				turn_timing();
			}
		}
		score_in_terminal();
		uint8_t test_next_player = test_valid_position();
		if (test_next_player == 0) {
			test_next_player = test_valid_position();
			if (test_next_player == 0) {
				set_game_over();
			}
		}
	}
	
}



void led_turn_display(void) {
	DDRA |= (1 << PINA3) | (1 << PINA4);                //DDRC |= (1 << PINC1) | (1 << PINC2);
	if (current_player == PLAYER_1) {
		PORTA &= ~((1 << PINA3) | (1 << PINA4));				//PORTC &= 11111001;
		PORTA |= (1 << PINA3);		//PORTC |= (1 << PINC1);

	} else {
		PORTA &= ~((1 << PINA3) | (1 << PINA4));	//PORTC &= 11111001;
		PORTA |= (1 << PINA4);	//PORTC |= (1 << PINC2);

	}
}

uint8_t red_score, green_score;

uint8_t game_over_flag = 0;

void score_in_terminal(void) {
	if (game_over_flag == 0) {
		red_score = 0;
		green_score = 0;
		for (uint8_t i = 0; i < WIDTH; i++) {
			for (uint8_t j = 0; j < HEIGHT; j++) {
				if (board[i][j] == PLAYER_1) {
					red_score += 1;
				} else if (board[i][j] == PLAYER_2) {
					green_score += 1;
				}
			}
		}
	}
	
	move_terminal_cursor(10,10);
	printf_P(PSTR("Red Score: "));
	move_terminal_cursor(35,10);
	printf_P(PSTR("%10d"), red_score);

	move_terminal_cursor(10,12);
	printf_P(PSTR("Green Score: "));
	move_terminal_cursor(35,12);
	printf_P(PSTR("%10d"), green_score);
}


uint8_t call_times = 0;
uint8_t seven_seg[10] = {63,6,91,79,102,109,125,7,127,111};
void score_in_seven_seg(void) {
	/* Set port A pins to be outputs*/
	if (turn_timing_flag == 0) {
		DDRC = 0xFF;                  //DDRA = 0xFF;
		DDRA |= (1 << 2);     //DDRC |= (1 << 0);
		uint8_t score = 0;
		if (current_player == PLAYER_1) {
			score = red_score;
		} else {
			score = green_score;
		}
		if (score < 10) {
			PORTC = seven_seg[score];               //PORTA = seven_seg[score];
			PORTA &= ~(1 << 2);                //PORTC &= 11111110;
		} else {
			// current_time = get_current_time();
			// if(current_time <= last_flash_time + 4) {
			// 	PORTA &= ~(1 << 2);           //PORTC &= 11111110;           //right
			// 	PORTC = seven_seg[score % 10];      //PORTA = seven_seg[score % 10];  //else
			// } else if (current_time <= last_flash_time + 8) {
			// 	PORTA |= (1 << PINA2);     //PORTC |= (1 << PINC0);           //left
			// 	PORTC = seven_seg[score / 10];  //0
			// } 
			if(call_times % 2 == 1) {
				PORTA &= ~(1 << 2);           //right
				PORTC = seven_seg[score % 10];  //else
			} else if (call_times % 2 == 0) {
				PORTA |= (1 << PINA2);           //left
				PORTC = seven_seg[score / 10];  //0
			} 
			// if (current_time >= last_flash_time + 8) {
			// 	last_flash_time = current_time;
			// }
			call_times++;
		}
	}

}	

uint8_t is_game_pause = 0;
void game_pause(void) {
	if (is_game_pause == 1) {
		is_game_pause = 0;
	} else {
		is_game_pause = 1;
	}
}


uint8_t time_count;
uint8_t change_side_flag = 0;
uint32_t last_time;
// uint32_t current_time_tt, current_time_l;
void turn_timing(void) {
	if (turn_timing_flag == 0) {
		last_time = get_current_time();
		time_count = 30;
		turn_timing_flag = 1;
	}
	DDRC = 0xFF;      //DDRA = 0xFF;
	DDRA |= (1 << PINA2);        //DDRC |= (1 << PINC0);
	// seven seg display
	if (time_count < 10) {
		PORTA &= ~(1 << PINA2);      //PORTC &= 11111110;           //right
		PORTC = seven_seg[time_count % 10];     //PORTA = seven_seg[time_count % 10];  //else
	} else {
		if(change_side_flag % 2 == 0) {
		PORTA &= ~(1 << PINA2);          //PORTC &= 11111110;           //right
		PORTC = seven_seg[time_count % 10];  //else

		} else {
			PORTA |= (1 << PINA2);          //PORTC |= (1 << PINC0);           //left
			PORTC = seven_seg[time_count / 10];  //0
		} 
	}

	if(time_count == 0) {
		game_over_flag = 1;
		if (current_player == PLAYER_1) {
			red_score = 0;
			green_score = 64;
		} else {
			red_score = 64;
			green_score = 0;
		}
		score_in_terminal();
		set_game_over();
		// game_over = 1;
	}
	// time count
	uint32_t current_time_l = get_current_time();
	if (current_time_l >= last_time + 1000 && time_count != 0) {
		if (is_game_pause == 0) {
			time_count -= 1;
		}
		last_time += 1000;
	}
	change_side_flag += 1;
}

void cancel_timed_game(void) {
	turn_timing_flag = 0;
	change_side_flag = 0;
}

//joystick



uint8_t is_game_over(void) {
	// The game ends when every single square is filled
	// Check for any squares that are empty
	// uint8_t has_a_valid_position = 0;
	if (game_over == 1) {
		return 1;
	}
	// if (player1_has_position == 1 && player2_has_position == 1) {
	// 	return 1;
	// }
	for (uint8_t x = 0; x < WIDTH; x++) {
		for (uint8_t y = 0; y < HEIGHT; y++) {
			// uint8_t valid_position = is_valid_position(x, y);
			// printf_P(PSTR("%d "), valid_position);

			// if (valid_position == 1) {
			// 	return 0;
			// }
			if (board[x][y] == EMPTY_SQUARE) {
				// there was an empty square, game is not over
				return 0;
			}
		}
	}
	
	// every single position has been checked and no empty squares were found
	// the game is over
	return 1;
}
