//도트 매트릭스
//DIN : 12
//CS : 10
//CLK : 11

//조이스틱
//VRx : A1
//VRy : A0
//SW : 8

#include "LedControl.h" // 도트 매트릭스 사용 시 라이브러리 사용 필요

// 스네이크 게임 핀 정의
struct Pin {
	static const short joystickX = A1;   // 조이스틱 X핀
	static const short joystickY = A0;   // 조이스틱 Y핀

	static const short CLK = 11;   // clock for LED matrix
	static const short CS  = 10;  // chip-select for LED matrix
	static const short DIN = 12; // data-in for LED matrix
};

// Dot LED matrix 밝기: 0(어두움) ~ 15(밝음)
const short intensity = 8;

// 메시지 출력 시 delay 시간
const short messageSpeed = 5;

// 뱀 처음 몸 길이 (1...63, recommended 3)
const short initialSnakeLength = 3;


void setup() {
	Serial.begin(115200);
	initialize();         // pins & LED matrix 초기화
	calibrateJoystick(); // 조이스틱 기본 위치 보정
	showSnakeMessage(); // 도트 매트릭스에 스네이크 게임 메시지 출력
}


void loop() {
	generateFood();      // 먹이 없으면 먹이 1개 생성
	scanJoystick();     // 조이스틱 움직임 확인 & 먹이 깜빡이기
	calculateSnake();  // 뱀 객체 계산 (길이, 위치)
	handleGameStates();
}



// --------------------------------------------------------------- //
// ----------------------------- 변수 ----------------------------- //
// --------------------------------------------------------------- //

LedControl matrix(Pin::DIN, Pin::CLK, Pin::CS, 1); //매트릭스 핀 설정

struct Point {
	int row = 0, col = 0;
	Point(int row = 0, int col = 0): row(row), col(col) {} //생성자 멤버변수 초기화
};

struct Coordinate {
	int x = 0, y = 0;
	Coordinate(int x = 0, int y = 0): x(x), y(y) {} //생성자 멤버변수 초기화
};

bool win = false;
bool gameOver = false;

// 뱀 머리 기본좌표, 랜덤 생성
Point snake;

// 먹이 선언 (아직은 먹이 없음)
Point food(-1, -1);

// 사용자가 조이스틱 기본 위치 보정을 사용하지 않는 경우 지정 (내꺼 가운데 보고 맞춤)
Coordinate joystickHome(512, 495);

// 뱀 변수
int snakeLength = initialSnakeLength; // 뱀 초기 길이 지정 (3 추천)
int snakeSpeed = 800; // 1000ms~1ms ,cannot be 0 (낮을수록 빨라짐)
int snakeDirection = 0; // if it is 0, the snake does not move

// 방향 상수 정의
const short up     = 1;
const short right  = 2;
const short down   = 3;  // 'down - 2' must be 'up' (180도 방향전환 방지를 위해)
const short left   = 4; // 'left - 2' must be 'right'

// 조이스틱 움직임 감지 임계값
const int joystickThreshold = 160;

int gameboard[8][8] = {};


// --------------------------------------------------------------- //
// ----------------------------- 함수 ----------------------------- //
// --------------------------------------------------------------- //

// if 먹이가 없으면, 하나 생성, 그리고 64줄 길이인지 확인 후 승리 선언
void generateFood() {
	if (food.row == -1 || food.col == -1) {
		// 몸통 64개시 승리
		if (snakeLength >= 64) {
			win = true;
			return; 
		}

		// 올바른 위치에 먹이 생길때 까지 반복
		do {
			food.col = random(8);
			food.row = random(8);
		} while (gameboard[food.row][food.col] > 0);
	}
}

// 조이스틱 움직임 확인 & 먹이 깜빡이기
void scanJoystick() {
	int previousDirection = snakeDirection; // 직전 위치 저장
	long timestamp = millis();
	while (millis() < timestamp + snakeSpeed) {
		// 뱀 속도 조절 가능

		// 조이스틱 방향이 어딘지 확인
		analogRead(Pin::joystickY) < (joystickHome.y - joystickThreshold) ? snakeDirection = up    : 0;     //1
		analogRead(Pin::joystickY) > (joystickHome.y + joystickThreshold) ? snakeDirection = down  : 0;    //3
		analogRead(Pin::joystickX) < (joystickHome.x - joystickThreshold) ? snakeDirection = left  : 0;   //4
		analogRead(Pin::joystickX) > (joystickHome.x + joystickThreshold) ? snakeDirection = right : 0;  //2

		// 180도 회전 방지
		snakeDirection + 2 == previousDirection && previousDirection != 0 ? snakeDirection = previousDirection : 0;
		snakeDirection - 2 == previousDirection && previousDirection != 0 ? snakeDirection = previousDirection : 0;

		// 먹이 깜빡거리게 하기
		matrix.setLed(0, food.row, food.col, millis() % 100 < 50 ? 1 : 0);
	}
}


