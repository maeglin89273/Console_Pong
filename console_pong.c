#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include <curses.h>
#include <pthread.h>
#include <unistd.h>

//these are structures of objects
typedef struct {
	int width, length;
	int centerX;
	int leftScore, rightScore;
	int padding;
} Court;

typedef struct {
	int size;
	int x;
	int topY;
	float speed;
	time_t lastMoveTimestamp;
} Paddle;

typedef struct {
	float x, y;
	float nextX, nextY;
	float vX, vY;
	int icon;
} Ball;

typedef struct {
	char leftPlayerUp, leftPlayerDown;
	char rightPlayerUp, rightPlayerDown;
	char speedUp, speedDown;
	char quit;
} KeySettings;

typedef struct {
	useconds_t upperBound, lowerBound;
	useconds_t value;
	useconds_t delta;
} RefreshRate;

//do some settings to the console/window
void initializeWindow(int *width, int *length);

//about creating and destorying objects
void newObjects(int windowdWidth, int windowLength);
KeySettings * newKeySettings();
RefreshRate * newRefreshRate();
Court * newCourt(int width, int length);
Paddle * newPaddle(int centerX, int centerY, int size);
Ball * newBall(int x, int y);
void releaseDynamicObjects();

//about threading
void spawnThreads(pthread_t *updateThread, pthread_t *drawThread);
void * looper(void *routine);

//about pressing keys
void readInput(KeySettings *keySettings);
void movePaddle(Paddle* paddle, int deltaY);
void adjustRefreshRate(RefreshRate *refreshRate, int coefficient);

//about updating objects
void update();
void setNextPositionOfBall(Ball* ball);
void moveBall(Ball* ball);
void placeBall(int x, int y, Ball* ball);
void detactPaddleAndRebound(Paddle *paddle, Ball* ball);
bool paddleDetaction(Paddle *paddle, Ball* ball);
bool paddleXDetection(Paddle *paddle, Ball* ball);
bool paddleYDetection(Paddle *paddle, Ball* ball);
void reboundOnPaddle(Paddle *paddle, Ball* ball);
float limitAcceration(float potentialAcceration);
int detactBoundaryAndRebound(Court* court, Ball* ball);
void updateScores(Court *court, int pointGet);

// about drawing
void draw();
void paintBallSpeed(Ball *ball);
void drawCourt(Court *court);
void drawBoundary(Court *court);
void showScores(Court *court);
int leftStartPosition(int score);
void drawPaddle(Paddle* paddle);
void drawBall(Ball *ball);

//global objects and threads
RefreshRate *refreshRate;
KeySettings *keySettings;
Paddle *paddleA, *paddleB;
Ball* ball;
Court* court;

pthread_t updateThread, drawThread;

bool isQuit = false; //the signal when quitting the game

// some constants for physical effects
const float DURATION_OF_ENERGY_PRESERVED = 0.5;
const float PASSED_ENEGRY_COEF = 0.3;
const float ACCERATION_THRESHOLD = 1.5;

int main() {
	int width, length;
	
	initializeWindow(&width, &length);
	newObjects(width, length);
	spawnThreads(&updateThread, &drawThread);

	readInput(keySettings);

	endwin();
	releaseDynamicObjects();
}

void initializeWindow(int *width, int *length) {
	initscr();
	cbreak();
	curs_set(false);
	noecho();
	
	getmaxyx(stdscr, *width, *length);
}

void newObjects(int windowWidth, int windowLength) {
	keySettings = newKeySettings();
	refreshRate = newRefreshRate();
	court = newCourt(windowWidth, windowLength);
	paddleA = newPaddle(court->padding, windowWidth / 2, 6);
	paddleB = newPaddle(windowLength - 1 - court->padding, windowWidth / 2, 6);
	ball = newBall(windowLength / 2, windowWidth / 2);
}

KeySettings * newKeySettings() {
	KeySettings * newKeySettings = (KeySettings *)malloc(sizeof(KeySettings));
	newKeySettings->leftPlayerUp = 'a';
	newKeySettings->leftPlayerDown = 'z';
	newKeySettings->rightPlayerUp = 'k';
	newKeySettings->rightPlayerDown = 'm';

	newKeySettings->speedUp = 't';
	newKeySettings->speedDown = 'r';

	newKeySettings->quit = 'q';
	return newKeySettings;
}

