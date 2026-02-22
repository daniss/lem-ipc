#include "game.h"

static player_t g_player;
static int g_display_mode = 0;

void signal_handler(int sig) {
	printf("\nReceived signal %d, cleaning up...\n", sig);
	if (g_player.game_state != NULL) {
		remove_player(&g_player);
		cleanup_ipc(&g_player);
	}
	exit(0);
}

void setup_signal_handlers(void) {
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGQUIT, signal_handler);
}

void display_usage(void) {
	printf("╔══════════════════════════════════╗\n");
	printf("║             Lem-IPC              ║\n");
	printf("╚══════════════════════════════════╝\n\n");
	
	printf("USAGE:\n");
	printf("  ./lemipc <team_number> [options]\n\n");
	
	printf("ARGUMENTS:\n");
	printf("  team_number    Team number (> 0)\n\n");
	
	printf("OPTIONS:\n");
	printf("  -d, --display  Enable real-time board display\n");
	printf("  -h, --help     Show this help message\n");
	printf("  -v, --version  Show version information\n\n");
	
	printf("EXAMPLES:\n");
	printf("  ./lemipc 1              # Join team 1\n");
	printf("  ./lemipc 2 -d           # Join team 2 with display\n");
	printf("  ./lemipc 3 --display    # Join team 3 with display\n\n");
	
	printf("GAME RULES:\n");
	printf("  • Players battle on a 10x10 board\n");
	printf("  • Goal: Be the last team standing\n");
	printf("  • Killed when surrounded by ≥2 enemies\n");
	printf("  • Team numbers are chosen at launch\n\n");
}


int main(int argc, char **argv) {
	if (argc < 2) {
		display_usage();
		return 1;
	}
	
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
	if (team <= 0) {
		printf("Error: Team number must be > 0\n");
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
	
	memset(&g_player, 0, sizeof(player_t));
	g_player.team = team;
	g_player.pid = getpid();
	g_player.on_board = 0;
	g_player.pos.x = -1;
	g_player.pos.y = -1;
	
	setup_signal_handlers();
	
	printf("Player %d joining team %d...\n", g_player.pid, g_player.team);
	
	init_ipc(&g_player);
	
	if (place_player(&g_player) == -1) {
		printf("Error: Could not place player on board (board full?)\n");
		cleanup_ipc(&g_player);
		return 1;
	}
	
	printf("Player %d placed at position (%d, %d)\n",
		   g_player.pid, g_player.pos.x, g_player.pos.y);

	player_game_loop(&g_player, g_display_mode);

	cleanup_ipc(&g_player);
	return 0;
}