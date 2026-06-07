/**
 * @file
 * @author Hamik Mukelyan
 *
 * Drives a text-based Flappy Bird knock-off that is intended to run in an
 * 80 x 24 console. Migrated from ncurses to native Windows API.
 */

#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <limits.h>

// Key constants (equivalent to ncurses KEY_UP etc.)
#define KEY_UP    0x100
#define KEY_DOWN  0x101
#define KEY_LEFT  0x102
#define KEY_RIGHT 0x103

//-------------------------------- Definitions --------------------------------

/**
 * Represents a vertical pipe through which Flappy The Bird is supposed to fly.
 */
typedef struct vpipe {
	/*
	 * The height of the opening of the pipe as a fraction of the height of the
	 * console window.
	 */
	float opening_height;

	/*
	 * Center of the pipe is at this column number (e.g. somewhere in [0, 79]).
	 * When the center + radius is negative then the pipe's center is rolled
	 * over to somewhere > the number of columns and the opening height is
	 * changed.
	 */
	int center;
} vpipe;

/** Represents Flappy the Bird. */
typedef struct flappy {
	/* Height of Flappy the Bird at the last up arrow press. */
	int h0;

	/* Time since last up arrow pressed. */
	int t;
} flappy;

//------------------------------ Global Constants -----------------------------

/** Gravitational acceleration constant */
const float GRAV = 0.05;

/** Initial velocity with up arrow press */
const float V0 = -0.5;

/** Number of rows in the console window. */
const int NUM_ROWS = 24;

/** Number of columns in the console window. */
const int NUM_COLS = 80;

/** Radius of each vertical pipe. */
const int PIPE_RADIUS = 3;

/** Width of the opening in each pipe. */
const int OPENING_WIDTH = 7;

/** Flappy stays in this column. */
const int FLAPPY_COL = 10;

/** Aiming for this many frames per second. */
const float TARGET_FPS = 24;

/** Amount of time the splash screen stays up. */
const float START_TIME_SEC = 3;

/** Length of the "progress bar" on the status screen. */
const int PROG_BAR_LEN = 76;

/** Row number at which the progress bar will show. */
const int PROG_BAR_ROW = 22;

const int SCORE_START_COL = 62;

//------------------------------ Global Variables -----------------------------

/** Frame number. */
int frame = 0;

/** Number of pipes that have been passed. */
int score = 0;

/** Number of digits in the score. */
int sdigs = 1;

/** Best score so far. */
int best_score = 0;

/** Number of digits in the best score. */
int bdigs = 1;

/** The vertical pipe obstacles. */
vpipe p1, p2;

/** Windows console handle. */
HANDLE hConsole;

/** Original console mode for restoration. */
DWORD dwOriginalConsoleMode;

//---------------------------------- Windows Helpers --------------------------

/**
 * Initializes the Windows console for game rendering.
 */
void console_init(void) {
	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	GetConsoleMode(hConsole, &dwOriginalConsoleMode);

	// Hide cursor
	CONSOLE_CURSOR_INFO cursorInfo;
	GetConsoleCursorInfo(hConsole, &cursorInfo);
	cursorInfo.bVisible = FALSE;
	SetConsoleCursorInfo(hConsole, &cursorInfo);

	// Set console title
	SetConsoleTitleA("Flappy Bird - Windows");

	// Set console screen buffer size
	COORD bufferSize = { NUM_COLS + 1, NUM_ROWS + 1 };
	SetConsoleScreenBufferSize(hConsole, bufferSize);

	// Set console window size
	SMALL_RECT windowRect = { 0, 0, NUM_COLS, NUM_ROWS };
	SetConsoleWindowInfo(hConsole, TRUE, &windowRect);
	SetConsoleScreenBufferSize(hConsole, bufferSize);
}

/**
 * Restores the console to its original state.
 */
void console_cleanup(void) {
	// Show cursor
	CONSOLE_CURSOR_INFO cursorInfo;
	GetConsoleCursorInfo(hConsole, &cursorInfo);
	cursorInfo.bVisible = TRUE;
	SetConsoleCursorInfo(hConsole, &cursorInfo);

	// Restore original mode
	SetConsoleMode(hConsole, dwOriginalConsoleMode);
}

/**
 * Clears the console screen.
 */
