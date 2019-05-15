/*
** game.c
**
** Author: Peter Sutton
**
*/

#include "game.h"
#include "ledmatrix.h"
#include "pixel_colour.h"
#include <stdlib.h>
/* Stdlib needed for random() - random number generator */

///////////////////////////////////////////////////////////
// Colours
#define COLOUR_ASTEROID		COLOUR_GREEN
#define COLOUR_PROJECTILE	COLOUR_RED
#define COLOUR_BASE			COLOUR_YELLOW

///////////////////////////////////////////////////////////
// Game positions (x,y) where x is 0 to 7 and y is 0 to 15
// are represented in a single 8 bit unsigned integer where the most
// significant 4 bits are the x value and the least significant 4 bits
// are the y value. The following macros allow the extraction of x and y
// values from a combined position value and the construction of a combined 
// position value from separate x, y values. Values are assumed to be in
// valid ranges. Invalid positions are any where the least significant
// bit is 1 (i.e. x value greater than 7). We can use all 1's (255) to 
// represent this.
#define GAME_POSITION(x,y)		( ((x) << 4)|((y) & 0x0F) )
#define GET_X_POSITION(posn)	((posn) >> 4)
#define GET_Y_POSITION(posn)	((posn) & 0x0F)
#define INVALID_POSITION		255

///////////////////////////////////////////////////////////
// Macros to convert game position to LED matrix position
// Note that the row number (y value) in the game (0 to 15 from the bottom) 
// corresponds to x values on the LED matrix (0 to 15).
// Column numbers (x values) in the game (0 to 7 from the left) correspond
// to LED matrix y values rom 7 to 0
//
// Note that these macros result in two expressions that are comma separated - suitable
// as use for the first two arguments to ledmatrix_update_pixel().
#define LED_MATRIX_POSN_FROM_XY(gameX, gameY)		(gameY) , (7-(gameX))
#define LED_MATRIX_POSN_FROM_GAME_POSN(posn)		\
		LED_MATRIX_POSN_FROM_XY(GET_X_POSITION(posn), GET_Y_POSITION(posn))

///////////////////////////////////////////////////////////
// Global variables.
//
// basePosition - stores the x position of the centre point of the 
// base station. The base station is three positions wide, but is
// permitted to partially move off the game field so that the centre
// point can take on any position from 0 to 7 inclusive.
//
// numProjectiles - The number of projectiles currently in flight. Must
// be less than or equal to MAX_PROJECTILES.
//
// projectiles - x,y positions of the projectiles that are currently
// in flight. The upper 4 bits represent the x position; the lower 4
// bits represent the y position. The array is indexed by projectile
// number from 0 to numProjectiles - 1.
//
// numAsteroids - The number of asteroids currently on the game field.
// Must be less than or equal to MAX_ASTEROIDS.
//
// asteroids - x,y positions of the asteroids on the field. The upper
// 4 bits represent the x position; the lower 4 bits represent the 
// y position. The array is indexed by asteroid number from 0 to 
// numAsteroids - 1.

int8_t		basePosition;
int8_t		numProjectiles;
uint8_t		projectiles[MAX_PROJECTILES];
int8_t		numAsteroids;
uint8_t		asteroids[MAX_ASTEROIDS];

///////////////////////////////////////////////////////////
// Prototypes for internal information functions 
//  - not available outside this module.

// Is there is an asteroid/projectile at the given position?. 
// Returns -1 if no, asteroid/projectile index number if yes.
// (The index number is the array index in the asteroids/
// projectiles array above.)

static int8_t asteroid_at(uint8_t x, uint8_t y);
static int8_t projectile_at(uint8_t x, uint8_t y);

// Remove the asteroid/projectile at the given index number. If
// the index is not valid, then no removal is performed. This 
// enables the functions to be used like:
//		remove_asteroid(asteroid_at(x,y));
static void remove_asteroid(int8_t asteroidIndex);
static void remove_projectile(int8_t projectileIndex);

