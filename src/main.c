#include "game.h"

static player_t g_player;
static int g_display_mode = 0;

void signal_handler(int sig) {
	// Handle signals for clean exit
	printf("\nReceived signal %d, cleaning up...\n", sig);
	if (g_player.game_state != NULL) {
		remove_player(&g_player);
		cleanup_ipc(&g_player);
	}
	exit(0);
}

void setup_signal_handlers(void) {
	// Set up signal handling for clean shutdown
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGQUIT, signal_handler);
}

void display_usage(void) {
	printf("\033[1m╔══════════════════════════════════╗\033[0m\n");
	printf("\033[1m║         Lem-IPC Game             ║\033[0m\n");
	printf("\033[1m╚══════════════════════════════════╝\033[0m\n\n");
	
	printf("\033[1mUSAGE:\033[0m\n");
	printf("  ./lemipc <team_number> [options]\n\n");
	
	printf("\033[1mARGUMENTS:\033[0m\n");
	printf("  team_number    Team number (1-4)\n\n");
	
	printf("\033[1mOPTIONS:\033[0m\n");
	printf("  -d, --display  Enable real-time board display\n");
	printf("  -h, --help     Show this help message\n");
	printf("  -v, --version  Show version information\n\n");
	
	printf("\033[1mEXAMPLES:\033[0m\n");
	printf("  ./lemipc 1              # Join team 1\n");
	printf("  ./lemipc 2 -d           # Join team 2 with display\n");
	printf("  ./lemipc 3 --display    # Join team 3 with display\n\n");
	
	printf("\033[1mGAME RULES:\033[0m\n");
	printf("  • Players battle on a 10x10 board\n");
	printf("  • Goal: Be the last team standing\n");
	printf("  • Killed when surrounded by ≥2 enemies\n");
	printf("  • Teams: \033[31m1(Red)\033[0m \033[32m2(Green)\033[0m \033[33m3(Yellow)\033[0m \033[34m4(Blue)\033[0m\n\n");
}

void *display_thread(void *arg) {
	player_t *player = (player_t *)arg;
	
	while (!player->game_state->game_over) {
		display_board(player->game_state);
		usleep(1000000);
	}
	
	display_board(player->game_state);
	printf("Final board state displayed.\n");
	return NULL;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		display_usage();
		return 1;
	}
	
	// Check for help/version flags first
	int i;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			display_usage();
			return 0;
		} else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
			printf("Lem-IPC v1.0 - Inter-Process Communication Battle Game\n");
			printf("Built with System V IPC (shared memory, semaphores, message queues)\n");
			return 0;
		}
	}
	
	int team = atoi(argv[1]);
	if (team < 1 || team > MAX_TEAMS) {
		printf("Error: Team number must be between 1 and %d\n", MAX_TEAMS);
		return 1;
	}
	
	for (i = 2; i < argc; i++) {
		if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--display") == 0) {
			g_display_mode = 1;
		} else {
			printf("Unknown option: %s\n", argv[i]);
			display_usage();
			return 1;
		}
	}
	
	srand(time(NULL) + getpid());
	
	// Initialize player structure
	memset(&g_player, 0, sizeof(player_t));
	g_player.team = team;
	g_player.player_id = getpid() % (MAX_TEAMS * MAX_PLAYERS_PER_TEAM);
	
	setup_signal_handlers();
	
	printf("Player %d joining team %d...\n", g_player.player_id, g_player.team);
	
	init_ipc(&g_player);
	
	if (place_player(&g_player) == -1) {
		printf("Error: Could not place player on board (board full?)\n");
		cleanup_ipc(&g_player);
		return 1;
	}
	
	printf("Player %d placed at position (%d, %d)\n", 
		   g_player.player_id, g_player.pos.x, g_player.pos.y);
	
	if (g_display_mode) {
		pid_t display_pid = fork();
		if (display_pid == 0) {
			while (!g_player.game_state->game_over) {
				display_board(g_player.game_state);
				usleep(1000000);
			}
			exit(0);
		} else if (display_pid > 0) {
			player_game_loop(&g_player);
			kill(display_pid, SIGTERM);
			waitpid(display_pid, NULL, 0);
		} else {
			perror("fork for display");
			g_display_mode = 0;
		}
	}
	
	if (!g_display_mode) {
		player_game_loop(&g_player);
	}
	
	cleanup_ipc(&g_player);
	return 0;
}