RefreshRate * newRefreshRate() {
	RefreshRate * newRefreshRate = (RefreshRate *)malloc(sizeof(RefreshRate));
	newRefreshRate->upperBound = 200000;
	newRefreshRate->lowerBound = 5000;
	newRefreshRate->value = 90000;
	newRefreshRate->delta = 5000;

	return newRefreshRate;
}

Court * newCourt(int width, int length) {
	Court * newCourt = (Court *)malloc(sizeof(Court));
	newCourt->width = width;
	newCourt->length = length;

	newCourt->centerX = length / 2;

	newCourt->leftScore = 0;
	newCourt->rightScore = 0;

	newCourt->padding = 3;
	return newCourt;
}

Paddle * newPaddle(int centerX, int centerY, int size) {
	Paddle * newPaddle = (Paddle *)malloc(sizeof(Paddle));
	newPaddle->size = size;

	newPaddle->x = centerX;
	newPaddle->topY = centerY - newPaddle->size / 2;
	newPaddle->lastMoveTimestamp = clock();

	return newPaddle;
}

Ball * newBall(int x, int y) {
	Ball * newBall = (Ball *)malloc(sizeof(Ball));
	newBall->icon = ACS_DIAMOND;
	srand(time(NULL));
	placeBall(x, y, newBall);

	return newBall;
}

void releaseDynamicObjects() {
	free(keySettings);
	free(refreshRate);
	free(paddleA);
	free(paddleB);
	free(ball);
	free(court);
}

void spawnThreads(pthread_t *updateThread, pthread_t *drawThread) {
	isQuit = pthread_create(updateThread, NULL, looper, (void *)update);
	if (isQuit) {
		printf("cannot create new thread\n");
		return;
	}

	isQuit = pthread_create(drawThread, NULL, looper, (void *)draw);
	if (isQuit) {
		printf("cannot create new thread\n");
	}
}

//for looping update and drawing routines
void * looper(void *routine) {
	void (*routineFunc)()  = (void (*)())routine;
	for (;!isQuit; usleep(refreshRate->value)) {
		routineFunc();
	}
}

void readInput(KeySettings *keySettings) {
	int key;
	for (;!isQuit;) {
		key = getch();
		//we don't use switch case, because key settings are not constants
		if (key == keySettings->leftPlayerUp) {
			movePaddle(paddleA, -1);
		} else if (key == keySettings->leftPlayerDown) {
			movePaddle(paddleA, 1);
		} else if (key == keySettings->rightPlayerUp) {
			movePaddle(paddleB, -1);
		} else if (key == keySettings->rightPlayerDown) {
			movePaddle(paddleB, 1);
		} else if (key == keySettings->speedUp) {
			adjustRefreshRate(refreshRate, -1);
		} else if (key == keySettings->speedDown) {
			adjustRefreshRate(refreshRate, 1);
		} else if (key == keySettings->quit) {
			isQuit = true;
		}
	}
}

void movePaddle(Paddle* paddle, int deltaY) {
	int nextY = paddle->topY + deltaY;
	if(nextY <= 0 || nextY + paddle->size >= court->width) { // if out of the court boundary
		return;
	}

	paddle->topY = nextY;

	// update moving speed and timestamp
	time_t timestamp = time(NULL);
	paddle->speed = deltaY / difftime(timestamp, paddle->lastMoveTimestamp);
	paddle->lastMoveTimestamp = timestamp;
}

void adjustRefreshRate(RefreshRate *refreshRate, int coefficient) {
	useconds_t potentialRate = refreshRate->value + coefficient * refreshRate->delta;
	if (potentialRate < refreshRate->lowerBound || potentialRate > refreshRate->upperBound) {
		return;
	}
	refreshRate->value = potentialRate;
}

//calculate ball position and determinate if gaining scores
void update() {
	setNextPositionOfBall(ball);
	detactPaddleAndRebound(paddleA, ball);
	detactPaddleAndRebound(paddleB, ball);
	int pointGet = detactBoundaryAndRebound(court, ball);

	if (pointGet) {
		updateScores(court, pointGet);
		placeBall(court->centerX, court->width / 2, ball);
	} else {
		moveBall(ball);	
	}
}

void setNextPositionOfBall(Ball* ball) {
	ball->nextX = ball->x + ball->vX;
	ball->nextY = ball->y + ball->vY;
}

//after rebounding, finally, set the position of the ball
void moveBall(Ball* ball) {
	ball->x = ball->nextX;
	ball->y = ball->nextY;
}