// Redraw functions
static void redraw_whole_display(void);
static void redraw_base(uint8_t colour);
static void redraw_all_asteroids(void);
static void redraw_asteroid(uint8_t asteroidNumber, uint8_t colour);
static void redraw_all_projectiles(void);
static void redraw_projectile(uint8_t projectileNumber, uint8_t colour);
///////////////////////////////////////////////////////////

 
// Initialise game field:
// (1) base starts in the centre (x=3)
// (2) no projectiles initially
// (3) the maximum number of asteroids, randomly distributed.
void initialise_game(void) {
	uint8_t x, y, i;
	
    basePosition = 3;
	numProjectiles = 0;
	numAsteroids = 0;

	for(i=0; i < MAX_ASTEROIDS ; i++) {
		// Generate random position that does not already
		// have an asteroid.
		do {
			// Generate random x position - somewhere from 0
			// to FIELD_WIDTH - 1
			x = (uint8_t)(random() % FIELD_WIDTH);
			// Generate random y position - somewhere from 3
			// to FIELD_HEIGHT - 1 (i.e., not in the lowest
			// three rows)
			y = (uint8_t)(3 + (random() % (FIELD_HEIGHT-3)));
		} while(asteroid_at(x,y) != -1);
		// If we get here, we've now found an x,y location without
		// an existing asteroid - record the position
		asteroids[i] = GAME_POSITION(x,y);
		numAsteroids++;
	}
	
	redraw_whole_display();
}

// Attempt to move the base station to the left or right. 
// The direction argument has the value MOVE_LEFT or
// MOVE_RIGHT. The move succeeds if the base isn't all 
// the way to one side, e.g., not permitted to move
// left if basePosition is already 0.
// Returns 1 if move successful, 0 otherwise.
int8_t move_base(int8_t direction) {	
	// The initial version of this function just moves
	// the base one position to the left, no matter where
	// the base station is now or what the direction argument
	// is. This may cause the base to move off the game field
	// (and eventually wrap around - e.g. subtracting 1 from
	// basePosition 256 times will eventually bring it back to
	// same value.
	
	// YOUR CODE HERE (AND BELOW) - FIX THIS FUNCTION
	
	// We erase the base from its current position first
	redraw_base(COLOUR_BLACK);
	
	// Move the base (only to the left at present)
	basePosition--;
	
	// Redraw the base
	redraw_base(COLOUR_BASE);
	
	return 1;
}

// Fire projectile - add it immediately above the base
// station, provided there is not already a projectile
// there. We are also limited in the number of projectiles
// we can have in flight (to MAX_PROJECTILES).
// Returns 1 if projectile fired, 0 otherwise.
int8_t fire_projectile(void) {
	uint8_t newProjectileNumber;
	if(numProjectiles < MAX_PROJECTILES && 
			projectile_at(basePosition, 2) == -1) {
		// Have space to add projectile - add it at the x position of
		// the base, in row 2(y=2)
		newProjectileNumber = numProjectiles++;
		projectiles[newProjectileNumber] = GAME_POSITION(basePosition, 2);
		redraw_projectile(newProjectileNumber, COLOUR_PROJECTILE);
		return 1;
	} else {
		return 0;
	}
}

// Move projectiles up by one position, and remove those that 
// have gone off the top or that hit an asteroid.
void advance_projectiles(void) {
	uint8_t x, y;
	int8_t projectileNumber;

	projectileNumber = 0;
	while(projectileNumber < numProjectiles) {
		// Get the current position of the projectile
		x = GET_X_POSITION(projectiles[projectileNumber]);
		y = GET_Y_POSITION(projectiles[projectileNumber]);
		
		// Work out the new position (but don't update the projectile 
		// location yet - we only do that if we know the move is valid)
		y = y+1;
		
		// Check if new position would be off the top of the display
		if(y == FIELD_HEIGHT) {
			// Yes - remove the projectile. (Note that we haven't updated
			// the position of the projectile itself - so the projectile 
			// will be removed from its old location.)
			remove_projectile(projectileNumber);
			// Note - we do not increment the projectileNumber here as
			// the remove_projectile() function moves the later projectiles
			// (if any) back down the list of projectiles so that
			// the projectileNumber is now the next projectile to be
			// dealt with (if we weren't at the last one in the list).
			// remove_projectile() will also result in numProjectiles being
			// decreased by 1
		} else {
			// Projectile is not going off the top of the display
			// CHECK HERE IF THE NEW PROJECTILE LOCATION CORRESPONDS TO
			// AN ASTEROID LOCATION. IF IT DOES, REMOVE THE PROJECTILE
			// AND THE ASTEROID.
			
			// OTHERWISE...
			
			// Remove the projectile from the display 
			redraw_projectile(projectileNumber, COLOUR_BLACK);
			
			// Update the projectile's position
			projectiles[projectileNumber] = GAME_POSITION(x,y);
			
			// Redraw the projectile
			redraw_projectile(projectileNumber, COLOUR_PROJECTILE);
			
			// Move on to the next projectile (we don't do this if a projectile
			// is removed since projectiles will be shuffled in the list and the
			// next projectile (if any) will take on the same projectile number)
			projectileNumber++;
		}			
	}
}

// Returns 1 if the game is over, 0 otherwise. Initially, the game is
// never over.
int8_t is_game_over(void) {
	return 0;
}


