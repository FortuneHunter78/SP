#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#define COLOR_RESET   "\x1b[0m"
#define COLOR_BLUE    "\x1b[34m"   // Синий - для директорий
#define COLOR_GREEN   "\x1b[32m"   // Зеленый - для сокетов
#define COLOR_CYAN    "\x1b[36m"   // Голубой - для символических ссылок
#define COLOR_RED     "\x1b[31m"   // Красный - для исполняемых файлов
#define COLOR_YELLOW  "\x1b[33m"   // Желтый - для именованных каналов (FIFO)
#define COLOR_MAGENTA "\x1b[35m"   // Пурпурный - для блочных и символьных устройств
#define COLOR_WHITE   "\x1b[37m"   // Белый - для обычных файлов

int determine_permissions(char *perm_string, struct stat *stats) {
    int temporary_mode;
    char temp_char;

    
    if (perm_string == NULL) {
        fprintf(stderr, "Error in determine_permissions: Invalid input received\n");
        return 1;
    }
    if (stats == NULL) {
        fprintf(stderr, "Error in determine_permissions: Invalid input received\n");
        return 1;
    }

    
    temporary_mode = stats->st_mode;

    
    if (S_ISDIR(temporary_mode) == 1) {
        temp_char = 'd';
        perm_string[0] = temp_char;
    }
    else if (S_ISLNK(temporary_mode) == 1) {
        temp_char = 'l';
        perm_string[0] = temp_char;
    }
    else if (S_ISFIFO(temporary_mode) == 1) {
        temp_char = 'p';
        perm_string[0] = temp_char;
    }
    else if (S_ISCHR(temporary_mode) == 1) {
        temp_char = 'c';
        perm_string[0] = temp_char;
    }
    else if (S_ISBLK(temporary_mode) == 1) {
        temp_char = 'b';
        perm_string[0] = temp_char;
    }
    else if (S_ISSOCK(temporary_mode) == 1) {
        temp_char = 's';
        perm_string[0] = temp_char;
    }
    else {
        temp_char = '-';
        perm_string[0] = temp_char;
    }

        if ((temporary_mode & S_IRUSR) != 0) {
        perm_string[1] = 'r';
    } else {
        perm_string[1] = '-';
    }
    if ((temporary_mode & S_IWUSR) != 0) {
        perm_string[2] = 'w';
    } else {
        perm_string[2] = '-';
    }
    if ((temporary_mode & S_IXUSR) != 0) {
        perm_string[3] = 'x';
    } else {
        perm_string[3] = '-';
    }

    
    if ((temporary_mode & S_IRGRP) != 0) {
        perm_string[4] = 'r';
    } else {
        perm_string[4] = '-';
    }
    if ((temporary_mode & S_IWGRP) != 0) {
        perm_string[5] = 'w';
    } else {
        perm_string[5] = '-';
    }
    if ((temporary_mode & S_IXGRP) != 0) {
        perm_string[6] = 'x';
    } else {
        perm_string[6] = '-';
    }

    
    if ((temporary_mode & S_IROTH) != 0) {
        perm_string[7] = 'r';
    } else {
        perm_string[7] = '-';
    }
    if ((temporary_mode & S_IWOTH) != 0) {
        perm_string[8] = 'w';
    } else {
        perm_string[8] = '-';
    }
    if ((temporary_mode & S_IXOTH) != 0) {
        perm_string[9] = 'x';
    } else {
        perm_string[9] = '-';
    }

    
    perm_string[10] = '\0';

    return 0;
}

int format_time_string(time_t mod_time, char *time_output) {
    struct tm *time_data;
    time_t current_time;
    double time_difference;
    char temporary_format[20];
    int months_threshold;

    
    time_data = localtime(&mod_time);
    if (time_data == NULL) {
        strcpy(time_output, "Wrong time label");
        return 0;
    }

    
    current_time = time(NULL);
    if (current_time == (time_t)-1) {
        strcpy(time_output, "Failed getting time");
        return 0;
    }

    
    time_difference = difftime(current_time, mod_time);

    
    months_threshold = 6 * 30 * 24 * 60 * 60;

    
    if (time_difference > months_threshold) {
        strcpy(temporary_format, "%b %d  %Y");
    } else {
        strcpy(temporary_format, "%b %d %H:%M");
    }

    
    strftime(time_output, 20, temporary_format, time_data);

    return 0;
}

