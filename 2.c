#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdint.h>

#define MAX_N 15
#define BLOCK_SIZE_BYTES(N) (((1 << N) + 7) / 8)

int xorN(int fileCount, char *files[], int N) {
    size_t blockSizeBytes = BLOCK_SIZE_BYTES(N);
    uint8_t *blockMemory = (uint8_t *)calloc(blockSizeBytes, 1);
    uint8_t *resultMemory = (uint8_t *)calloc(blockSizeBytes, 1);
    if (blockMemory == NULL || resultMemory == NULL) {
        printf("Cannot allocate memory for block or result\n");
        free(blockMemory);
        free(resultMemory);
        return 0;
    }

    int fileIndex = 0;
    while (fileIndex < fileCount) {
        FILE *fileHandle = fopen(files[fileIndex], "rb");
        if (fileHandle == NULL) {
            printf("Unable to access file %s\n", files[fileIndex]);
            fileIndex++;
            continue;
        }

        int processedBlocks = 0;

        while (1) {
            size_t bytesRead = fread(blockMemory, 1, blockSizeBytes, fileHandle);
            if (bytesRead == 0) {
                break;
            }
            if (ferror(fileHandle)) {
                printf("File read error occurred in %s\n", files[fileIndex]);
                break;
            }
            if (bytesRead < blockSizeBytes) {
                memset(blockMemory + bytesRead, 0, blockSizeBytes - bytesRead);
            }
            if (N == 2) {
                uint8_t byte = blockMemory[0];
                uint8_t highNibble = (byte >> 4) & 0x0F;
                uint8_t lowNibble = byte & 0x0F;

                if (processedBlocks == 0) {
                    resultMemory[0] = highNibble;
                } else {
                    resultMemory[0] ^= highNibble;
                }
                processedBlocks++;

                if (processedBlocks == 1) {
                    resultMemory[0] = lowNibble;
                } else {
                    resultMemory[0] ^= lowNibble;
                }
                processedBlocks++;
            } else {
                if (processedBlocks == 0) {
                    memcpy(resultMemory, blockMemory, blockSizeBytes);
                } else {
                    for (size_t i = 0; i < blockSizeBytes; i++) {
                        resultMemory[i] ^= blockMemory[i];
                    }
                }
                processedBlocks++;
            }
        }

        if (processedBlocks == 0) {
            printf("No data in %s\n", files[fileIndex]);
        } else {
            printf("Computed XOR for %s: ", files[fileIndex]);
            if (N == 2) {
                printf("%01x\n", resultMemory[0] & 0x0F);
            } else {
                for (size_t i = 0; i < blockSizeBytes; i++) {
                    printf("%02x", resultMemory[i]);
                }
                printf("\n");
            }
        }

        fclose(fileHandle);
        fileIndex++;
    }

    free(blockMemory);
    free(resultMemory);
    return 0;
}

int count_mask_fits(int fileCount, char *files[], uint32_t mask) {
    int currFileIndex = 0;
    while (currFileIndex < fileCount) {
        FILE *file_p = fopen(files[currFileIndex], "rb");
        if (file_p == NULL) {
            printf("Could not open file");
            currFileIndex = currFileIndex + 1;
            continue;
        }

        int fits = 0;
        uint32_t value;

        printf("Checking file %s with mask: 0x%08X\n", files[currFileIndex], mask);

        while (fread(&value, sizeof(uint32_t), 1, file_p) == 1) {
            uint32_t maskedValue = value & mask;
            if (maskedValue == mask) {
                printf("Value: 0x%08X, Mask: 0x%08X\n", 
                    value, mask);
                fits = fits + 1;
            }
        }

        printf("found %d matches in %s\n", fits, files[currFileIndex]);
        fclose(file_p);
        currFileIndex = currFileIndex + 1;
    }

    return 0;
}

int copyN(int fileCount, char *files[], int N) {
    if (N > MAX_N) {
        printf("Too big N\n");
        return 0;
    }
    int totalProcesses = fileCount * N;
    pid_t *pids = (pid_t *)malloc(totalProcesses * sizeof(pid_t));
    if (!pids) {
        printf("PID memory allocation error");
        return 0;
    }
    int pidCount = 0;

    for (int fileIdx = 0; fileIdx < fileCount; fileIdx++) {
        for (int copyIdx = 0; copyIdx < N; copyIdx++) {
            pid_t pid = fork();
            if (pid == 0) {
                char newFilename[256];
                char *dot = strrchr(files[fileIdx], '.');
                if (dot) {
                    
                    size_t nameLength = dot - files[fileIdx];
                    strncpy(newFilename, files[fileIdx], nameLength);
                    newFilename[nameLength] = '\0';
                    snprintf(newFilename + nameLength, sizeof(newFilename) - nameLength, 
                            "_%d%s", copyIdx + 1, dot);
                } else {
                    
                    snprintf(newFilename, sizeof(newFilename), "%s_%d", 
                            files[fileIdx], copyIdx + 1);
                }
                
                FILE *source = fopen(files[fileIdx], "rb");
                if (!source) {
                    printf("Error opening source file: %s\n", files[fileIdx]);
                    return 0;
                }
                FILE *dest = fopen(newFilename, "wb");
                if (!dest) {
                    printf("Error opening dest file: %s\n", newFilename);
                    fclose(source);
                    return 0;
                }

                uint8_t buffer[4096];
                size_t bytes;
                while ((bytes = fread(buffer, 1, sizeof(buffer), source)) > 0) {
                    if (fwrite(buffer, 1, bytes, dest) != bytes) {
                        fclose(source);
                        fclose(dest);
                        return 0;
                    }
                }

                fclose(source);
                fclose(dest);
                return 1;
            } else if (pid > 0) {
                pids[pidCount++] = pid;
            }
        }
    }

    int remaining = pidCount;
    int failures = 0;
    while (remaining > 0) {
        for (int i = 0; i < pidCount; i++) {
            if (pids[i] > 0) {
                int status;
                pid_t result = waitpid(pids[i], &status, WNOHANG);
                if (result > 0) {
                    if (WIFEXITED(status) && WEXITSTATUS(status) != 1) {
                        failures++;
                    }
                    pids[i] = -1;
                    remaining--;
                }
            }
        }
        usleep(1000);
    }

    if (failures > 0) {
        printf("Some copy operations failed (%d failures)\n", failures);
    }
    free(pids);
    return 0;
}