void console_clear(void) {
	COORD topLeft = { 0, 0 };
	CONSOLE_SCREEN_BUFFER_INFO screen;
	DWORD written;

	GetConsoleScreenBufferInfo(hConsole, &screen);
	DWORD size = screen.dwSize.X * screen.dwSize.Y;

	FillConsoleOutputCharacterA(hConsole, ' ', size, topLeft, &written);
	FillConsoleOutputAttribute(hConsole, 0x07, size, topLeft, &written);
	SetConsoleCursorPosition(hConsole, topLeft);
}

/**
 * Sets the cursor position in the console.
 */
void gotoxy(int x, int y) {
	COORD pos = { (SHORT)x, (SHORT)y };
	SetConsoleCursorPosition(hConsole, pos);
}

/**
 * Prints a string at the specified position.
 */
void mvprintw(int row, int col, const char *str) {
	if (row < 0 || row >= NUM_ROWS || col < 0) return;

	COORD pos = { (SHORT)col, (SHORT)row };
	SetConsoleCursorPosition(hConsole, pos);

	DWORD written;
	int len = (int)strlen(str);
	if (col + len > NUM_COLS) {
		len = NUM_COLS - col;
	}
	if (len > 0) {
		WriteConsoleA(hConsole, str, len, &written, NULL);
	}
}

/**
 * Prints a single character at the specified position.
 */
void mvaddch(int row, int col, char ch) {
	COORD pos = { (SHORT)col, (SHORT)row };
	SetConsoleCursorPosition(hConsole, pos);
	DWORD written;
	WriteConsoleA(hConsole, &ch, 1, &written, NULL);
}

/**
 * Refreshes the console (no-op on Windows as we write directly).
 */
void refresh(void) {
	// On Windows, console updates are immediate, so this is a no-op.
}

/**
 * Non-blocking check if a key is available.
 */
int kbhit_wrapper(void) {
	return _kbhit();
}

/**
 * Gets a character from the console without blocking if timeout is 0.
 * Returns -1 if no key is pressed.
 */
int getch_nonblocking(void) {
	if (_kbhit()) {
		int ch = _getch();
		// Handle special keys (arrow keys return 0 or 0xE0 followed by scan code)
		if (ch == 0 || ch == 0xE0) {
			int ext = _getch();
			// Map arrow keys to values similar to ncurses KEY_UP
			if (ext == 72) return KEY_UP;  // UP arrow
			if (ext == 80) return KEY_DOWN;
			if (ext == 75) return KEY_LEFT;
			if (ext == 77) return KEY_RIGHT;
			return -1;
		}
		return ch;
	}
	return -1;
}

/**
 * Sleeps for the specified number of milliseconds.
 */
void sleep_ms(int ms) {
	Sleep(ms);
}

//---------------------------------- Functions --------------------------------

/**
 * Converts the given char into a string.
 */
void chtostr(char ch, char *str) {
	str[0] = ch;
	str[1] = '\0';
}

/**
 * "Moving" floor and ceiling are written into the window array.
 */
void draw_floor_and_ceiling(int ceiling_row, int floor_row,
		char ch, int spacing, int col_start) {
	char c[2];
	chtostr(ch, c);
	int i;
	for (i = col_start; i < NUM_COLS - 1; i += spacing) {
		if (i < SCORE_START_COL - sdigs - bdigs)
			mvprintw(ceiling_row, i, c);
		mvprintw(floor_row, i, c);
	}
}

/**
 * Updates the pipe center and opening height for each new frame.
 */
void pipe_refresh(vpipe *p) {
	if(p->center + PIPE_RADIUS < 0) {
		p->center = NUM_COLS + PIPE_RADIUS;
		p->opening_height = rand() / ((float) INT_MAX) * 0.5 + 0.25;
		score++;
		if(sdigs == 1 && score > 9)
			sdigs++;
		else if(sdigs == 2 && score > 99)
			sdigs++;
	}
	p->center--;
}

/**
 * Gets the row number of the top or bottom of the opening in the given pipe.
 */
int get_orow(vpipe p, int top) {
	return p.opening_height * (NUM_ROWS - 1) -
			(top ? 1 : -1) * OPENING_WIDTH / 2;
}

/**
 * Draws the given pipe on the console.
 */
