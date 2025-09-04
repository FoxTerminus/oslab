#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <getopt.h>
#include <testkit.h>
#include "labyrinth.h"

int main(int argc, char *argv[], char *envp[]) {
    // TODO: Implement this function

    int opt;
    char* map_file = NULL;
    char* player_id = NULL;
    char* move_direction = NULL;
    Labyrinth labyrinth;
    bool version_flag = false;

    static struct option long_options[] = {
        {"map", required_argument, 0, 'm'},
        {"player", required_argument, 0, 'p'},
        {"move", required_argument, 0, 'd'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}  
    };

    // 处理命令行指令
    while ((opt = getopt_long(argc, argv, "m:p:", long_options, 0))) {
        if (opt == '?') {
            printUsage();
            return 1;
        }
        if (opt == -1) {
            break;
        }

        switch (opt) {
            case 'm':
                map_file = optarg;
                break;
            case 'p':
                player_id = optarg;
                break;
            case 'd':
                move_direction = optarg;
                break;
            case 'v':
                version_flag = true;
                break;
            default:
                return 1;
        }
    }
    if (optind < argc) {
        printUsage();        
        return 1;
    }

    if (version_flag) {
        if (map_file == NULL && player_id == NULL && move_direction == NULL) {
            printf("Labyrinth Game.\n");
            return 0;
        }
        return 1;
    }

    if (map_file == NULL || player_id == NULL) {
        return 1;
    }

    // 检查玩家ID
    if ((strlen(player_id) != 1 || !isValidPlayer(*player_id))) {
        fprintf(stderr, "Invalid playerID\n");
        return 1;
    }

    //检查并载入地图
    if (!loadMap(&labyrinth, map_file)) {
        return 1;
    }
    if (!isConnected(&labyrinth)) {
        fprintf(stderr, "Map is not connected.\n");
        return 1;
    }

    // 玩家移动
    if (move_direction == NULL) {
        for (int i = 0; i < labyrinth.rows; i++) {
            for (int j = 0; j < labyrinth.cols; j++) {
                printf("%c", labyrinth.map[i][j]);
            }
            printf("\n");
        }
    } else if (move_direction != NULL && !movePlayer(&labyrinth, *player_id, move_direction)) {
        return 1;
    }


    // 保存地图
    if(!saveMap(&labyrinth, map_file)) {
        return 1;
    };
    return 0;
}

void printUsage() {
    printf("Usage:\n");
    printf("  labyrinth --map map.txt --player id\n");
    printf("  labyrinth -m map.txt -p id\n");
    printf("  labyrinth --map map.txt --player id --move direction\n");
    printf("  labyrinth --version\n");
}

bool isValidPlayer(char playerId) {
    // TODO: Implement this function
    if ('0' <= playerId && playerId <= '9') {
        return true;
    }
    return false;
}

bool loadMap(Labyrinth *labyrinth, const char *filename) {
    // TODO: Implement this function
    FILE* file = fopen(filename, "r");

    // 检查地图文件能否正常打开
    if (!file) {
        perror("Failed to open map file");
        return false;
    }

    // 载入地图
    int rows = 0;
    while (fgets(labyrinth->map[rows], sizeof(labyrinth->map[rows]), file)) {
        if (rows >= MAX_ROWS) {
            fprintf(stderr, "Map is too large.\n");
            fclose(file);
            return false;
        }
        labyrinth->map[rows][strcspn(labyrinth->map[rows], "\n")] = '\0';
        rows++;
    }
    fclose(file);

    // 检查地图大小
    if (rows == 0){
        fprintf(stderr, "Map is empty.\n");
        return false;
    }
    labyrinth->rows = rows;
    labyrinth->cols = strlen(labyrinth->map[0]);

    // 检查地图格式
    for (int i = 0; i < rows; i++) {
        if (strlen(labyrinth->map[i]) != labyrinth->cols) {
            fprintf(stderr, "Map format is not correct: rows have different lengths.\n");
            return false;
        }
        if (strlen(labyrinth->map[i]) > MAX_COLS) {
            fprintf(stderr, "Map is too large.\n");
            return false;
        }
        for (int j = 0; j < labyrinth->cols; j++) {
            char c = labyrinth->map[i][j];
            if (c != '#' && c != '.' && (c < '0' || c > '9')) {
                fprintf(stderr, "Map format is not correct: invalid character '%c'.\n", c);
                return false;
            }
        }
    }

    return true;
}

