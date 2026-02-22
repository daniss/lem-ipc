#include "game.h"

static const int KILL_DX[] = {-1, -1, -1,  0,  0,  1,  1,  1};
static const int KILL_DY[] = {-1,  0,  1, -1,  1, -1,  0,  1};
static const int KILL_DIRECTIONS = 8;

static const int MOVE_DX[] = {-1,  0,  0,  1};
static const int MOVE_DY[] = { 0, -1,  1,  0};
static const int MOVE_DIRECTIONS = 4;

static int count_adjacent_enemies(player_t *player, int x, int y) {
	int enemy_count = 0;
	int i;

	for (i = 0; i < KILL_DIRECTIONS; i++) {
		int nx = x + KILL_DX[i];
		int ny = y + KILL_DY[i];

		if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE) {
			int cell_team = player->game_state->board[nx][ny];
			if (cell_team != EMPTY_CELL && cell_team != player->team) {
				int same_enemy_team = 0;
				int j;
				for (j = 0; j < KILL_DIRECTIONS; j++) {
					int nnx = x + KILL_DX[j];
					int nny = y + KILL_DY[j];
					if (j != i && nnx >= 0 && nnx < BOARD_SIZE &&
						nny >= 0 && nny < BOARD_SIZE &&
						player->game_state->board[nnx][nny] == cell_team) {
						same_enemy_team = 1;
						break;
					}
				}
				if (same_enemy_team) {
					enemy_count = 2;
					break;
				}
			}
		}
	}

	return enemy_count;
}

int check_kill_condition(player_t *player) {
	sem_lock(player->sem_id, SEM_BOARD);

	int adjacent_enemies = count_adjacent_enemies(player, player->pos.x, player->pos.y);

	sem_unlock(player->sem_id, SEM_BOARD);

	return adjacent_enemies >= 2;
}

static void send_target_message(player_t *player, position_t target, int target_team) {
	message_t msg;
	msg.msg_type = player->team;
	msg.team = target_team;
	msg.player_id = player->pid;
	msg.pos = target;
	msg.action = ACTION_MOVE;

	if (msgsnd(player->msg_id, &msg, sizeof(msg) - sizeof(long), IPC_NOWAIT) == -1) {
		if (errno != EAGAIN) {
			perror("msgsnd");
		}
	}
}

static int receive_target_message(player_t *player, position_t *target, int *target_team) {
	message_t msg;

	if (msgrcv(player->msg_id, &msg, sizeof(msg) - sizeof(long),
			   player->team, IPC_NOWAIT) != -1) {
		*target = msg.pos;
		*target_team = msg.team;
		return 1;
	}
	return 0;
}

static position_t find_nearest_enemy(player_t *player, int *enemy_team) {
	position_t nearest;
	nearest.x = -1;
	nearest.y = -1;
	int min_distance = BOARD_SIZE * BOARD_SIZE + 1;
	int i, j;

	for (i = 0; i < BOARD_SIZE; i++) {
		for (j = 0; j < BOARD_SIZE; j++) {
			int cell_team = player->game_state->board[i][j];
			if (cell_team != EMPTY_CELL && cell_team != player->team) {
				int dist = abs(i - player->pos.x) + abs(j - player->pos.y);
				if (dist < min_distance) {
					min_distance = dist;
					nearest.x = i;
					nearest.y = j;
					*enemy_team = cell_team;
				}
			}
		}
	}

	return nearest;
}

static int count_nearby_teammates(player_t *player, int x, int y, int radius) {
	int count = 0;
	int i, j;

	for (i = x - radius; i <= x + radius; i++) {
		for (j = y - radius; j <= y + radius; j++) {
			if (i >= 0 && i < BOARD_SIZE && j >= 0 && j < BOARD_SIZE) {
				if (player->game_state->board[i][j] == player->team &&
					!(i == player->pos.x && j == player->pos.y)) {
					count++;
				}
			}
		}
	}

	return count;
}

static int is_safe_move(player_t *player, int x, int y) {
	return count_adjacent_enemies(player, x, y) < 2;
}

static position_t get_move_toward_target(player_t *player, position_t target) {
	position_t best_move;
	best_move.x = -1;
	best_move.y = -1;
	int min_distance = BOARD_SIZE * BOARD_SIZE + 1;
	int i;

	for (i = 0; i < MOVE_DIRECTIONS; i++) {
		int nx = player->pos.x + MOVE_DX[i];
		int ny = player->pos.y + MOVE_DY[i];

		if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE &&
			player->game_state->board[nx][ny] == EMPTY_CELL) {

			int dist = abs(nx - target.x) + abs(ny - target.y);

			if (dist < min_distance && is_safe_move(player, nx, ny)) {
				min_distance = dist;
				best_move.x = nx;
				best_move.y = ny;
			}
		}
	}

	if (best_move.x == -1) {
		min_distance = BOARD_SIZE * BOARD_SIZE + 1;
		for (i = 0; i < MOVE_DIRECTIONS; i++) {
			int nx = player->pos.x + MOVE_DX[i];
			int ny = player->pos.y + MOVE_DY[i];

			if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE &&
				player->game_state->board[nx][ny] == EMPTY_CELL) {
				int dist = abs(nx - target.x) + abs(ny - target.y);
				if (dist < min_distance) {
					min_distance = dist;
					best_move.x = nx;
					best_move.y = ny;
				}
			}
		}
	}

	return best_move;
}