void draw_pipe(vpipe p, char vch, char hcht, char hchb,
		int ceiling_row, int floor_row) {
	int i, upper_terminus, lower_terminus;
	char c[2];

	// Draw vertical part of upper half of pipe.
	for(i = ceiling_row + 1; i < get_orow(p, 1); i++) {
		if ((p.center - PIPE_RADIUS) >= 0 &&
				(p.center - PIPE_RADIUS) < NUM_COLS - 1) {
			chtostr(vch, c);
			mvprintw(i, p.center - PIPE_RADIUS, c);
		}
		if ((p.center + PIPE_RADIUS) >= 0 &&
				(p.center + PIPE_RADIUS) < NUM_COLS - 1) {
			chtostr(vch, c);
			mvprintw(i, p.center + PIPE_RADIUS, c);
		}
	}
	upper_terminus = i;

	// Draw horizontal part of upper part of pipe.
	for (i = -PIPE_RADIUS; i <= PIPE_RADIUS; i++) {
		if ((p.center + i) >= 0 &&
				(p.center + i) < NUM_COLS - 1) {
			chtostr(hcht, c);
			mvprintw(upper_terminus, p.center + i, c);
		}
	}

	// Draw vertical part of lower half of pipe.
	for(i = floor_row - 1; i > get_orow(p, 0); i--) {
		if ((p.center - PIPE_RADIUS) >= 0 &&
				(p.center - PIPE_RADIUS) < NUM_COLS - 1) {
			chtostr(vch, c);
			mvprintw(i, p.center - PIPE_RADIUS, c);
		}
		if ((p.center + PIPE_RADIUS) >= 0 &&
				(p.center + PIPE_RADIUS) < NUM_COLS - 1) {
			chtostr(vch, c);
			mvprintw(i, p.center + PIPE_RADIUS, c);
		}
	}
	lower_terminus = i;

	// Draw horizontal part of lower part of pipe.
	for (i = -PIPE_RADIUS; i <= PIPE_RADIUS; i++) {
		if ((p.center + i) >= 0 &&
				(p.center + i) < NUM_COLS - 1) {
			chtostr(hchb, c);
			mvprintw(lower_terminus, p.center + i, c);
		}
	}
}

/**
 * Get Flappy's height along its parabolic arc.
 */
int get_flappy_position(flappy f) {
	return f.h0 + V0 * f.t + 0.5 * GRAV * f.t * f.t;
}

/**
 * Returns true if Flappy crashed into a pipe.
 */
int crashed_into_pipe(flappy f, vpipe p) {
	if (FLAPPY_COL >= p.center - PIPE_RADIUS - 1 &&
			FLAPPY_COL <= p.center + PIPE_RADIUS + 1) {

		if (get_flappy_position(f) >= get_orow(p, 1)  + 1 &&
				get_flappy_position(f) <= get_orow(p, 0) - 1) {
			return 0;
		}
		else {
			return 1;
		}
	}
	return 0;
}

/**
 * Prints a failure screen asking the user to either play again or quit.
 */
int failure_screen(void) {
	char ch;
	console_clear();
	mvprintw(NUM_ROWS / 2 - 1, NUM_COLS / 2 - 22,
			"Flappy died :-(. <Enter> to flap, 'q' to quit.");
	refresh();

	// Block until user enters something
	while(1) {
		if (_kbhit()) {
			ch = _getch();
			if (ch == 0 || ch == 0xE0) {
				_getch(); // consume extended key
				continue;
			}
			break;
		}
		Sleep(10);
	}

	switch(ch) {
	case 'q':
	case 'Q':
		console_cleanup();
		exit(0);
		break;
	default:
		if (score > best_score)
			best_score = score;
		if (bdigs == 1 && best_score > 9)
			bdigs++;
		else if(bdigs == 2 && best_score > 99)
			bdigs++;
		score = 0;
		sdigs = 1;
		return 1;
	}
	console_cleanup();
	exit(0);
}

/**
 * Draws Flappy to the screen and shows death message if Flappy collides with
 * ceiling or floor.
 */