int get_disk_block(const char *file_path, struct stat *stats) {
    int file_handle;
    int block_number;
    int result_of_ioctl;

    
    if (S_ISREG(stats->st_mode) == 0) {
        if (S_ISDIR(stats->st_mode) == 0) {
            return -1;
        }
    }

    
    file_handle = open(file_path, O_RDONLY);
    if (file_handle < 0) {
        return -1;
    }

    
    block_number = 0;

    
    result_of_ioctl = ioctl(file_handle, FIBMAP, &block_number);
    if (result_of_ioctl != 0) {
        close(file_handle);
        return -1;
    }

    
    close(file_handle);

    return block_number;
}

int process_file(const char *file_path) {
    struct stat file_stats;

    if (!file_path) {
        fprintf(stderr, "Error in process_file: empty path\n");
        return 1;
    }

    if (stat(file_path, &file_stats) != 0) {
        fprintf(stderr, "Error in process_file: failed collecting data about file\n");
        return 1;
    }

    char permissions[11];
    if (determine_permissions(permissions, &file_stats) != 0) {
        return 1;
    }

    struct passwd *owner = getpwuid(file_stats.st_uid);
    struct group *group = getgrgid(file_stats.st_gid);
    char *owner_name = owner ? owner->pw_name : "unknown";
    char *group_name = group ? group->gr_name : "unknown";

    char time_buffer[20];
    format_time_string(file_stats.st_mtime, time_buffer);

    int block_number = get_disk_block(file_path, &file_stats);

    
    const char *color = COLOR_RESET;
    if (S_ISDIR(file_stats.st_mode)) {
        color = COLOR_BLUE;
    } else if (S_ISLNK(file_stats.st_mode)) {
        color = COLOR_CYAN;
    } else if (S_ISFIFO(file_stats.st_mode)) {
        color = COLOR_YELLOW;
    } else if (S_ISCHR(file_stats.st_mode)) {
        color = COLOR_MAGENTA;
    } else if (S_ISBLK(file_stats.st_mode)) {
        color = COLOR_MAGENTA;
    } else if (S_ISSOCK(file_stats.st_mode)) {
        color = COLOR_GREEN;
    } else if (file_stats.st_mode & S_IXUSR) {
        color = COLOR_RED;
    }

    printf("%s %2lu %s %s %6lu %s %d %s%s%s\n",
           permissions, (unsigned long)file_stats.st_nlink, owner_name, group_name,
           (unsigned long)file_stats.st_ino, time_buffer, block_number, color, file_path, COLOR_RESET);

    return 0;
}

int explore_directory(const char *folder_path) {
    if (!folder_path) {
        fprintf(stderr, "Error in explore_directory: wrong path\n");
        return 1;
    }

    DIR *directory = opendir(folder_path);
    if (!directory) {
        fprintf(stderr, "Error in explore_directory: access to directory failed\n");
        return 1;
    }

    printf("Directory content: %s\n", folder_path);
    struct dirent *entry;
    char *full_path = NULL;

    while ((entry = readdir(directory)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
            continue;
        }

        size_t path_len = strlen(folder_path) + strlen(entry->d_name) + 2;
        full_path = realloc(full_path, path_len);
        
        if (!full_path) {
            fprintf(stderr, "Error: memory allocation for path failed\n");
            closedir(directory);
            return 1;
        }

        
        snprintf(full_path, path_len, "%s/%s", folder_path, entry->d_name);
        
        if (process_file(full_path) != 0) {
            free(full_path);
            closedir(directory);
            return 1;
        }
    }

    
    free(full_path);
    closedir(directory);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <dir1> [dir2 ...]\n", argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (explore_directory(argv[i]) != 0) {
            return 1;
        }
        if (i < argc - 1) {
            printf("\n");
        }
    }

    return 0;
}