// 뱀 객체 계산 (길이, 위치)
void calculateSnake() {
	switch (snakeDirection) {
		case up:
			snake.row--;
			fixEdge();
			matrix.setLed(0, snake.row, snake.col, 1);
			break;

		case right:
			snake.col++;
			fixEdge();
			matrix.setLed(0, snake.row, snake.col, 1);
			break;

		case down:
			snake.row++;
			fixEdge();
			matrix.setLed(0, snake.row, snake.col, 1);
			break;

		case left:
			snake.col--;
			fixEdge();
			matrix.setLed(0, snake.row, snake.col, 1);
			break;

		default: // 뱀 움직임 없으면 빠져나오기
			return;
	}

	// 뱀 몸에 부딪히면
	if (gameboard[snake.row][snake.col] > 1 && snakeDirection != 0) {
		gameOver = true;
		return;
	}

	// 먹이를 먹었는지 확인
	if (snake.row == food.row && snake.col == food.col) {
		food.row = -1; // 먹었다면 먹이 reset
		food.col = -1;

		// 뱀 몸통 증가
		snakeLength++;

		// 뱀 객체 길이 증가
		for (int row = 0; row < 8; row++) {
			for (int col = 0; col < 8; col++) {
				if (gameboard[row][col] > 0 ) {
					gameboard[row][col]++;
				}
			}
		}
	}

	// 뱀 머리 객체 길이 증가
	gameboard[snake.row][snake.col] = snakeLength + 1; 

	for (int row = 0; row < 8; row++) {
		for (int col = 0; col < 8; col++) {
			if (gameboard[row][col] > 0 ) {
				gameboard[row][col]--;
			}

			// 현재 뱀 출력
			matrix.setLed(0, row, col, gameboard[row][col] == 0 ? 0 : 1);
		}
	}
}


// 8x8 벽 통과
void fixEdge() {
	snake.col < 0 ? snake.col += 8 : 0;
	snake.col > 7 ? snake.col -= 8 : 0;
	snake.row < 0 ? snake.row += 8 : 0;
	snake.row > 7 ? snake.row -= 8 : 0;
}


void handleGameStates() {
	if (gameOver || win) {
		unrollSnake();

		showScoreMessage(snakeLength - initialSnakeLength);

		if (gameOver) showGameOverMessage();
		else if (win) showWinMessage();

		// 게임 다시 시작
		win = false;
		gameOver = false;
		snake.row = random(8);
		snake.col = random(8);
		food.row = -1;
		food.col = -1;
		snakeLength = initialSnakeLength;
		snakeDirection = 0;
		memset(gameboard, 0, sizeof(gameboard[0][0]) * 8 * 8);
		matrix.clearDisplay(0);
	}
}


void unrollSnake() {
	// 음식 LED 끄기
	matrix.setLed(0, food.row, food.col, LOW);

	delay(800);

	// 화면 5번 점등
	for (int i = 0; i < 5; i++) {
		// 화면 반전
		for (int row = 0; row < 8; row++) {
			for (int col = 0; col < 8; col++) {
				matrix.setLed(0, row, col, gameboard[row][col] == 0 ? 1 : 0);
			}
		}

		delay(20);

		// 화면 반전
		for (int row = 0; row < 8; row++) {
			for (int col = 0; col < 8; col++) {
				matrix.setLed(0, row, col, gameboard[row][col] == 0 ? 0 : 1);
			}
		}

		delay(50);

	}


	delay(600);

	for (int i = 1; i <= snakeLength; i++) {
		for (int row = 0; row < 8; row++) {
			for (int col = 0; col < 8; col++) {
				if (gameboard[row][col] == i) {
					matrix.setLed(0, row, col, 0);
					delay(100);
				}
			}
		}
	}
}


