#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

// Stores the word currently being read
// The buffer can grow if the word is too long
typedef struct {
    char *text;
    size_t length;
    size_t capacity;
} Word;

// Stores a unique word and how often it appears
typedef struct {
    char *text;
    size_t count;
} WordCount;

// Dynamic array containing all unique words and their counts
typedef struct {
    WordCount *entries;
    size_t length;
    size_t capacity;
} WordTable;


int isLetter(char ch) {
    return ((ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z'));
}

int appendChar(Word *word, char ch) {
    //if new char and '\0' (null terminator) doesn't fit in *word raise cap and realloc/setup of new word
    if (word->length + 2 > word->capacity) {
        size_t newCapacity;

        if (word->capacity == 0) {
            newCapacity = 32;//default size
        } else {
            newCapacity = word->capacity * 2;
        }

        char *newText = realloc(word->text, newCapacity);
        if (newText == NULL) {
            return 0;
        }

        word->text = newText;
        word->capacity = newCapacity;
    }

    //adds char to word
    word->text[word->length] = ch;
    word->length++;
    word->text[word->length] = '\0';

    return 1;
}


int addWord(WordTable *table, const char *text, size_t length) {
    // Increase the count if the word is already in the table
    for (size_t i = 0; i < table->length; i++) {
        if (strcmp(table->entries[i].text, text) == 0) {
            table->entries[i].count++;
            return 1;
        }
    }

    // Make room for another entry if the table is full
    if (table->length == table->capacity) {
        size_t newCapacity;

        if (table->capacity == 0) {
            newCapacity = 16;//default size
        } else {
            newCapacity = table->capacity * 2;
        }

        WordCount *newEntries = realloc(table->entries, newCapacity * sizeof(WordCount));

        if (newEntries == NULL) {
            return 0;
        }

        table->entries = newEntries;
        table->capacity = newCapacity;
    }
    // Save a separate copy because the current word buffer gets reused
    char *copy = malloc(length + 1);
    if (copy == NULL) {
        return 0;
    }

    memcpy(copy, text, length + 1);

    table->entries[table->length].text = copy;
    table->entries[table->length].count = 1;
    table->length++;

    return 1;
}

int writeAll(const char *text, size_t length) {
    while (length > 0) {
        ssize_t n = write(STDOUT_FILENO, text, length);

        if (n <= 0) {
            return 0;
        }

        text += n;
        length -= (size_t)n;
    }

    return 1;
}


// Adds a completed word to the table, then clears the current word
int finishWord(Word *word, WordTable *table) {
    if (word->length == 0) {
        return 1;
    }

    // An apostrophe-only sequence is not a word ( ''' is not a word but a''' still is )
    int hasLetter = 0;
    for (size_t i = 0; i < word->length; i++) {
        if (isLetter(word->text[i])) {
            hasLetter = 1;
            break;
        }
    }

    //adds the word to the table
    if (hasLetter && !addWord(table, word->text, word->length)) {
        return 0;
    }

    //resets the buffer of word
    word->length = 0;
    word->text[0] = '\0';

    return 1;
}

// Prints the counts in the table's current order
int printCounts(const WordTable *table) {
    char countText[32];

    for (size_t i = 0; i < table->length; i++) {
        WordCount entry = table->entries[i];

        int n = sprintf(countText, "%zu\n", entry.count);

        if (n < 0 ||
            !writeAll(entry.text, strlen(entry.text)) ||
            !writeAll(" ", 1) ||
            !writeAll(countText, (size_t)n)) {
            return 0;
        }
    }

    return 1;
}


void freeWordTable(WordTable *table) {
    for (size_t i = 0; i < table->length; i++) {
        free(table->entries[i].text);
    }

    free(table->entries);

    table->entries = NULL;
    table->length = 0;
    table->capacity = 0;
}


int main(int argc, char const *argv[]){
    if (argc != 2) {
        fprintf(stderr, "Usage: %s FILE\n", argv[0]);
        return 1;
    }
    int curFile = open(argv[1], O_RDONLY);
    if (curFile == -1) {
        perror(argv[1]);
        return 1;
    }

    char buffer[4096];
    ssize_t bytes_read;

    Word word = {0};
    WordTable table = {0};

    char prevCh=' ';
    int pendingHyphen = 0;
    while ((bytes_read = read(curFile, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            char ch = buffer[i];

            // Delay a hyphen until we know the following character
            if (pendingHyphen) {
                if (isLetter(ch)) {

                    //adds the - to word
                    if (!appendChar(&word, '-')) {
                        perror("realloc");
                        free(word.text);
                        freeWordTable(&table);
                        close(curFile);
                        return 1;
                    }
                } else {
                    //word ends
                    if (!finishWord(&word, &table)) {
                        perror("addWord");
                        free(word.text);
                        freeWordTable(&table);
                        close(curFile);
                        return 1;
                    }
                }
                pendingHyphen = 0;
            }
            if (isLetter(ch) || ch == '\'') {

                //addes the letters to word
                if (!appendChar(&word, ch)) {
                    perror("realloc");
                    free(word.text);
                    freeWordTable(&table);
                    close(curFile);
                    return 1;
                }
            } else if (ch == '-' && isLetter(prevCh)) {
                pendingHyphen = 1;
            } else if (word.length>0) { 
                //word ends
                if (!finishWord(&word, &table)) {
                    perror("addWord");
                    free(word.text);
                    freeWordTable(&table);
                    close(curFile);
                    return 1;
                }
            }
            prevCh=ch;
        }
    }

    if (bytes_read == -1) {
        perror("read");
        free(word.text);
        freeWordTable(&table)
        close(curFile);
        return 1;
    }

    if (!finishWord(&word, &table)) {
        perror("addWord");
        free(word.text);
        freeWordTable(&table);
        close(curFile);
        return 1;
    }

    if (!printCounts(&table)) {
        perror("write");
        free(word.text);
        freeWordTable(&table);
        close(curFile);
        return 1;
    }

    free(word.text);
    freeWordTable(&table);

    if (close(curFile) == -1) {
        perror("close");
        return 1;
    }
    return 0;
}
