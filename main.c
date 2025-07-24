#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
// https://pubs.opengroup.org/onlinepubs/7908799/xsh/unistd.h.html
// The <unistd.h> header defines miscellaneous symbolic constants and types, and declares miscellaneous functions. The contents of this header are shown below.
#include <unistd.h>
#include <sys/ioctl.h>

// CUSTOM TYPES
typedef struct
{
    int x;
    int y;
} position;

typedef struct
{
    int armor;
    int health;
} game_info;

typedef struct
{
    position pos;
    int active;
    int x_direction;
    int y_direction;
} enemy;


// CONSTS
#define MAX_ENEMIES 25
#define FPS 20

// GLOBALS
struct termios original_terminal_settings;
enemy enemies[MAX_ENEMIES];

// ==========================
// GAME
// ==========================

// ==== TERMINAL
void restore_terminal(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &original_terminal_settings);
    printf("\033[?25h");
    printf("\033[0m");
    fflush(stdout);
}

void handle_exit(int signal_value)
{
    restore_terminal();
    exit(EXIT_SUCCESS);
}

void init_terminal(void)
{
    // canonical vs non-canonical mode: https://stackoverflow.com/questions/358342/canonical-vs-non-canonical-terminal-input
    // In noncanonical input processing mode, characters are not grouped into lines, and ERASE and KILL processing is not performed.
    // The granularity with which bytes are read in noncanonical input mode is controlled by the MIN and TIME settings.

    // switching to non-canonical mode and disable echo (show lines in terminal after input)
    tcgetattr(STDIN_FILENO, &original_terminal_settings);
    atexit(restore_terminal);
    signal(SIGINT, handle_exit);
    signal(SIGTERM, handle_exit);

    struct termios new_terminal_settings = original_terminal_settings;
    new_terminal_settings.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_terminal_settings);

    printf("\033[?25l");
}

// ==== HELPERS
void clear_screen(void)
{
    //printf("\033[2J");
    printf("\033[H");
}

void sleep_for_fps(int fps)
{
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1000000000L / fps;

    nanosleep(&ts, NULL);
}

int capture_keyboard_input(void)
{
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
}

void wait_for_keypress(void)
{
    fd_set set;
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    select(STDIN_FILENO + 1, &set, NULL, NULL, NULL);
    getchar();
}

// ==== DRAWERS
void draw_borders(const struct winsize* win_size)
{
    for (int row_index = 0; row_index < win_size->ws_row; row_index++)
    {
        for (int col_index = 0; col_index < win_size->ws_col; col_index++)
        {
            // TOP LEFT CORNER
            if (row_index == 0 && col_index == 0)
            {
                printf("┌");
            }

            // TOP RIGHT CORNER
            else if (row_index == 0 && col_index == win_size->ws_col - 1)
            {
                printf("┐");
            }

            // BOTTOM LEFT CORNER
            else if (row_index == win_size->ws_row - 1 && col_index == 0)
            {
                printf("└");
            }

            // BOTTOM RIGHT CORNER
            else if (row_index == win_size->ws_row - 1 && col_index == (win_size->ws_col - 1))
            {
                printf("┘");
            }

            // TOP AND BOTTOM OUTER LINE
            else if (row_index == 0 || row_index == win_size->ws_row - 1)
            {
                printf("-");
            }

            // LEFT AND RIGHT OUTER LINE
            else if (col_index == 0 || col_index == win_size->ws_col - 1)
            {
                printf("│");
            }
            else
            {
                printf(" ");
            }
        }
    }
}

void draw_player(const position* player_position)
{
    printf("\033[%i;%iH", player_position->y, player_position->x);
    printf("█");
}

void draw_info(const game_info* game_info)
{
    printf("\033[1;3H");
    printf("H: %i | A: %i", game_info->health, game_info->armor);
}

void draw_enemies(void)
{
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        printf("\033[%d;%dH<=>", enemies[i].pos.y, enemies[i].pos.x);
    }
}


// ==== GAME LOGIC
void move_player(position* player_position, char input, const struct winsize* win_size)
{
    if (input == 'w' && player_position->y > 1)
    {
        player_position->y--;
    }
    else if (input == 's' && player_position->y < win_size->ws_row - 2)
    {
        player_position->y++;
    }
    else if (input == 'a' && player_position->x > 1)
    {
        player_position->x--;
    }

    else if (input == 'd' && player_position->x < win_size->ws_col - 2)
    {
        player_position->x++;
    }
}

