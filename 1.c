#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>

#define MAX_USERS 1000
#define LOGIN 6
#define SANCTION "12345"

typedef struct {
    char login[LOGIN + 1];
    long pin;
    int sanctionLimit;
} User;

typedef struct {
    User *users[MAX_USERS];
    int count;
    char dbFilePath[256];
} UserDatabase;

int menu() {
    printf("\nCommand list:\n");
    printf("  Time - display current time\n");
    printf("  Date - display current date\n");
    printf("  Howmuch <dd.mm.yyyy> <flag: -s -m -h -y> - calculate passed time since a date\n");
    printf("  Sanctions <username> <number> - set restrictions for a user\n");
    printf("  Logout - exit to login menu\n");

    return 0;
}

long hash_pin(long pin) {
    long hash = 5381;
    long temp = pin;
    
    while (temp != 0) {
        int digit = temp % 10;
        hash = ((hash << 5) + hash) + digit;
        temp /= 10;
    }
    
    hash = (hash >> 16) ^ (hash & 0xFFFF);
    hash = hash ^ (hash >> 16);
    
    return hash & 0x7FFFFFFF;
}

int check_if_year_is_leap(int year) {
    if (year % 400 == 0) return 1;
    if (year % 100 == 0) return 0;
    if (year % 4 == 0) return 1;
    return 0;
}

int howmuch_days_in_month(int month, int year) {
    if (month == 2) {
        if (check_if_year_is_leap(year)) return 29;
        return 28;
    }
    if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
    return 31;
}

int verify_login(const char *login) {
    int i = 0;
    while (login[i] != '\0') {
        if (!isalnum(login[i])) return -1;
        i++;
    }
    if (i == 0 || i > LOGIN) return -1;
    return 1;
}

int db_init(UserDatabase* db, const char* filePath) {
    if (db == NULL) {
        printf("Error: Invalid database pointer provided.\n");
        return 0;
    }
    if (filePath == NULL) {
        printf("Error: Invalid file path provided.\n");
        return 0;
    }
    if (strlen(filePath) >= 256) {
        printf("Error: File path exceeds maximum length of 255 characters.\n");
        return 0;
    }

    db->count = 0;
    int i;
    for (i = 0; i < MAX_USERS; i++) {
        db->users[i] = NULL;
    }
    strcpy(db->dbFilePath, filePath);
    return 1;
}

User* locate_user(const UserDatabase* db, const char *login) {
    int i = 0;
    while (i < db->count) {
        if (strcmp(db->users[i]->login, login) == 0) return db->users[i];
        i++;
    }
    return NULL;
}

int current_time() {
    time_t t;
    time(&t);
    struct tm *tm = localtime(&t);
    if (tm) {
        printf("Current time: %02d:%02d:%02d\n", tm->tm_hour, tm->tm_min, tm->tm_sec);
    } else {
        printf("Failed to get current time.\n");
    }

    return 0;
}

