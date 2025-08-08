NAME = lemipc

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99 -g
LDFLAGS = 

SRCDIR = src
INCDIR = include
OBJDIR = obj

SOURCES = main.c ipc.c board.c player.c
OBJS = $(addprefix $(OBJDIR)/, $(SOURCES:.c=.o))

INCLUDES = -I$(INCDIR)

all: $(OBJDIR) $(NAME)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(NAME): $(OBJS)
	$(CC) $(OBJS) -o $(NAME) $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re