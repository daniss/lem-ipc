#include "game.h"

static int count_adjacent_enemies(player_t *player, int x, int y) {
	int enemy_count = 0;
	// Directions: up-left, up, up-right, left, right, down-left, down, down-right
	int dx[] = {-1, -1, -1,  0,  0,  1,  1,  1};
	int dy[] = {-1,  0,  1, -1,  1, -1,  0,  1};
	int i;
	
	for (i = 0; i < 8; i++) {
		int nx = x + dx[i];
		int ny = y + dy[i];
		
		if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE) {
			int cell_team = player->game_state->board[nx][ny];
			if (cell_team != EMPTY_CELL && cell_team != player->team) {
				int same_enemy_team = 0;
				int j;
				for (j = 0; j < 8; j++) {
					int nnx = x + dx[j];
					int nny = y + dy[j];
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

static position_t get_random_move(player_t *player) {
	position_t moves[8];
	int valid_moves = 0;
	int dx[] = {-1, -1, -1,  0,  0,  1,  1,  1};
	int dy[] = {-1,  0,  1, -1,  1, -1,  0,  1};
	int i;
	
	for (i = 0; i < 8; i++) {
		int nx = player->pos.x + dx[i];
		int ny = player->pos.y + dy[i];
		
		if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE &&
			player->game_state->board[nx][ny] == EMPTY_CELL) {
			moves[valid_moves].x = nx;
			moves[valid_moves].y = ny;
			valid_moves++;
		}
	}
	
	position_t result;
	if (valid_moves > 0) {
		int choice = rand() % valid_moves;
		result = moves[choice];
	} else {
		// No valid moves available
		result.x = -1;
		result.y = -1;
	}
	
	return result;
}

void player_game_loop(player_t *player) {
	int move_counter = 0;
	int killed = 0;
	
	while (!player->game_state->game_over && !killed) {
		if (check_kill_condition(player)) {
			sem_lock(player->sem_id, SEM_BOARD);
			player->game_state->total_kills++;
			sem_unlock(player->sem_id, SEM_BOARD);
			
			printf("💀 Player %d from team %d has been eliminated!\n", 
				   player->player_id, player->team);
			remove_player(player);
			killed = 1;
			break;
		}
		
		if (is_game_over(player->game_state)) {
			player->game_state->game_over = 1;
			break;
		}
		
		move_counter++;
		if (move_counter % 5 == 0) {
			position_t new_pos = get_random_move(player);
			if (new_pos.x != -1) {
				move_player(player, new_pos.x, new_pos.y);
			}
		}
		
		usleep(500000);
	}
	
	if (player->game_state->game_over) {
		printf("Game over! Player %d from team %d exiting.\n", 
			   player->player_id, player->team);
	}
}