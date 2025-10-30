#include "game.h"

void init_board(game_state_t *game_state) {
	int i, j;
	
	// Clear the board
	for (i = 0; i < BOARD_SIZE; i++) {
		for (j = 0; j < BOARD_SIZE; j++) {
			game_state->board[i][j] = EMPTY_CELL;
		}
	}
	
	game_state->player_count = 0;
	game_state->teams_alive = 0;
	game_state->game_over = 0;
	game_state->total_kills = 0;
	game_state->game_start_time = time(NULL);
	
	// Initialize team counts
	for (i = 0; i <= MAX_TEAMS; i++) {
		game_state->team_counts[i] = 0;
	}
	
	// Initialize player positions
	for (i = 0; i < MAX_TEAMS * MAX_PLAYERS_PER_TEAM; i++) {
		game_state->players[i].x = -1;
		game_state->players[i].y = -1;
		game_state->player_teams[i] = 0;
	}
}

void display_board(game_state_t *game_state, int sem_id) {
	int i, j;
	// ANSI color codes for different teams
	const char* team_colors[] = {
		"\033[37m",  // White for empty
		"\033[31m",  // Red for team 1
		"\033[32m",  // Green for team 2
		"\033[33m",  // Yellow for team 3
		"\033[34m"   // Blue for team 4
	};
	const char* reset_color = "\033[0m";

	// Lock before reading game state (per FAQ requirement)
	sem_lock(sem_id, SEM_BOARD);

	// Copy data we need while holding the lock
	int board_copy[BOARD_SIZE][BOARD_SIZE];
	int player_count = game_state->player_count;
	int teams_alive = game_state->teams_alive;
	int total_kills = game_state->total_kills;
	int team_counts_copy[MAX_TEAMS + 1];
	int game_start_time = game_state->game_start_time;

	for (i = 0; i < BOARD_SIZE; i++) {
		for (j = 0; j < BOARD_SIZE; j++) {
			board_copy[i][j] = game_state->board[i][j];
		}
	}

	int t;
	for (t = 0; t <= MAX_TEAMS; t++) {
		team_counts_copy[t] = game_state->team_counts[t];
	}

	// Release lock - now we can safely display without blocking others
	sem_unlock(sem_id, SEM_BOARD);

	// Display using copied data (no lock needed for printf)
	system("clear");
	printf("\033[1m╔══════════════════════════════════╗\033[0m\n");
	printf("\033[1m║          Lem-IPC Arena           ║\033[0m\n");
	printf("\033[1m╚══════════════════════════════════╝\033[0m\n");

	printf("Players: \033[1m%d\033[0m | Teams alive: \033[1m%d\033[0m | Kills: \033[1m%d\033[0m\n",
		   player_count, teams_alive, total_kills);

	int game_time = time(NULL) - game_start_time;
	printf("Game time: \033[1m%02d:%02d\033[0m\n", game_time / 60, game_time % 60);

	printf("\nTeam Stats: ");
	for (t = 1; t <= MAX_TEAMS; t++) {
		if (team_counts_copy[t] > 0) {
			printf("%sTeam %d: %d%s  ", team_colors[t], t, team_counts_copy[t], reset_color);
		}
	}
	printf("\n\nLegend: ");
	printf("%s● Empty%s  ", team_colors[0], reset_color);
	for (t = 1; t <= MAX_TEAMS; t++) {
		printf("%s● Team %d%s  ", team_colors[t], t, reset_color);
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
				printf("%s ⋅%s ", team_colors[0], reset_color);
			} else if (team <= MAX_TEAMS) {
				printf("%s %d%s ", team_colors[team], team, reset_color);
			} else {
				printf(" ? ");
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
	player->game_state->board[pos.x][pos.y] = player->team;
	player->game_state->players[player->player_id] = pos;
	player->game_state->player_teams[player->player_id] = player->team;
	player->game_state->player_count++;
	
	if (player->team > 0 && player->team <= MAX_TEAMS) {
		if (player->game_state->team_counts[player->team] == 0) {
			player->game_state->teams_alive++;
		}
		player->game_state->team_counts[player->team]++;
	}
	
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
	player->game_state->players[player->player_id] = player->pos;
	
	sem_unlock(player->sem_id, SEM_BOARD);
	return 0;
}

void remove_player(player_t *player) {
	sem_lock(player->sem_id, SEM_BOARD);
	
	if (is_valid_position(player->pos.x, player->pos.y)) {
		player->game_state->board[player->pos.x][player->pos.y] = EMPTY_CELL;
	}
	
	player->game_state->players[player->player_id].x = -1;
	player->game_state->players[player->player_id].y = -1;
	player->game_state->player_teams[player->player_id] = 0;
	player->game_state->player_count--;
	
	if (player->team > 0 && player->team <= MAX_TEAMS) {
		player->game_state->team_counts[player->team]--;
		if (player->game_state->team_counts[player->team] == 0) {
			player->game_state->teams_alive--;
		}
	}
	
	sem_unlock(player->sem_id, SEM_BOARD);
}

int is_game_over(game_state_t *game_state) {
	// Game over only if no players remain
	// OR if only one team remains AND game has been running for at least 10 seconds
	int game_duration = time(NULL) - game_state->game_start_time;
	return (game_state->player_count == 0 || 
			(game_state->teams_alive <= 1 && game_state->player_count > 0 && game_duration >= 10));
}