void placeBall(int x, int y, Ball* ball) {
	ball->x = ball->nextX = x;
	ball->y = ball->nextY = y;
	ball->vX = (rand() / (float)RAND_MAX) < 0.5? -1: 1; //due to rendering problems, vX should be either -1 or +1
	ball->vY = (rand() / (float)RAND_MAX) * 2 - 1;
}

void detactPaddleAndRebound(Paddle *paddle, Ball* ball) {
	if (paddleDetaction(paddle, ball)) {
		reboundOnPaddle(paddle, ball);
	}
}

bool paddleDetaction(Paddle *paddle, Ball* ball) {
	return paddleXDetection(paddle, ball) && paddleYDetection(paddle, ball);
}

bool paddleXDetection(Paddle *paddle, Ball* ball) {
	float offsetX = ball->x - paddle->x;
	float offsetNextX = ball->nextX - paddle->x;

	// if signs are different, it means the ball is crossing over the paddle, which is not allowed
	return offsetX * offsetNextX <= 0;
}

bool paddleYDetection(Paddle *paddle, Ball* ball) {
	return ball->nextY >= paddle->topY && ball->nextY <= paddle->topY + paddle->size;
}

void reboundOnPaddle(Paddle *paddle, Ball* ball) {
	ball->vX = -ball->vX;
	ball->nextX = ball->x + ball->vX;

	//test if paddle moves recently, because we want to change the velocity of the ball
	float duration = fabs(difftime(time(NULL), paddle->lastMoveTimestamp));
	if (duration <= DURATION_OF_ENERGY_PRESERVED) {
		//the imaginary formula of acceration as follow:
		ball->vY += limitAcceration(paddle->speed * (1 - duration / DURATION_OF_ENERGY_PRESERVED) * PASSED_ENEGRY_COEF);
		ball->nextY = ball->y + ball->vY;
	}
}

// limiting the acceration prevents the crazy bouncing
float limitAcceration(float potentialAcceration) {
	if (fabs(potentialAcceration) > ACCERATION_THRESHOLD) {
		return potentialAcceration < 0? -ACCERATION_THRESHOLD: ACCERATION_THRESHOLD;
	}
	return  potentialAcceration;
}

/* return 1 if left player gets a point,
 -1 if right player gets a point,
 or 0 if no players get points.
 */
int detactBoundaryAndRebound(Court* court, Ball* ball) {
	if (((int)ball->nextY) <= 0 || ((int)ball->nextY) >= court->width - 1) {
		ball->nextY = ball->y;
		ball->vY = -ball->vY;
	}

	if (ball->x <= 0) {
		return -1;
	}

	if (ball->x >= court->length - 1) {
		return 1;
	}

	return 0;
}

void updateScores(Court *court, int pointGet) {
	if (pointGet > 0) {
		court->leftScore += pointGet;
	} else {
		court->rightScore += -pointGet;
	}
}

//draw everything
void draw() {
	clear();
	paintBallSpeed(ball);
	drawCourt(court);	
	drawPaddle(paddleA);
	drawPaddle(paddleB);
	drawBall(ball);
	refresh();
}

void paintBallSpeed(Ball *ball) {
	mvprintw(1, 1, "vx = %.2f", ball->vX);
	mvprintw(2, 1, "vy = %.2f", ball->vY);
}

void drawCourt(Court *court) {
	drawBoundary(court);
	showScores(court);
}

void drawBoundary(Court *court) {
	box(stdscr, ACS_VLINE, ACS_HLINE); //the boundary
	mvaddch(0, court->centerX, ACS_TTEE); //top T
	mvvline(1, court->centerX, ACS_VLINE, court->width - 2); //center division
	mvaddch(court->width - 1, court->centerX, ACS_BTEE); // bottom T
}

void showScores(Court *court) {
	int leftScoreStartX = court->centerX - court->padding - leftStartPosition(court->leftScore);
	int rightScoreStartX = court->centerX + court->padding;

	mvprintw(court->padding, leftScoreStartX, "%d", court->leftScore);
	mvprintw(court->padding, rightScoreStartX, "%d", court->rightScore);
}

int leftStartPosition(int score) {
	if (score == 0) {
		return 1;
	}
	//calculates how many digits of the score, including negative sign just in case
	//NOTE: there is no log10(0), so it should be excluded beforehand
	return ((int)log10(abs(score))) + (score < 0? 1: 0);
}

void drawPaddle(Paddle* paddle) {
	mvvline(paddle->topY, paddle->x, ACS_CKBOARD, paddle->size);
}

void drawBall(Ball *ball) {
	mvaddch((int)ball->y, (int)ball->x, ball->icon);
}