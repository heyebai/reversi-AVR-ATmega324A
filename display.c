/*
 * display.c
 *
 * Author: Luke Kamols
 */ 

#include <stdio.h>
#include <avr/pgmspace.h>

#include "display.h"
#include "pixel_colour.h"
#include "ledmatrix.h"

// constant value used to display 'RVRSI' on launch
static const uint8_t reversi_display[MATRIX_NUM_COLUMNS] = 
		{125, 81, 89, 117, 120, 4, 120, 125, 81, 89, 117, 116, 84, 84, 92, 93};

void initialise_display(void) {
	// start by clearing the LED matrix
	ledmatrix_clear();

	// create an array with the background colour at every position
	PixelColour col_colours[MATRIX_NUM_ROWS];
	for (int row = 0; row < MATRIX_NUM_ROWS; row++) {
		col_colours[row] = MATRIX_COLOUR_BG;
	}

	// then add the bounds on the left
	for (int x = 0; x < MATRIX_X_OFFSET; x++) {
		ledmatrix_update_column(x, col_colours);
	}

	// and add the bounds on the right
	for (int x = MATRIX_X_OFFSET + WIDTH; x < MATRIX_NUM_COLUMNS; x++) {
		ledmatrix_update_column(x, col_colours);
	}
}

void start_display(void) {
	PixelColour colour;
	MatrixColumn column_colour_data;
	uint8_t col_data;
		
	ledmatrix_clear(); // start by clearing the LED matrix
	for (uint8_t col = 0; col < MATRIX_NUM_COLUMNS; col++) {
		col_data = reversi_display[col];
		// using the LSB as the colour determining bit, 1 is red, 0 is green
		if (col_data & 0x01) {
			colour = COLOUR_RED;
		} else {
			colour = COLOUR_GREEN;
		}
		// go through the top 7 bits (not the bottom one as that was our colour bit)
		// and set any to be the correct colour
		for(uint8_t i=7; i>=1; i--) {
			// If the relevant font bit is set, we make this a coloured pixel, else blank
			if(col_data & 0x80) {
				column_colour_data[i] = colour;
				} else {
				column_colour_data[i] = 0;
			}
			col_data <<= 1;
		}
		column_colour_data[0] = 0;
		ledmatrix_update_column(col, column_colour_data);
	}
}

void update_square_colour(uint8_t x, uint8_t y, uint8_t object) {
	// determine which colour corresponds to this object
	PixelColour colour;
	if (object == PLAYER_1) {
		colour = MATRIX_COLOUR_P1;
		} else if (object == PLAYER_2) {
		colour = MATRIX_COLOUR_P2;
		} else if (object == CURSOR) {
		colour = MATRIX_COLOUR_CURSOR;
		} else if (object == INVALID_CURSOR) {
		colour = MATRIX_COLOUR_INVALID_CURSOR;	
		} else {
		// anything unexpected will be black
		colour = MATRIX_COLOUR_EMPTY;
	}

	// update the pixel at the given location with this colour
	// the board is offset on the x axis to be centred on the LED matrix
	ledmatrix_update_pixel(x + MATRIX_X_OFFSET, y, colour);
}