Position findPlayer(Labyrinth *labyrinth, char playerId) {
    // TODO: Implement this function
    Position pos = {-1, -1};
    for (int i = 0; i < labyrinth->rows; i++) {
        for (int j = 0; j < labyrinth->cols; j++) {
            if (labyrinth->map[i][j] == playerId) {
                pos.row = i;
                pos.col = j;
                // printf("%d %d\n", pos.row, pos.col);
                return pos;
            }
        }
    }    
    return pos;
}

Position findFirstEmptySpace(Labyrinth *labyrinth) {
    // TODO: Implement this function
    Position pos = {-1, -1};
    for (int i = 0; i < labyrinth->rows; i++) {
        for (int j = 0; j < labyrinth->cols; j++) {
            if (labyrinth->map[i][j] == '.') { // potential problems
                pos.row = i;
                pos.col = j;
                // printf("%d %d\n", pos.row, pos.col);
                return pos;
            }
        }
    }
    return pos;
}


Position findFirstNotWall(Labyrinth *labyrinth) {
    // TODO: Implement this function
    Position pos = {-1, -1};
    for (int i = 0; i < labyrinth->rows; i++) {
        for (int j = 0; j < labyrinth->cols; j++) {
            if (labyrinth->map[i][j] != '#') { // potential problems
                pos.row = i;
                pos.col = j;
                // printf("%d %d\n", pos.row, pos.col);
                return pos;
            }
        }
    }
    return pos;
}

bool isEmptySpace(Labyrinth *labyrinth, int row, int col) {
    // TODO: Implement this function
    if (0 <= row && row < labyrinth->rows && 0 <= col && col < labyrinth->cols && labyrinth->map[row][col] == '.') {
        return true;
    }
    return false;
}

bool movePlayer(Labyrinth *labyrinth, char playerId, const char *direction) {
    // TODO: Implement this function
    // 未找到玩家
    if (findPlayer(labyrinth, playerId).row == -1) {
        // printf("Player not found.\n");
        int new_row = findFirstEmptySpace(labyrinth).row;
        int new_col = findFirstEmptySpace(labyrinth).col;
        if (new_row == -1) {
            fprintf(stderr, "No empty space.\n");
            return false;
        }
        labyrinth->map[new_row][new_col] = playerId;
        return true;
    }

    // 确定方向
    Position pos = findPlayer(labyrinth, playerId);
    int new_row = pos.row;
    int new_col = pos.col;
    if (strcmp(direction, "up") == 0) {
         new_row--;
    }
    else if (strcmp(direction, "down") == 0) {
        new_row++;
    }
    else if (strcmp(direction,  "left") == 0) {
        new_col--;
    }
    else if (strcmp(direction, "right") == 0) {
        new_col++;
    }
    else {
        fprintf(stderr, "Invalid direction.\n");
        return false;
    }

    if (isEmptySpace(labyrinth, new_row, new_col)) {
        labyrinth->map[pos.row][pos.col] = '.';
        labyrinth->map[new_row][new_col] = playerId;
        return true;
    }
    fprintf(stderr, "Unsuccessful movement.\n");
    return false;
}

bool saveMap(Labyrinth *labyrinth, const char *filename) {
    // TODO: Implement this function
    FILE* file = fopen(filename, "w");
    if (!file) {
        perror("Failed to open map file");
        return false;
    }

    for (int i = 0; i < labyrinth->rows; i++) {
        for (int j = 0; j <= labyrinth->cols; j++) {
            if (j == labyrinth->cols) {
                fputc('\n', file);
            }
            else {
                fputc(labyrinth->map[i][j], file);
            }
        }
    }

    fclose(file);
    return true;
}

// Check if all empty spaces are connected using DFS
void dfs(Labyrinth *labyrinth, int row, int col, bool visited[MAX_ROWS][MAX_COLS]) {
    // TODO: Implement this function
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};

    visited[row][col] = true;

    for (int i = 0; i < 4; i++) {
        int new_row = row + dx[i];
        int new_col = col + dy[i];

        if (new_row >= 0 && new_row < labyrinth->rows && new_col >= 0 && new_col < labyrinth->cols && labyrinth->map[new_row][new_col] != '#' && !visited[new_row][new_col]) {
            dfs(labyrinth, new_row, new_col, visited);
        }
    }
}

bool isConnected(Labyrinth *labyrinth) {
    // TODO: Implement this function
    bool visited[MAX_ROWS][MAX_COLS] = {false};
    Position pos = findFirstNotWall(labyrinth);
    if (pos.row == -1) {
        return false;
    }
    
    dfs(labyrinth, pos.row, pos.col, visited);
    for (int i = 0; i < labyrinth->rows; i++) {
        for (int j = 0; j < labyrinth->cols; j++) {
            if (labyrinth->map[i][j] != '#' && !visited[i][j]) {
                return false;
            }
        }
    }
    return true;
}
