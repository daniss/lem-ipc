#ifndef GAME_H
#define GAME_H

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/wait.h>

#define BOARD_SIZE 10
#define EMPTY_CELL 0
#define MAX_TRACKED_TEAMS (BOARD_SIZE * BOARD_SIZE)

#define IPC_KEY_BASE 0x12345
#define SHM_KEY (IPC_KEY_BASE + 1)
#define MSG_KEY (IPC_KEY_BASE + 2)
#define SEM_KEY (IPC_KEY_BASE + 3)

#define SEM_BOARD 0
#define SEM_COUNT 1

typedef struct {
	int x;
	int y;
} position_t;

typedef struct {
	int board[BOARD_SIZE][BOARD_SIZE];
	int player_count;
	int game_over;
	int seen_multiple_teams;
	int total_kills;
	int game_start_time;
	int control_pause;
	int control_tick_ms;
} game_state_t;

typedef struct {
	long msg_type;
	int team;
	int player_id;
	position_t pos;
	int action;
} message_t;

typedef struct {
	int team;
	int pid;
	int on_board;
	position_t pos;
	int shm_id;
	int msg_id;
	int sem_id;
	game_state_t *game_state;
} player_t;

enum actions {
	ACTION_MOVE
};

void init_ipc(player_t *player);
void cleanup_ipc(player_t *player);
void init_board(game_state_t *game_state);
void display_board(game_state_t *game_state, int sem_id);
int place_player(player_t *player);
int move_player(player_t *player, int new_x, int new_y);
int check_kill_condition(player_t *player);
void remove_player(player_t *player);
int is_game_over(game_state_t *game_state);
void sem_lock(int sem_id, int sem_num);
void sem_unlock(int sem_id, int sem_num);
void player_game_loop(player_t *player, int display_mode);

#endif