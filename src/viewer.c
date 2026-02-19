#include "game.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

#define VIEWER_TITLE_BASE "lemipc_viewer"
#define WINDOW_WIDTH 900
#define WINDOW_HEIGHT 780
#define BOARD_MARGIN 40
#define HUD_HEIGHT 120
#define VIEWER_FRAME_DELAY_MS 60
#define DEFAULT_GAME_TICK_MS 500
#define MIN_GAME_TICK_MS 80
#define MAX_GAME_TICK_MS 2000

static int viewer_sem_lock(int sem_id) {
	struct sembuf sb = {SEM_BOARD, -1, 0};
	return semop(sem_id, &sb, 1);
}

static int viewer_sem_unlock(int sem_id) {
	struct sembuf sb = {SEM_BOARD, 1, 0};
	return semop(sem_id, &sb, 1);
}

static SDL_Color team_color(int team) {
	SDL_Color palette[] = {
		{220, 20, 60, 255},
		{30, 144, 255, 255},
		{50, 205, 50, 255},
		{255, 165, 0, 255},
		{138, 43, 226, 255},
		{255, 105, 180, 255},
		{0, 206, 209, 255},
		{255, 215, 0, 255}
	};
	if (team <= 0) {
		return (SDL_Color){35, 35, 40, 255};
	}
	return palette[(team - 1 + 1024) % 8];
}

static int collect_team_counts(const game_state_t *state, int *teams, int *counts) {
	int entries = 0;
	for (int y = 0; y < BOARD_SIZE; y++) {
		for (int x = 0; x < BOARD_SIZE; x++) {
			int team = state->board[y][x];
			if (team == EMPTY_CELL) {
				continue;
			}
			int found = -1;
			for (int i = 0; i < entries; i++) {
				if (teams[i] == team) {
					found = i;
					break;
				}
			}
			if (found >= 0) {
				counts[found]++;
			} else if (entries < MAX_TRACKED_TEAMS) {
				teams[entries] = team;
				counts[entries] = 1;
				entries++;
			}
		}
	}
	return entries;
}

static void update_window_title(SDL_Window *window,
		const game_state_t *snapshot,
		int game_tick_ms,
		bool paused,
		int teams_alive) {
	int elapsed = (int)time(NULL) - snapshot->game_start_time;
	if (elapsed < 0) {
		elapsed = 0;
	}
	char title[256];
	snprintf(title, sizeof(title),
		"%s | %s | game-tick=%dms | players=%d teams=%d kills=%d time=%02d:%02d",
		VIEWER_TITLE_BASE,
		paused ? "PAUSED" : "RUNNING",
		game_tick_ms,
		snapshot->player_count,
		teams_alive,
		snapshot->total_kills,
		elapsed / 60,
		elapsed % 60);
	SDL_SetWindowTitle(window, title);
}