static position_t get_intelligent_move(player_t *player) {
	position_t target;
	int target_team = 0;
	position_t result;
	result.x = -1;
	result.y = -1;

	sem_lock(player->sem_id, SEM_BOARD);

	if (receive_target_message(player, &target, &target_team)) {
		int target_still_there = (target.x >= 0 && target.x < BOARD_SIZE &&
								  target.y >= 0 && target.y < BOARD_SIZE &&
								  player->game_state->board[target.x][target.y] == target_team);

		if (target_still_there) {
			result = get_move_toward_target(player, target);
			sem_unlock(player->sem_id, SEM_BOARD);
			return result;
		}
	}

	int enemy_team = 0;
	target = find_nearest_enemy(player, &enemy_team);

	if (target.x != -1) {
		int nearby_teammates = count_nearby_teammates(player, player->pos.x, player->pos.y, 3);
		result = get_move_toward_target(player, target);
		sem_unlock(player->sem_id, SEM_BOARD);

		if (nearby_teammates > 0 || (rand() % 3 == 0)) {
			send_target_message(player, target, enemy_team);
		}

		return result;
	}

	position_t moves[MOVE_DIRECTIONS];
	int valid_moves = 0;
	int i;

	for (i = 0; i < MOVE_DIRECTIONS; i++) {
		int nx = player->pos.x + MOVE_DX[i];
		int ny = player->pos.y + MOVE_DY[i];

		if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE &&
			player->game_state->board[nx][ny] == EMPTY_CELL &&
			is_safe_move(player, nx, ny)) {
			moves[valid_moves].x = nx;
			moves[valid_moves].y = ny;
			valid_moves++;
		}
	}

	if (valid_moves > 0) {
		int choice = rand() % valid_moves;
		result = moves[choice];
		sem_unlock(player->sem_id, SEM_BOARD);
	} else {
		for (i = 0; i < MOVE_DIRECTIONS; i++) {
			int nx = player->pos.x + MOVE_DX[i];
			int ny = player->pos.y + MOVE_DY[i];

			if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE &&
				player->game_state->board[nx][ny] == EMPTY_CELL) {
				result.x = nx;
				result.y = ny;
				sem_unlock(player->sem_id, SEM_BOARD);
				return result;
			}
		}
		result.x = -1;
		result.y = -1;
		sem_unlock(player->sem_id, SEM_BOARD);
	}

	return result;
}

void player_game_loop(player_t *player, int display_mode) {
	int move_counter = 0;
	int killed = 0;
	int move_period = 5;

	while (!killed) {
		int game_over;
		int paused;
		int tick_ms;

		sem_lock(player->sem_id, SEM_BOARD);
		game_over = player->game_state->game_over;
		paused = player->game_state->control_pause;
		tick_ms = player->game_state->control_tick_ms;
		sem_unlock(player->sem_id, SEM_BOARD);
		if (game_over) {
			if (player->on_board) {
				remove_player(player);
			}
			break;
		}

		if (tick_ms < 50) {
			tick_ms = 50;
		} else if (tick_ms > 2000) {
			tick_ms = 2000;
		}

		if (paused) {
			usleep(100000);
			continue;
		}

		if (display_mode && (move_counter % 2 == 0)) {
			display_board(player->game_state, player->sem_id);
		}

		if (check_kill_condition(player)) {
			sem_lock(player->sem_id, SEM_BOARD);
			player->game_state->total_kills++;
			sem_unlock(player->sem_id, SEM_BOARD);

			printf("💀 Player %d from team %d has been eliminated!\n",
				   player->pid, player->team);
			remove_player(player);
			killed = 1;
			break;
		}

		sem_lock(player->sem_id, SEM_BOARD);
		if (is_game_over(player->game_state)) {
			player->game_state->game_over = 1;
			sem_unlock(player->sem_id, SEM_BOARD);
			if (player->on_board) {
				remove_player(player);
			}
			break;
		}
		sem_unlock(player->sem_id, SEM_BOARD);

		move_counter++;
		if (move_counter % move_period == 0) {
			position_t new_pos = get_intelligent_move(player);
			if (new_pos.x != -1) {
				move_player(player, new_pos.x, new_pos.y);
			}
		}

		usleep((useconds_t)tick_ms * 1000);
	}

	if (display_mode) {
		display_board(player->game_state, player->sem_id);
	}

	if (player->game_state->game_over) {
		printf("Game over! Player %d from team %d exiting.\n",
			   player->pid, player->team);
	}
}