int find_string_in_files(int fileCount, char *files[], const char *searchStr) {
    if (strlen(searchStr) == 0) {
        printf("Error: Empty search string provided.\n");
        return 0;
    }

    pid_t *pids = malloc(fileCount * sizeof(pid_t));
    if (!pids) {
        printf("Cannot allocate memory for process IDs\n");
        return 0;
    }
    int pidCount = 0;

    for (int i = 0; i < fileCount; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            FILE *file = fopen(files[i], "r");
            if (!file) {
                printf("Error opening file %s\n", files[i]);
                free(pids);
                return 1;
            }

            char *line = NULL;
            size_t len = 0;
            int found = 0;

            while (getline(&line, &len, file) != -1) {
                if (strstr(line, searchStr)) {
                    printf("Match located in: %s\n", files[i]);
                    found = 1;
                    break;
                }
            }

            free(line);
            fclose(file);
            free(pids);
            if (found) {
                return 0;
            } else {
                return 1;
            }
        } else if (pid < 0) {
            printf("Process creation failed\n");
        } else {
            pids[pidCount++] = pid;
        }
    }

    int remaining = pidCount;
    int found = 0;
    while (remaining > 0) {
        for (int i = 0; i < pidCount; i++) {
            if (pids[i] > 0) {
                int status;
                pid_t result = waitpid(pids[i], &status, WNOHANG);
                if (result > 0) {
                    if (WIFEXITED(status) && WEXITSTATUS(status) == 1) {
                        found = 1;
                    }
                    pids[i] = -1;
                    remaining--;
                }
            }
        }
        usleep(1000);
    }

    if (!found) {
        printf("No occurrences of '%s' found in the files.\n", searchStr);
    }
    free(pids);
    return 0;
}

int show_info() {
    printf("Usage: ./a.out <file1> <file2> ... <flag> <arg>\n");
    printf("Flags:\n");
    printf("xor<N> - XOR blocks of 2^N bits (N=2,3,4,5,6)\n");
    printf("mask <hex> - counting 4-byte integers matching the mask\n");
    printf("copy<N> - creating N copies of each file, numbering each copy\n");
    printf("find <string> - searches for a string in files\n");

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        show_info();
        return 1;
    }

    int fileCount = argc - 2;
    char *flag = argv[argc - 1];
    char *flagM = argv[argc - 2];
    char *arg = argv[argc - 1];
    int N;

    if (strstr(flag, "xor") == flag) {
        sscanf(flag, "xor%d", &N);
        if (N < 2 || N > 6) {
            printf("Error: N for xorN must be between 2 and 6.\n");
            return -1;
        }
        xorN(fileCount, argv + 1, N);
        return 1;
    } else if (strcmp(flagM, "mask") == 0) {
        if (argc < 4) {
            printf("Not enough arguments (must be 4)");
            return -1;
        }
        char *endptr;
        uint32_t mask = strtoul(arg, &endptr, 16);
        if (*endptr != '\0') {
            printf("Error: Invalid hexadecimal mask: %s\n", arg);
            return -1;
        }
        count_mask_fits(fileCount - 1, argv + 1, mask);
        return 1;
    } else if (strstr(flag, "copy") == flag) {
        sscanf(flag, "copy%d", &N);
        if (N <= 0) {
            printf("Error: N for copyN must be a positive number.\n");
            return -1;
        }
        copyN(fileCount, argv + 1, N);
        return 1;
    } else if (strcmp(flagM, "find") == 0) {
        if (argc < 4) {
            printf("Not enough arguments (must be 4)");
            return -1;
        }
        char * some_string = argv[argc - 1];
        int len = strlen(some_string) + 1;
        for (char * ptr = some_string; *ptr != '\0'; ++ptr) {
            if (*ptr == '\\') {
                switch (ptr[1])
                {
                case 'n':
                    memmove(ptr, &ptr[1], len - (ptr - some_string) - 1);
                    *ptr = '\n';
                    break;
                case 't':
                    memmove(ptr, &ptr[1], len - (ptr - some_string) - 1);
                    *ptr = '\t';
                    break;
                case '0':
                    memmove(ptr, &ptr[1], len - (ptr - some_string) - 1);
                    *ptr = '\0';
                    break;
                default:
                    break;
                }
            }
        }
        find_string_in_files(fileCount - 1, argv + 1, some_string);
        return 1;
    } else {
        printf("Unknown or absent flag\n");
        show_info();
        return -1;
    }

    return 0;
}