int current_date() {
    time_t t;
    time(&t);
    struct tm *tm = localtime(&t);
    if (tm) {
        printf("Current date: %02d.%02d.%04d\n", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
    } else {
        printf("Failed to get current date.\n");
    }

    return 0;
}


int check_time_input(const char *date, int *day, int *month, int *year, const char *flag) {
    if (flag[0] != '-' || 
    (flag[1] != 's' && flag[1] != 'm' && flag[1] != 'h' && flag[1] != 'y') || 
    flag[2] != '\0') return 0;
    
    int d = 0, m = 0, y = 0;
    int i = 0, part = 0;
    while (date[i] != '\0') {
        if (date[i] == '.') {
            part++;
            i++;
            continue;
        }
        if (part == 0) d = d * 10 + (date[i] - '0');
        else if (part == 1) m = m * 10 + (date[i] - '0');
        else if (part == 2) y = y * 10 + (date[i] - '0');
        i++;
    }

    if (m < 1 || m > 12 || y < 1900) return 0;
    if (d < 1 || d > howmuch_days_in_month(m, y)) return 0;

    *day = d;
    *month = m;
    *year = y;
    
    return 1;
}

int calculate_passed_time(int day, int month, int year, const char *flag) {
    struct tm input = {0};
    input.tm_mday = day;
    input.tm_mon = month - 1;
    input.tm_year = year - 1900;

    time_t past = mktime(&input);
    if (past == -1) {
        printf("Failed to process the date.\n");
        return 0 ;
    }

    time_t now;
    time(&now);
    double diff = difftime(now, past);
    if (diff < 0) {
        printf("Error: Date is in the future.\n");
        return 0 ;
    }

    if (flag[1] == 's') printf("Time passed: %.0f seconds\n", diff);
    else if (flag[1] == 'm') printf("Time passed: %.0f minutes\n", diff / 60);
    else if (flag[1] == 'h') printf("Time passed: %.0f hours\n", diff / 3600);
    else if (flag[1] == 'y') printf("Time passed: %.2f years\n", diff / (3600 * 24 * 365.25));

    return 0;
}

int store_users(const UserDatabase* db) {
    if (db == NULL) {
        printf("Error: Invalid database pointer provided.\n");
        return 0;
    }

    FILE* file = fopen(db->dbFilePath, "w");
    if (!file) {
        printf("Error opening file for saving user data");
        return 0;
    }
    
    fprintf(file, "# Users Database\n");
    fprintf(file, "# Format: login,pin,sanctionLimit\n");
    
    int i = 0;
    while (i < db->count) {
        if (db->users[i] == NULL) {
            printf("Error: Null user entry found at index %d.\n", i);
            fclose(file);
            return 0;
        }
        fprintf(file, "%s,%ld,%d\n", 
                db->users[i]->login,
                db->users[i]->pin,
                db->users[i]->sanctionLimit);
        i++;
    }
    fclose(file);
    return 1;
}

int how_much(int day, int month, int year, const char *flag) {
    struct tm input = {0};
    input.tm_mday = day;
    input.tm_mon = month - 1;
    input.tm_year = year - 1900;

    time_t past = mktime(&input);
    if (past == -1) {
        printf("Failed to process the date.\n");
        return 0 ;
    }

    time_t now;
    time(&now);
    double diff = difftime(now, past);
    if (diff < 0) {
        printf("Error: Date is in the future.\n");
        return 0 ;
    }

    if (flag[1] == 's') printf("Time passed: %.0f seconds\n", diff);
    else if (flag[1] == 'm') printf("Time passed: %.0f minutes\n", diff / 60);
    else if (flag[1] == 'h') printf("Time passed: %.0f hours\n", diff / 3600);
    else if (flag[1] == 'y') printf("Time passed: %.2f years\n", diff / (3600 * 24 * 365.25));

    return 0;
}

int handle_time_input(const char* date, const char* flag) {
    int day, month, year;
    if (!check_time_input(date, &day, &month, &year, flag)) {
        printf("Invalid date or unit (-s, -m, -h, or -y).\n");
        return 0;
    }

    how_much(day, month, year, flag);

    return 0;
}

int check_restriction_input(const char *username, const char *limitStr, const char *confirm) {
    if (verify_login(username) == -1) return 0;
    int limit = atoi(limitStr);
    if (limit < 0 || strcmp(confirm, SANCTION) != 0) return 0;
    return 1;
}

int set_restrictions(UserDatabase* db, const char *username, int limit) {
    User *targetUser = locate_user(db, username);
    if (!targetUser) {
        printf("User not found.\n");
        return 0;
    }
    targetUser->sanctionLimit = limit;
    if (!store_users(db)) {
        printf("Error: Failed to save restriction changes.\n");
        targetUser->sanctionLimit = -1; 
        return 0;
    }
    printf("Restrictions set successfully!\n");

    return 0;
}



int handle_restriction_input(UserDatabase* db, const char* username, const char* number) {
    char confirm[10];

    printf("Enter confirmation code: ");
    if (!fgets(confirm, sizeof(confirm), stdin)) return 0;
    confirm[strcspn(confirm, "\n")] = '\0';

    if (!check_restriction_input(username, number, confirm)) {
        printf("Invalid input or wrong confirmation code.\n");
        return 0;
    }

    int limit = atoi(number);
    set_restrictions(db, username, limit);

    return 0;
}

int fetch_users(UserDatabase* db) {
    FILE* file = fopen(db->dbFilePath, "r");
    if (!file) return 0;

    char line[256];
    while (db->count < MAX_USERS && fgets(line, sizeof(line), file)) {
        if (line[0] == '#') continue;
        
        User* user = malloc(sizeof(User));
        if (!user) {
            printf("Failed to allocate memory.\n");
            break;
        }
        
        if (sscanf(line, "%6[^,],%ld,%d", 
                  user->login, 
                  &user->pin, 
                  &user->sanctionLimit) != 3) {
            free(user);
            continue;
        }
        
        if (verify_login(user->login) == -1) {
            free(user);
            continue;
        }
        
        db->users[db->count] = user;
        db->count++;
    }
    fclose(file);
    return 1;
}

int db_clean(UserDatabase* db) {
    int i = 0;
    while (i < db->count) {
        free(db->users[i]);
        i++;
    }
    db->count = 0;

    return 0;
}

int add_user(UserDatabase* db, const char *login, long int pin) {
    if (db->count >= MAX_USERS) {
        printf("User limit reached.\n");
        return 0;
    }
    if (verify_login(login) == -1) {
        printf("Error: Login must be 1-6 alphanumeric characters.\n");
        return 0;
    }
    if (locate_user(db, login)) {
        printf("This login is already in use!\n");
        return 0;
    }

    User *newUser = (User *)malloc(sizeof(User));
    if (!newUser) {
        printf("Failed to allocate memory.\n");
        return 0;
    }
    strcpy(newUser->login, login);
    newUser->pin = hash_pin(pin);
    newUser->sanctionLimit = -1;

    db->users[db->count] = newUser;
    db->count++;
    
    if (!store_users(db)) {
        printf("Error: Failed to save new user to database.\n");
        free(newUser);
        db->users[db->count - 1] = NULL;
        db->count--;
        return 0;
    }
    printf("Registration complete!\n");
    return 1;
}

int user_session(UserDatabase* db, const User* currentUser) {
    int sessionCommandCount = 0;

    while (1) {
        menu();
        char command[50];
        printf("\nEnter command: ");
        if (!fgets(command, sizeof(command), stdin)) {
            printf("Session ended with EOF.\n");
            break;
        }
        command[strcspn(command, "\n")] = '\0';

        if (strcmp(command, "Logout") == 0) {
            printf("Signing out.\n");
            return 0;
        }

        if (currentUser->sanctionLimit != -1 && sessionCommandCount >= currentUser->sanctionLimit) {
            printf("Limit reached. Only Logout is available.\n");
            continue;
        }

        if (strcmp(command, "Time") == 0) {
            current_time();
            sessionCommandCount++;
        }
        else if (strcmp(command, "Date") == 0) {
            current_date();
            sessionCommandCount++;
        }
        else if (strstr(command, "Howmuch") == command) {
            char date[32];
            char flag[10];
            sscanf(command, "Howmuch %s %s", date, flag);
            handle_time_input(date, flag);
            sessionCommandCount++;
        }
        else if (strstr(command, "Sanctions") == command) {
            char username[LOGIN + 2], number[10];
            sscanf(command, "Sanctions %s %s", username, number);
            handle_restriction_input(db, username, number);
            sessionCommandCount++;
        }
        else {
            printf("Error: Unknown command.\n");
        }
    }

    return 0;
}

int login_menu(UserDatabase* db) {
    fetch_users(db);

    while (1) {
        printf("\n1. Sign in\n2. Sign up\n3. Quit\nSelect: ");
        char choiceStr[10];
        if (!fgets(choiceStr, sizeof(choiceStr), stdin)) {
            printf("Terminated with EOF.\n");
            break;
        }
        choiceStr[strcspn(choiceStr, "\n")] = '\0';

        char *endptr;
        long choice = strtol(choiceStr, &endptr, 10);
        if (*endptr != '\0' || choice < 1 || choice > 3) {
            printf("Invalid selection. Use 1-3.\n");
            continue;
        }
        if (choice == 3) {
            printf("Quiting...\n");
            break;
        }

        char login[LOGIN + 2];
        printf("Login (1-6 chars): ");
        if (!fgets(login, sizeof(login), stdin)) continue;
        login[strcspn(login, "\n")] = '\0';

        if (verify_login(login) == -1) {
            printf("Ivalid input: Login must be 1-6 alphanumeric chars.\n");
            continue;
        }

        printf("PIN (0-100000): ");
        char pinStr[10];
        if (!fgets(pinStr, sizeof(pinStr), stdin)) continue;
        pinStr[strcspn(pinStr, "\n")] = '\0';

        char *pinEndPtr;
        long pin = strtol(pinStr, &pinEndPtr, 10);
        if (*pinEndPtr != '\0' || pin < 0 || pin > 100000) {
            printf("Invalid input: PIN must be between 0 and 100000.\n");
            continue;
        }

        if (choice == 1) {
            User *user = locate_user(db, login);
            if (!user || user->pin != hash_pin(pin)) { 
                printf("Wrong login or PIN!\n");
                continue;
            }
            printf("User (%s) authorized\n", login);
            user_session(db, user);
        } else {
            if (locate_user(db, login)) {
                printf("Login already exists!\n");
                continue;
            }
            if (add_user(db, login, pin)) {
                user_session(db, db->users[db->count - 1]);
            }
        }
    }
    if (!store_users(db)) {
        printf("Warning: Failed to save user data on exit.\n");
    }

    return 0;
}

int main() {
    UserDatabase db;
    if (!db_init(&db, "users.txt")) {
        printf("Failed to initialize database. Exiting.\n");
        return 1;
    }
    int running = 1;

    while (running) {
        printf("\nMain Menu:\n");
        printf("  1. Authorization\n");
        printf("  2. Exit\n");
        printf("Choose an option: ");

        char choiceStr[10];
        if (!fgets(choiceStr, sizeof(choiceStr), stdin)) {
            printf("Program terminated with EOF.\n");
            break;
        }
        choiceStr[strcspn(choiceStr, "\n")] = '\0';

        char *endptr;
        long choice = strtol(choiceStr, &endptr, 10);
        if (*endptr != '\0' || choice < 1 || choice > 2) {
            printf("Please select a valid option (1-2).\n");
            continue;
        }

        switch (choice) {
            case 1:
                login_menu(&db);
                break;
            case 2:
                printf("Exiting...\n");
                running = 0;
                break;
        }
    }

    db_clean(&db);
    return 0;
}