static void draw_hud(SDL_Renderer *renderer,
		const game_state_t *snapshot,
		int teams_alive,
		const int *teams,
		const int *counts,
		int entries,
		bool paused,
		int game_tick_ms) {
	SDL_Rect hud = {0, 0, WINDOW_WIDTH, HUD_HEIGHT};
	SDL_SetRenderDrawColor(renderer, 22, 22, 28, 255);
	SDL_RenderFillRect(renderer, &hud);

	SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
	SDL_RenderDrawLine(renderer, 0, HUD_HEIGHT - 1, WINDOW_WIDTH, HUD_HEIGHT - 1);

	SDL_Rect players_bar = {20, 18, snapshot->player_count * 8, 16};
	if (players_bar.w > WINDOW_WIDTH - 40) {
		players_bar.w = WINDOW_WIDTH - 40;
	}
	SDL_SetRenderDrawColor(renderer, 100, 170, 255, 255);
	SDL_RenderFillRect(renderer, &players_bar);

	SDL_Rect teams_bar = {20, 42, teams_alive * 20, 14};
	if (teams_bar.w > WINDOW_WIDTH - 40) {
		teams_bar.w = WINDOW_WIDTH - 40;
	}
	SDL_SetRenderDrawColor(renderer, 120, 220, 120, 255);
	SDL_RenderFillRect(renderer, &teams_bar);

	SDL_Rect kills_bar = {20, 64, snapshot->total_kills * 10, 12};
	if (kills_bar.w > WINDOW_WIDTH - 40) {
		kills_bar.w = WINDOW_WIDTH - 40;
	}
	SDL_SetRenderDrawColor(renderer, 240, 120, 120, 255);
	SDL_RenderFillRect(renderer, &kills_bar);

	int x = WINDOW_WIDTH - 22;
	for (int i = 0; i < entries && i < 12; i++) {
		SDL_Color c = team_color(teams[i]);
		SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
		int h = 6 + counts[i] * 7;
		if (h > HUD_HEIGHT - 20) {
			h = HUD_HEIGHT - 20;
		}
		SDL_Rect team_stat = {x - 12, HUD_HEIGHT - 8 - h, 10, h};
		SDL_RenderFillRect(renderer, &team_stat);
		x -= 15;
	}

	if (paused) {
		SDL_SetRenderDrawColor(renderer, 255, 208, 0, 255);
		SDL_Rect p1 = {WINDOW_WIDTH / 2 - 14, 24, 8, 30};
		SDL_Rect p2 = {WINDOW_WIDTH / 2 + 6, 24, 8, 30};
		SDL_RenderFillRect(renderer, &p1);
		SDL_RenderFillRect(renderer, &p2);
	}

	int speed_w = MAX_GAME_TICK_MS - game_tick_ms;
	if (speed_w < 0) {
		speed_w = 0;
	}
	speed_w = (speed_w * 200) / (MAX_GAME_TICK_MS - MIN_GAME_TICK_MS);
	if (speed_w > 200) {
		speed_w = 200;
	}
	SDL_Rect speed_bar = {WINDOW_WIDTH / 2 - 100, 70, speed_w, 10};
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_RenderDrawRect(renderer, &(SDL_Rect){WINDOW_WIDTH / 2 - 100, 70, 200, 10});
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 180);
	SDL_RenderFillRect(renderer, &speed_bar);
}

static void draw_board(SDL_Renderer *renderer, const game_state_t *snapshot) {
	int board_top = HUD_HEIGHT + BOARD_MARGIN;
	int board_h = WINDOW_HEIGHT - board_top - BOARD_MARGIN;
	int board_w = WINDOW_WIDTH - 2 * BOARD_MARGIN;
	int cell_w = board_w / BOARD_SIZE;
	int cell_h = board_h / BOARD_SIZE;

	SDL_SetRenderDrawColor(renderer, 12, 12, 16, 255);
	SDL_RenderClear(renderer);

	for (int y = 0; y < BOARD_SIZE; y++) {
		for (int x = 0; x < BOARD_SIZE; x++) {
			SDL_Color c = team_color(snapshot->board[y][x]);
			SDL_Rect cell = {
				BOARD_MARGIN + x * cell_w,
				board_top + y * cell_h,
				cell_w,
				cell_h
			};
			SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
			SDL_RenderFillRect(renderer, &cell);

			SDL_SetRenderDrawColor(renderer, 45, 45, 55, 255);
			SDL_RenderDrawRect(renderer, &cell);
		}
	}
}