void move_enemies(const struct winsize* ws)
{
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (enemies[i].active)
        {
            int new_x = enemies[i].pos.x + enemies[i].x_direction;
            int new_y = enemies[i].pos.y + enemies[i].y_direction;

            if (new_x < 1 || new_x > ws->ws_col - 4)
            {
                enemies[i].x_direction *= -1;
            }

            else
            {
                enemies[i].pos.x = new_x;
            }


            if (new_y < 1 || new_y > ws->ws_row - 2)
            {
                enemies[i].y_direction *= -1;
            }
            else
            {
                enemies[i].pos.y = new_y;
            }
        }
    }
}

void spawn_enemies(const struct winsize* ws)
{
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (!enemies[i].active)
        {
            enemies[i].pos.x = (rand() % (ws->ws_col - 10)) + 2;
            enemies[i].pos.y = (rand() % (ws->ws_row - 4)) + 2;

            enemies[i].x_direction = (rand() % 3) - 1;
            enemies[i].y_direction = (rand() % 3) - 1;

            if (enemies[i].x_direction == 0 && enemies[i].y_direction == 0)
            {
                enemies[i].x_direction = 1;
            }

            enemies[i].active = 1;
            break;
        }
    }
}

int detect_collision(const position* player)
{
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (!enemies[i].active) continue;

        int enemy_x = enemies[i].pos.x;
        int enemy_y = enemies[i].pos.y;

        if (player->y == enemy_y && player->x >= enemy_x && player->x <= enemy_x + 2)
        {
            return i;
        }
    }
    return -1;
}

// ==== GAME SCREENS

void show_welcome_message(const struct winsize* win_size)
{
    printf("\033[%i;%iH", win_size->ws_row / 2, win_size->ws_col / 2);
    printf("WELCOME TO PANTRIX");

    printf("\033[%i;%iH", (win_size->ws_row / 2) + 2, (win_size->ws_col / 2) - 6);
    printf("press any key to start the game");
}

void show_game_over_message(const struct winsize* win_size)
{
    printf("\033[%i;%iH", win_size->ws_row / 2, win_size->ws_col / 2);
    printf("DU BIST EIN WECHSLER!!! \n TRENNT DEIN PFAND UND VERSUCH'S NOCHMAL");
}

// ==== GAME LOOP
void game_loop(const struct winsize* win_size)
{
    position player_position;
    player_position.x = win_size->ws_col - win_size->ws_col / 2;
    player_position.y = win_size->ws_row - win_size->ws_row / 2;

    game_info game_info;
    game_info.health = 100;
    game_info.armor = 100;

    clear_screen();

    int frame_counter = 0;

    while (1)
    {
        if (game_info.health <= 0)
        {
            clear_screen();
            printf("\033[2J");
            show_game_over_message(win_size);
            fflush(stdout);
            sleep(5);
            break;
        }

        if (capture_keyboard_input())
        {
            char c = getchar();
            if (c == 'q')
            {
                break;
            }

            move_player(&player_position, c, win_size);
        }

        spawn_enemies(win_size);

        clear_screen();
        draw_borders(win_size);
        draw_info(&game_info);

        if (frame_counter % 2 == 0)
        {
            move_enemies(win_size);
        }

        draw_enemies();

        int is_collided_with_enemy = detect_collision(&player_position);
        if (is_collided_with_enemy != -1)
        {
            game_info.health -= 50;
            enemies[is_collided_with_enemy].active = 0;
        }


        draw_player(&player_position);

        fflush(stdout);
        sleep_for_fps(FPS);

        if (frame_counter > 1000)
        {
            frame_counter = 0;
        }

        frame_counter++;
    }
}


int main()
{
    srand(time(NULL));
    init_terminal();

    struct winsize win_size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &win_size);

    clear_screen();
    draw_borders(&win_size);
    show_welcome_message(&win_size);
    fflush(stdout);

    wait_for_keypress();

    game_loop(&win_size);

    return EXIT_SUCCESS;
}