/******** INTERNAL FUNCTIONS ****************/

// Check whether there is an asteroid at a given position.
// Returns -1 if there is no asteroid, otherwise we return
// the asteroid number (from 0 to numAsteroids-1).
static int8_t asteroid_at(uint8_t x, uint8_t y) {
	uint8_t i;
	uint8_t positionToCheck = GAME_POSITION(x,y);
	for(i=0; i < numAsteroids; i++) {
		if(asteroids[i] == positionToCheck) {
			// Asteroid i is at the given position
			return i;
		}
	}
	// No match was found - no asteroid at the given position
	return -1;
}

// Check whether there is a projectile at a given position.
// Returns -1 if there is no projectile, otherwise we return
// the projectile number (from 0 to numProjectiles-1).
static int8_t projectile_at(uint8_t x, uint8_t y) {
	uint8_t i;
	uint8_t positionToCheck = GAME_POSITION(x,y);
	for(i=0; i < numProjectiles; i++) {
		if(projectiles[i] == positionToCheck) {
			// Projectile i is at the given position
			return i;
		}
	}
	// No match was found - no projectile at the given position 
	return -1;
}

/* Remove asteroid with the given index number (from 0 to
** numAsteroids - 1).
*/
static void remove_asteroid(int8_t asteroidNumber) {
	if(asteroidNumber < 0 || asteroidNumber >= numAsteroids) {
		// Invalid index - do nothing
		return;
	}
	
	// Remove the asteroid from the display
	redraw_asteroid(asteroidNumber, COLOUR_BLACK);
	
	if(asteroidNumber < numAsteroids - 1) {
		// Asteroid is not the last one in the list
		// - move the last one in the list to this position
		asteroids[asteroidNumber] = asteroids[numAsteroids - 1];
	}
	// Last position in asteroids array is no longer used
	numAsteroids--;
}

// Remove projectile with the given projectile number (from 0 to
// numProjectiles - 1).
static void remove_projectile(int8_t projectileNumber) {	
	if(projectileNumber < 0 || projectileNumber >= numProjectiles) {
		// Invalid index - do nothing 
		return;
	}
	
	// Remove the projectile from the display
	redraw_projectile(projectileNumber, COLOUR_BLACK);
	
	// Close up the gap in the list of projectiles - move any
	// projectiles after this in the list closer to the start of the list
	for(uint8_t i = projectileNumber+1; i < numProjectiles; i++) {
		projectiles[i-1] = projectiles[i];
	}
	// Update projectile count - have one fewer projectiles now.
	numProjectiles--;
}

// Redraw the whole display - base, asteroids and projectiles.
// We assume all of the data structures have been appropriately poplulated
static void redraw_whole_display(void) {
	// clear the display
	ledmatrix_clear();
	
	// Redraw each of the elements
	redraw_base(COLOUR_BASE);
	redraw_all_asteroids();	
	redraw_all_projectiles();
}

static void redraw_base(uint8_t colour){
	// Add the bottom row of the base first (0) followed by the single bit
	// in the next row (1)
	for(int8_t x = basePosition - 1; x <= basePosition+1; x++) {
		if (x >= 0 && x < FIELD_WIDTH) {
			ledmatrix_update_pixel(LED_MATRIX_POSN_FROM_XY(x, 0), colour);
		}
	}
	ledmatrix_update_pixel(LED_MATRIX_POSN_FROM_XY(basePosition, 1), colour);
}

static void redraw_all_asteroids(void) {
	// For each asteroid, determine it's position and redraw it
	for(uint8_t i=0; i < numAsteroids; i++) {
		redraw_asteroid(i, COLOUR_ASTEROID);
	}
}

static void redraw_asteroid(uint8_t asteroidNumber, uint8_t colour) {
	uint8_t asteroidPosn;
	if(asteroidNumber < numAsteroids) {
		asteroidPosn = asteroids[asteroidNumber];
		ledmatrix_update_pixel(LED_MATRIX_POSN_FROM_GAME_POSN(asteroidPosn), colour);
	}
}

static void redraw_all_projectiles(void){
	// For each projectile, determine its position and redraw it
	for(uint8_t i = 0; i < numProjectiles; i++) {
		redraw_projectile(i, COLOUR_PROJECTILE);
	}
}

static void redraw_projectile(uint8_t projectileNumber, uint8_t colour) {
	uint8_t projectilePosn;
	
	// Check projectileNumber is valid - ignore otherwise
	if(projectileNumber < numProjectiles) {
		projectilePosn = projectiles[projectileNumber];
		ledmatrix_update_pixel(LED_MATRIX_POSN_FROM_GAME_POSN(projectilePosn), colour);
	}
}