// 조이스틱 기본 위치 보정 (10회 측정 후 평균값)
void calibrateJoystick() {
	Coordinate values;

	for (int i = 0; i < 10; i++) {
		values.x += analogRead(Pin::joystickX);
		values.y += analogRead(Pin::joystickY);
	}

	joystickHome.x = values.x / 10;
	joystickHome.y = values.y / 10;
}


void initialize() {

	matrix.shutdown(0, false);
	matrix.setIntensity(0, intensity);
	matrix.clearDisplay(0);

	randomSeed(analogRead(A5));
	snake.row = random(8); //뱀 초기위치 설정
	snake.col = random(8);
}




// --------------------------------------------------------------- //
// -------------------------- messages --------------------------- //
// --------------------------------------------------------------- //

const PROGMEM bool snakeMessage[8][56] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

const PROGMEM bool gameOverMessage[8][90] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

const PROGMEM bool scoreMessage[8][58] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

const PROGMEM bool digits[][8][8] = {
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 1, 1, 1, 1, 0, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 1, 1, 0, 1, 1, 1, 0},
		{0, 1, 1, 1, 0, 1, 1, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 1, 1, 1, 1, 0, 0}
	},
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 1, 1, 0, 0, 0},
		{0, 0, 0, 1, 1, 0, 0, 0},
		{0, 0, 1, 1, 1, 0, 0, 0},
		{0, 0, 0, 1, 1, 0, 0, 0},
		{0, 0, 0, 1, 1, 0, 0, 0},
		{0, 0, 0, 1, 1, 0, 0, 0},
		{0, 1, 1, 1, 1, 1, 1, 0}
	},
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 1, 1, 1, 1, 0, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 0, 0, 0, 1, 1, 0},
		{0, 0, 0, 0, 1, 1, 0, 0},
		{0, 0, 1, 1, 0, 0, 0, 0},
		{0, 1, 1, 0, 0, 0, 0, 0},
		{0, 1, 1, 1, 1, 1, 1, 0}
	},
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 1, 1, 1, 1, 0, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 0, 0, 0, 1, 1, 0},
		{0, 0, 0, 1, 1, 1, 0, 0},
		{0, 0, 0, 0, 0, 1, 1, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 1, 1, 1, 1, 0, 0}
	},
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 1, 1, 0, 0},
		{0, 0, 0, 1, 1, 1, 0, 0},
		{0, 0, 1, 0, 1, 1, 0, 0},
		{0, 1, 0, 0, 1, 1, 0, 0},
		{0, 1, 1, 1, 1, 1, 1, 0},
		{0, 0, 0, 0, 1, 1, 0, 0},
		{0, 0, 0, 0, 1, 1, 0, 0}
	},
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 1, 1, 1, 1, 1, 1, 0},
		{0, 1, 1, 0, 0, 0, 0, 0},
		{0, 1, 1, 1, 1, 1, 0, 0},
		{0, 0, 0, 0, 0, 1, 1, 0},
		{0, 0, 0, 0, 0, 1, 1, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 1, 1, 1, 1, 0, 0}
	},
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 1, 1, 1, 1, 0, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 1, 1, 0, 0, 0, 0, 0},
		{0, 1, 1, 1, 1, 1, 0, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 1, 1, 1, 1, 0, 0}
	},
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 1, 1, 1, 1, 1, 1, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 0, 0, 1, 1, 0, 0},
		{0, 0, 0, 0, 1, 1, 0, 0},
		{0, 0, 0, 1, 1, 0, 0, 0},
		{0, 0, 0, 1, 1, 0, 0, 0},
		{0, 0, 0, 1, 1, 0, 0, 0}
	},
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 1, 1, 1, 1, 0, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 1, 1, 1, 1, 0, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 1, 1, 1, 1, 0, 0}
	},
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 1, 1, 1, 1, 0, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 1, 1, 1, 1, 1, 0},
		{0, 0, 0, 0, 0, 1, 1, 0},
		{0, 1, 1, 0, 0, 1, 1, 0},
		{0, 0, 1, 1, 1, 1, 0, 0}
	}
};


