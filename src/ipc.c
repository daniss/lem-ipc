#include "game.h"

void sem_lock(int sem_id, int sem_num) {
	struct sembuf sb = {sem_num, -1, 0};
	if (semop(sem_id, &sb, 1) == -1) {
		perror("semop lock");
		exit(EXIT_FAILURE);
	}
}

void sem_unlock(int sem_id, int sem_num) {
	struct sembuf sb = {sem_num, 1, 0};
	if (semop(sem_id, &sb, 1) == -1) {
		perror("semop unlock");
		exit(EXIT_FAILURE);
	}
}

static int create_semaphore(key_t key) {
	int sem_id = semget(key, SEM_COUNT, IPC_CREAT | IPC_EXCL | 0666);
	if (sem_id != -1) {
		union semun {
			int val;
			struct semid_ds *buf;
			unsigned short *array;
		} sem_union;
		
		sem_union.val = 1;
		if (semctl(sem_id, SEM_BOARD, SETVAL, sem_union) == -1) {
			perror("semctl init");
			exit(EXIT_FAILURE);
		}
	} else if (errno == EEXIST) {
		sem_id = semget(key, SEM_COUNT, 0666);
		if (sem_id == -1) {
			perror("semget existing");
			exit(EXIT_FAILURE);
		}
	} else {
		perror("semget create");
		exit(EXIT_FAILURE);
	}
	return sem_id;
}

static int create_shared_memory(key_t key) {
	int shm_id = shmget(key, sizeof(game_state_t), IPC_CREAT | IPC_EXCL | 0666);
	if (shm_id == -1 && errno == EEXIST) {
		shm_id = shmget(key, sizeof(game_state_t), 0666);
	}
	if (shm_id == -1) {
		perror("shmget");
		exit(EXIT_FAILURE);
	}
	return shm_id;
}

static int create_message_queue(key_t key) {
	int msg_id = msgget(key, IPC_CREAT | IPC_EXCL | 0666);
	if (msg_id == -1 && errno == EEXIST) {
		msg_id = msgget(key, 0666);
	}
	if (msg_id == -1) {
		perror("msgget");
		exit(EXIT_FAILURE);
	}
	return msg_id;
}

void init_ipc(player_t *player) {
	int is_first_player = 0;
	
	player->shm_id = shmget(SHM_KEY, sizeof(game_state_t), 0666);
	if (player->shm_id == -1) {
		is_first_player = 1;
		player->shm_id = create_shared_memory(SHM_KEY);
	}
	
	player->game_state = shmat(player->shm_id, NULL, 0);
	if (player->game_state == (void *)-1) {
		perror("shmat");
		exit(EXIT_FAILURE);
	}
	
	player->msg_id = create_message_queue(MSG_KEY);
	player->sem_id = create_semaphore(SEM_KEY);
	
	if (is_first_player) {
		sem_lock(player->sem_id, SEM_BOARD);
		init_board(player->game_state);
		sem_unlock(player->sem_id, SEM_BOARD);
	}
}

void cleanup_ipc(player_t *player) {
	if (player->game_state == NULL) {
		return; // Already cleaned up
	}
	
	// Use a timeout for semaphore operations to avoid hanging
	struct timespec timeout;
	clock_gettime(CLOCK_REALTIME, &timeout);
	timeout.tv_sec += 5; // 5 second timeout
	
	struct sembuf sb = {SEM_BOARD, -1, 0};
	if (semtimedop(player->sem_id, &sb, 1, &timeout) == 0) {
		// Don't decrement again - remove_player already did this!
		int remaining_players = player->game_state->player_count;
		
		struct sembuf sb_unlock = {SEM_BOARD, 1, 0};
		semop(player->sem_id, &sb_unlock, 1);
		
		if (shmdt(player->game_state) == -1) {
			perror("shmdt");
		}
		player->game_state = NULL;
		
		if (remaining_players == 0) {
			// Small delay to ensure other processes have detached
			usleep(100000); // 100ms
			
			if (shmctl(player->shm_id, IPC_RMID, NULL) == -1) {
				perror("shmctl remove");
			}
			if (msgctl(player->msg_id, IPC_RMID, NULL) == -1) {
				perror("msgctl remove");
			}
			if (semctl(player->sem_id, 0, IPC_RMID) == -1) {
				perror("semctl remove");
			}
		}
	} else {
		// Timeout or error - force cleanup if we're likely the last process
		if (shmdt(player->game_state) == -1) {
			perror("shmdt");
		}
		player->game_state = NULL;
		
		// Attempt cleanup anyway - if it fails, resources might be already cleaned
		shmctl(player->shm_id, IPC_RMID, NULL);
		msgctl(player->msg_id, IPC_RMID, NULL);
		semctl(player->sem_id, 0, IPC_RMID);
	}
}