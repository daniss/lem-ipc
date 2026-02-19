NAME = lemipc
VIEWER_NAME = lemipc_viewer

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99 -g
LDFLAGS = 
SDL2_CFLAGS = $(shell sdl2-config --cflags)
SDL2_LIBS = $(shell sdl2-config --libs)

SRCDIR = src
INCDIR = include
OBJDIR = obj

SOURCES = main.c ipc.c board.c player.c
OBJS = $(addprefix $(OBJDIR)/, $(SOURCES:.c=.o))
VIEWER_SOURCE = viewer.c
VIEWER_OBJ = $(OBJDIR)/viewer.o

INCLUDES = -I$(INCDIR)

all: $(OBJDIR) $(NAME)

bonus: $(OBJDIR) $(VIEWER_NAME)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(NAME): $(OBJS)
	$(CC) $(OBJS) -o $(NAME) $(LDFLAGS)

$(VIEWER_NAME): $(VIEWER_OBJ)
	$(CC) $(VIEWER_OBJ) -o $(VIEWER_NAME) $(SDL2_LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(VIEWER_OBJ): $(SRCDIR)/$(VIEWER_SOURCE)
	$(CC) $(CFLAGS) $(INCLUDES) $(SDL2_CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME) $(VIEWER_NAME)

re: fclean all

.PHONY: all bonus clean fclean re