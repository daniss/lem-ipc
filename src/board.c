#include "game.h"

void init_board(game_state_t *game_state) {
	int i, j;
	
	for (i = 0; i < BOARD_SIZE; i++) {
		for (j = 0; j < BOARD_SIZE; j++) {
			game_state->board[i][j] = EMPTY_CELL;
		}
	}
	
	game_state->player_count = 0;
	game_state->game_over = 0;
	game_state->seen_multiple_teams = 0;
	game_state->total_kills = 0;
	game_state->game_start_time = time(NULL);
	game_state->control_pause = 0;
	game_state->control_tick_ms = 500;
}

static int count_teams_alive(game_state_t *game_state) {
	int team_ids[MAX_TRACKED_TEAMS];
	int team_count = 0;
	int i;
	int j;

	for (i = 0; i < BOARD_SIZE; i++) {
		for (j = 0; j < BOARD_SIZE; j++) {
			int team = game_state->board[i][j];
			int found = 0;
			int index;

			if (team == EMPTY_CELL) {
				continue;
			}
			for (index = 0; index < team_count; index++) {
				if (team_ids[index] == team) {
					found = 1;
					break;
				}
			}
			if (!found && team_count < MAX_TRACKED_TEAMS) {
				team_ids[team_count] = team;
				team_count++;
			}
		}
	}

	return team_count;
}

static int collect_team_counts(game_state_t *game_state, int *teams, int *counts) {
	int team_count = 0;
	int i;
	int j;

	for (i = 0; i < BOARD_SIZE; i++) {
		for (j = 0; j < BOARD_SIZE; j++) {
			int team = game_state->board[i][j];
			int index;

			if (team == EMPTY_CELL) {
				continue;
			}

			for (index = 0; index < team_count; index++) {
				if (teams[index] == team) {
					counts[index]++;
					break;
				}
			}

			if (index == team_count && team_count < MAX_TRACKED_TEAMS) {
				teams[team_count] = team;
				counts[team_count] = 1;
				team_count++;
			}
		}
	}

	return team_count;
}

void display_board(game_state_t *game_state, int sem_id) {
	int i, j;

	sem_lock(sem_id, SEM_BOARD);

	int board_copy[BOARD_SIZE][BOARD_SIZE];
	int player_count = game_state->player_count;
	int total_kills = game_state->total_kills;
	int game_start_time = game_state->game_start_time;
	int teams_copy[MAX_TRACKED_TEAMS];
	int counts_copy[MAX_TRACKED_TEAMS];
	int team_entries = collect_team_counts(game_state, teams_copy, counts_copy);
	int teams_alive = count_teams_alive(game_state);

	for (i = 0; i < BOARD_SIZE; i++) {
		for (j = 0; j < BOARD_SIZE; j++) {
			board_copy[i][j] = game_state->board[i][j];
		}
	}

	sem_unlock(sem_id, SEM_BOARD);

	system("clear");
	printf("╔══════════════════════════════════╗\n");
	printf("║          Lem-IPC Arena           ║\n");
	printf("╚══════════════════════════════════╝\n");

	printf("Players: %d | Teams alive: %d | Kills: %d\n",
		   player_count, teams_alive, total_kills);

	int game_time = time(NULL) - game_start_time;
	printf("Game time: %02d:%02d\n", game_time / 60, game_time % 60);

	printf("\nTeam Stats: ");
	int t;
	for (t = 0; t < team_entries; t++) {
		printf("Team %d: %d  ", teams_copy[t], counts_copy[t]);
	}
	printf("\n\nLegend: ");
	printf(". Empty  ");
	for (t = 0; t < team_entries; t++) {
		printf("%d Team %d  ", teams_copy[t], teams_copy[t]);
	}
	printf("\n\n");

	printf("   ");
	for (j = 0; j < BOARD_SIZE; j++) {
		printf("%2d ", j);
	}
	printf("\n");

	for (i = 0; i < BOARD_SIZE; i++) {
		printf("%2d ", i);
		for (j = 0; j < BOARD_SIZE; j++) {
			int team = board_copy[i][j];
			if (team == EMPTY_CELL) {
				printf(" . ");
			} else {
				printf("%2d ", team);
			}
		}
		printf("\n");
	}
	printf("\n");
}

static int is_valid_position(int x, int y) {
	return (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE);
}

static int is_position_empty(game_state_t *game_state, int x, int y) {
	return (is_valid_position(x, y) && game_state->board[x][y] == EMPTY_CELL);
}

static position_t find_empty_position(game_state_t *game_state) {
	position_t pos;
	int attempts = 0;
	
	do {
		pos.x = rand() % BOARD_SIZE;
		pos.y = rand() % BOARD_SIZE;
		attempts++;
	} while (!is_position_empty(game_state, pos.x, pos.y) && attempts < 1000);
	
	if (attempts >= 1000) {
		pos.x = -1;
		pos.y = -1;
	}
	
	return pos;
}

int place_player(player_t *player) {
	position_t pos;
	int result = 0;
	
	sem_lock(player->sem_id, SEM_BOARD);
	
	pos = find_empty_position(player->game_state);
	if (pos.x == -1) {
		sem_unlock(player->sem_id, SEM_BOARD);
		result = -1;
		return result;
	}
	
	player->pos = pos;
	player->on_board = 1;
	player->game_state->board[pos.x][pos.y] = player->team;
	player->game_state->player_count++;
	
	sem_unlock(player->sem_id, SEM_BOARD);
	return result;
}

int move_player(player_t *player, int new_x, int new_y) {
	if (!is_valid_position(new_x, new_y)) {
		return -1;
	}
	
	sem_lock(player->sem_id, SEM_BOARD);
	
	if (!is_position_empty(player->game_state, new_x, new_y)) {
		sem_unlock(player->sem_id, SEM_BOARD);
		return -1;
	}
	
	player->game_state->board[player->pos.x][player->pos.y] = EMPTY_CELL;
	player->pos.x = new_x;
	player->pos.y = new_y;
	player->game_state->board[new_x][new_y] = player->team;
	
	sem_unlock(player->sem_id, SEM_BOARD);
	return 0;
}

void remove_player(player_t *player) {
	if (!player->on_board) {
		return;
	}

	sem_lock(player->sem_id, SEM_BOARD);
	
	if (is_valid_position(player->pos.x, player->pos.y)) {
		player->game_state->board[player->pos.x][player->pos.y] = EMPTY_CELL;
	}
	
	player->game_state->player_count--;
	player->on_board = 0;
	
	sem_unlock(player->sem_id, SEM_BOARD);
}

int is_game_over(game_state_t *game_state) {
	int teams_alive;

	if (game_state->player_count == 0) {
		return 1;
	}

	teams_alive = count_teams_alive(game_state);
	if (teams_alive >= 2) {
		game_state->seen_multiple_teams = 1;
		return 0;
	}

	return game_state->seen_multiple_teams && teams_alive <= 1;
}