// 'snake' 메시지 도트 매트릭스에 출력
void showSnakeMessage() {
	[&] {
		for (int d = 0; d < sizeof(snakeMessage[0]) - 7; d++) {
			for (int col = 0; col < 8; col++) {
				delay(messageSpeed);
				for (int row = 0; row < 8; row++) {
					// PROGMEM에서 바이트를 읽고 화면에 표시
					matrix.setLed(0, row, col, pgm_read_byte(&(snakeMessage[row][col + d])));
				}
			}

			// if 조이스틱 움직이면, 메시지 출력 탈출 후 게임 준비
			if (analogRead(Pin::joystickY) < joystickHome.y - joystickThreshold
			        || analogRead(Pin::joystickY) > joystickHome.y + joystickThreshold
			        || analogRead(Pin::joystickX) < joystickHome.x - joystickThreshold
			        || analogRead(Pin::joystickX) > joystickHome.x + joystickThreshold) {
				return; // 람다 함수 반환
			}
		}
	}();

	matrix.clearDisplay(0);

	// 조이스틱 움직이기 전까지 대기
	while (analogRead(Pin::joystickY) < joystickHome.y - joystickThreshold
	        || analogRead(Pin::joystickY) > joystickHome.y + joystickThreshold
	        || analogRead(Pin::joystickX) < joystickHome.x - joystickThreshold
	        || analogRead(Pin::joystickX) > joystickHome.x + joystickThreshold) {}

}


// 'game over' 메시지 도트 매트릭스에 출력
void showGameOverMessage() {
	[&] {
		for (int d = 0; d < sizeof(gameOverMessage[0]) - 7; d++) {
			for (int col = 0; col < 8; col++) {
				delay(messageSpeed);
				for (int row = 0; row < 8; row++) {
					// PROGMEM에서 바이트를 읽고 화면에 표시
					matrix.setLed(0, row, col, pgm_read_byte(&(gameOverMessage[row][col + d])));
				}
			}

			// if 조이스틱 움직이면, 메시지 출력 탈출 후 게임 준비
			if (analogRead(Pin::joystickY) < joystickHome.y - joystickThreshold
			        || analogRead(Pin::joystickY) > joystickHome.y + joystickThreshold
			        || analogRead(Pin::joystickX) < joystickHome.x - joystickThreshold
			        || analogRead(Pin::joystickX) > joystickHome.x + joystickThreshold) {
				return; // 람다 함수 반환
			}
		}
	}();

	matrix.clearDisplay(0);

	// 조이스틱 움직이기 전까지 대기
	while (analogRead(Pin::joystickY) < joystickHome.y - joystickThreshold
	        || analogRead(Pin::joystickY) > joystickHome.y + joystickThreshold
	        || analogRead(Pin::joystickX) < joystickHome.x - joystickThreshold
	        || analogRead(Pin::joystickX) > joystickHome.x + joystickThreshold) {}

}


// 'win' 메시지 도트 매트릭스에 출력
void showWinMessage() {
    }


// 'score=ㅁ' 메시지 도트 매트릭스에 출력
void showScoreMessage(int score) {
	if (score < 0 || score > 99) return;


	int second = score % 10;
	int first = (score / 10) % 10;

	[&] {
		for (int d = 0; d < sizeof(scoreMessage[0]) + 2 * sizeof(digits[0][0]); d++) {
			for (int col = 0; col < 8; col++) {
				delay(messageSpeed);
				for (int row = 0; row < 8; row++) {
					if (d <= sizeof(scoreMessage[0]) - 8) {
						matrix.setLed(0, row, col, pgm_read_byte(&(scoreMessage[row][col + d])));
					}

					int c = col + d - sizeof(scoreMessage[0]) + 6; // 이전 메시지 앞으로 6px 이동

					// if the score is < 10, 첫 번째 숫자 (0)를 이동
					if (score < 10) c += 8;

					if (c >= 0 && c < 8) {
						if (first > 0) matrix.setLed(0, row, col, pgm_read_byte(&(digits[first][row][c]))); // 점수 >= 10 인 경우에만 출력 (위 참조)
					} else {
						c -= 8;
						if (c >= 0 && c < 8) {
							matrix.setLed(0, row, col, pgm_read_byte(&(digits[second][row][c]))); // show always
						}
					}
				}
			}

			// if 조이스틱 움직이면, 메시지 출력 탈출 후 게임 준비
			if (analogRead(Pin::joystickY) < joystickHome.y - joystickThreshold
			        || analogRead(Pin::joystickY) > joystickHome.y + joystickThreshold
			        || analogRead(Pin::joystickX) < joystickHome.x - joystickThreshold
			        || analogRead(Pin::joystickX) > joystickHome.x + joystickThreshold) {
				return; // 람다 함수 반환
			}
		}
	}();

	matrix.clearDisplay(0);
}