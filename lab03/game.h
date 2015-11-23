#ifndef __GAME_H__
#define __GAME_H__

uint32_t generate_hash(char *str) {
	uint16_t * wtf = (uint16_t *) str;
	uint32_t res = 0xf653b931;
	int i = 0;
	for (; i < 16; ++i) {
		res += (((wtf[i] * wtf[i]) << i) ^0xaa55);
	}
	return res;
}

#define ROCK	0
#define PAPER	1
#define SCISSORS	2
#define LIZARD	3
#define SPOCK	4


#define MOVES_TOTAL			8
#define A_WIN				0
#define DRAW				1
#define B_WIN				2


int table[5][5] = {
	{DRAW, B_WIN, A_WIN, A_WIN, B_WIN},
	{A_WIN, DRAW, B_WIN, B_WIN, A_WIN},
	{B_WIN, A_WIN, DRAW, A_WIN, B_WIN},
	{B_WIN, A_WIN, B_WIN, DRAW, A_WIN},
	{A_WIN, B_WIN, A_WIN, B_WIN, DRAW}
};


int rpsls(int a, int b) {
	return table[a][b];
}





#endif // __GAME_H__