int draw_flappy(flappy f) {
	char c[2];
	int h = get_flappy_position(f);

	// If Flappy crashed into the ceiling or the floor...
	if (h <= 0 || h >= NUM_ROWS - 1)
		return failure_screen();

	// If Flappy crashed into a pipe...
	if (crashed_into_pipe(f, p1) || crashed_into_pipe(f, p2)) {
		return failure_screen();
	}

	// If going down, don't flap
	if (GRAV * f.t + V0 > 0) {
		chtostr('\\', c);
		mvprintw(h, FLAPPY_COL - 1, c);
		mvprintw(h - 1, FLAPPY_COL - 2, c);
		chtostr('0', c);
		mvprintw(h, FLAPPY_COL, c);
		chtostr('/', c);
		mvprintw(h, FLAPPY_COL + 1, c);
		mvprintw(h - 1, FLAPPY_COL + 2, c);
	}
	// If going up, flap!
	else {
		// Left wing
		if (frame % 6 < 3) {
			chtostr('/', c);
			mvprintw(h, FLAPPY_COL - 1, c);
			mvprintw(h + 1, FLAPPY_COL - 2, c);
		}
		else {
			chtostr('\\', c);
			mvprintw(h, FLAPPY_COL - 1, c);
			mvprintw(h - 1, FLAPPY_COL - 2, c);
		}

		// Body
		chtostr('0', c);
		mvprintw(h, FLAPPY_COL, c);

		// Right wing
		if (frame % 6 < 3) {
			chtostr('\\', c);
			mvprintw(h, FLAPPY_COL + 1, c);
			mvprintw(h + 1, FLAPPY_COL + 2, c);
		}
		else {
			chtostr('/', c);
			mvprintw(h, FLAPPY_COL + 1, c);
			mvprintw(h - 1, FLAPPY_COL + 2, c);
		}
	}

	return 0;
}

/**
 * Print a splash screen and show a progress bar.
 */
void splash_screen(void) {
	int i;
	int r = NUM_ROWS / 2 - 6;
	int c = NUM_COLS / 2 - 22;

	// Print the title.
	mvprintw(r, c,     " ___ _                       ___ _        _ ");
	mvprintw(r + 1, c, "| __| |__ _ _ __ _ __ _  _  | _ |_)_ _ __| |");
	mvprintw(r + 2, c, "| _|| / _` | '_ \\ '_ \\ || | | _ \\ | '_/ _` |");
	mvprintw(r + 3, c, "|_| |_\\__,_| .__/ .__/\\_, | |___/_|_| \\__,_|");
	mvprintw(r + 4, c, "           |_|  |_|   |__/                  ");
	mvprintw(NUM_ROWS / 2 + 1, NUM_COLS / 2 - 10,
			"Press <up> to flap!");

	// Print the progress bar.
	mvprintw(PROG_BAR_ROW, NUM_COLS / 2 - PROG_BAR_LEN / 2 - 1, "[");
	mvprintw(PROG_BAR_ROW, NUM_COLS / 2 + PROG_BAR_LEN / 2, "]");
	refresh();
	for(i = 0; i < PROG_BAR_LEN; i++) {
		sleep_ms((int)(1000.0 * START_TIME_SEC / (float) PROG_BAR_LEN));
		gotoxy(NUM_COLS / 2 - PROG_BAR_LEN / 2 + i, PROG_BAR_ROW);
		DWORD written;
		WriteConsoleA(hConsole, "=", 1, &written, NULL);
		refresh();
	}
	sleep_ms(500);
}

//------------------------------------ Main -----------------------------------

int main(void)
{
	int leave_loop = 0;
	int ch;
	flappy f;
	int restart = 1;

	srand((unsigned int)time(NULL));

	console_init();

	splash_screen();

	while(!leave_loop) {

		if (restart) {
			p1.center = (int)(1.2 * (NUM_COLS - 1));
			p1.opening_height = rand() / ((float) INT_MAX) * 0.5 + 0.25;
			p2.center = (int)(1.75 * (NUM_COLS - 1));
			p2.opening_height = rand() / ((float) INT_MAX) * 0.5 + 0.25;

			f.h0 = NUM_ROWS / 2;
			f.t = 0;
			restart = 0;
		}

		sleep_ms((int)(1000.0 / TARGET_FPS));

		ch = getch_nonblocking();
		if (ch != -1) {
			switch (ch) {
			case 'q':
			case 'Q':
				console_cleanup();
				exit(0);
				break;
			case KEY_UP:
				f.h0 = get_flappy_position(f);
				f.t = 0;
				break;
			default:
				f.t++;
			}
		} else {
			f.t++;
		}

		console_clear();

		draw_floor_and_ceiling(0, NUM_ROWS - 1, '/', 2, frame % 2);

		draw_pipe(p1, '|', '=', '=', 0, NUM_ROWS - 1);
		draw_pipe(p2, '|', '=', '=', 0, NUM_ROWS - 1);
		pipe_refresh(&p1);
		pipe_refresh(&p2);

		if(draw_flappy(f)) {
			restart = 1;
			continue;
		}

		char scoreStr[64];
		sprintf(scoreStr, " Score: %d  Best: %d", score, best_score);
		mvprintw(0, SCORE_START_COL - bdigs - sdigs, scoreStr);

		refresh();
		frame++;
	}

	console_cleanup();

	return 0;
}