int main(void) {
	int shm_id = shmget(SHM_KEY, sizeof(game_state_t), 0666);
	int sem_id = semget(SEM_KEY, SEM_COUNT, 0666);
	if (shm_id == -1 || sem_id == -1) {
		fprintf(stderr, "lemipc_viewer: cannot access IPCs (start game players first)\n");
		return 1;
	}

	game_state_t *shared = shmat(shm_id, NULL, 0);
	if (shared == (void *)-1) {
		perror("shmat");
		return 1;
	}

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		shmdt(shared);
		return 1;
	}

	SDL_Window *window = SDL_CreateWindow(
		VIEWER_TITLE_BASE,
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		WINDOW_WIDTH,
		WINDOW_HEIGHT,
		SDL_WINDOW_SHOWN
	);
	if (!window) {
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		SDL_Quit();
		shmdt(shared);
		return 1;
	}

	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!renderer) {
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	}
	if (!renderer) {
		fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
		SDL_DestroyWindow(window);
		SDL_Quit();
		shmdt(shared);
		return 1;
	}

	bool running = true;
	bool paused = false;
	int game_tick_ms = DEFAULT_GAME_TICK_MS;
	game_state_t snapshot;
	memset(&snapshot, 0, sizeof(snapshot));

	if (viewer_sem_lock(sem_id) == 0) {
		if (shared->control_tick_ms <= 0) {
			shared->control_tick_ms = DEFAULT_GAME_TICK_MS;
		}
		paused = shared->control_pause != 0;
		game_tick_ms = shared->control_tick_ms;
		memcpy(&snapshot, shared, sizeof(snapshot));
		viewer_sem_unlock(sem_id);
	}

	while (running) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				running = false;
			} else if (event.type == SDL_KEYDOWN) {
				switch (event.key.keysym.sym) {
					case SDLK_ESCAPE:
						running = false;
						break;
					case SDLK_SPACE:
						paused = !paused;
						if (viewer_sem_lock(sem_id) == 0) {
							shared->control_pause = paused ? 1 : 0;
							viewer_sem_unlock(sem_id);
						}
						break;
					case SDLK_PLUS:
					case SDLK_KP_PLUS:
						game_tick_ms -= 50;
						if (game_tick_ms < MIN_GAME_TICK_MS) {
							game_tick_ms = MIN_GAME_TICK_MS;
						}
						if (viewer_sem_lock(sem_id) == 0) {
							shared->control_tick_ms = game_tick_ms;
							viewer_sem_unlock(sem_id);
						}
						break;
					case SDLK_MINUS:
					case SDLK_KP_MINUS:
						game_tick_ms += 50;
						if (game_tick_ms > MAX_GAME_TICK_MS) {
							game_tick_ms = MAX_GAME_TICK_MS;
						}
						if (viewer_sem_lock(sem_id) == 0) {
							shared->control_tick_ms = game_tick_ms;
							viewer_sem_unlock(sem_id);
						}
						break;
					case SDLK_0:
						game_tick_ms = DEFAULT_GAME_TICK_MS;
						if (viewer_sem_lock(sem_id) == 0) {
							shared->control_tick_ms = game_tick_ms;
							viewer_sem_unlock(sem_id);
						}
						break;
					default:
						break;
				}
			}
		}

		if (!paused) {
			if (viewer_sem_lock(sem_id) == 0) {
				memcpy(&snapshot, shared, sizeof(snapshot));
				paused = shared->control_pause != 0;
				game_tick_ms = shared->control_tick_ms;
				viewer_sem_unlock(sem_id);
			}
		} else {
			if (viewer_sem_lock(sem_id) == 0) {
				memcpy(&snapshot, shared, sizeof(snapshot));
				viewer_sem_unlock(sem_id);
			}
		}

		int teams[MAX_TRACKED_TEAMS] = {0};
		int counts[MAX_TRACKED_TEAMS] = {0};
		int team_entries = collect_team_counts(&snapshot, teams, counts);
		int teams_alive = team_entries;

		update_window_title(window, &snapshot, game_tick_ms, paused, teams_alive);
		draw_board(renderer, &snapshot);
		draw_hud(renderer, &snapshot, teams_alive, teams, counts, team_entries, paused, game_tick_ms);
		SDL_RenderPresent(renderer);

		if (snapshot.game_over) {
			running = false;
		}
		SDL_Delay((Uint32)VIEWER_FRAME_DELAY_MS);
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	shmdt(shared);
	return 0;
}
