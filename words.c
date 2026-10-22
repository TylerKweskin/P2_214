#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

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

    for(int i = 0; i < table->length-1; i++){
        for(int k = 0; k < table->length-1-i; k++){
            //strcmp(table->entries[k].text, table->entries[k+1].text)
            if(table->entries[k].count < table->entries[k+1].count){
                //Swap
                WordCount temp = table->entries[k];
                table->entries[k] = table->entries[k+1];
                table->entries[k+1] = temp;
            } else if(table->entries[k].count == table->entries[k+1].count){
                if(strcmp(table->entries[k].text, table->entries[k+1].text) > 0){
                    //Swap
                    WordCount temp = table->entries[k];
                    table->entries[k] = table->entries[k+1];
                    table->entries[k+1] = temp;
                }
            }
        }
    }

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

char** addFromDir(char** names, int* count, char* name, char* ext){
    //printf("name: %s", name);

    char** returnArray = names;

    DIR *dirp = opendir (name);
    if (!dirp) {
        perror (name);
        exit (EXIT_FAILURE);
    }
    struct dirent *de;
    //int first = 1;
    while ((de = readdir (dirp))) {
        //if(first){ first = 0; continue; }
        //printf("de->d_name: %s\n", de->d_name);

        // use de for something
        //if(de->d_type == DT_REG) continue; //skips over penises starting with a period

        if(de->d_type == DT_DIR){
            if((int)strcmp((char*)(de->d_name), ".")!=0){
                if((int)strcmp((char*)(de->d_name), "..")!=0){
                    char* dirPath = malloc(sizeof(char) * (strlen(name) + strlen(de->d_name) + 1+1));
                    dirPath[0] = '\0';
                    strcat(dirPath, name);
                    strcat(dirPath, "/");
                    strcat(dirPath, de->d_name);
                    names = addFromDir(names, count, dirPath, ext);
                    free(dirPath);
                }
            }

            
        }

        if(de->d_type == DT_REG && strlen((char*)de->d_name) >= strlen(ext) && (char*)de->d_name[0] != '.'){
            char* ninad = malloc(strlen(ext)+1);
            strncpy(ninad, (char*)de->d_name + strlen((char*)de->d_name) - strlen(ext), strlen(ext)+1);
            if(strcmp(ninad, ext) == 0){ //Check if it has the correct extension
                char** temp = realloc(names, sizeof(char*) * (*count + 1));
                if(temp == NULL) perror("Null pointer on 228");
                names = temp;

                char* a = malloc(sizeof(char) * (strlen(name) + 1 + strlen((char*)de->d_name)) + 1);
                a[0] = '\0';
                strcpy(a, name);
                strcat(a, "/"); strcat(a, (char*)de->d_name); 
                names[(*count)] = a;
                //free(a);
                (*count)++;
            }
            free(ninad);
        }


        //printf ("%s\n", de->d_name);
    }

    closedir (dirp);
    return names;
}

/*
Needs to:


*/
int main(int argc, char const *argv[]){
    int isFormatSpecified = 0;
    
    /*for(int i = 0; i < argc; i++){
        printf("Arg %d: %s\n", i, argv[i]);
    }*/

    //format specified
    if(argv[1][0]=='-'){
        //printf("dash found cooked\n");
        isFormatSpecified = 1;
    }

    char* fileExtension;
    char* fileName;


    int fileNameCount = 0;
    char** dynamicFileNameStorage = malloc(sizeof(char*));

    if(!isFormatSpecified){
        // ./word docs
        //Good to look for txt file extension
        fileExtension = malloc(sizeof(char) * strlen(argv[1])+1);
        strcpy(fileExtension, ".txt");
        //fileName = malloc(sizeof(char) * strlen(argv[1]));
        fileName = argv[1];
    } else {
        if(strlen(argv[1]) == 2){
            // ./words -s .md docs
            //format is specified in argv[2]
            fileExtension = malloc(sizeof(char) * (strlen(argv[2])+1));
            strcpy(fileExtension,argv[2]);

            //fileName = malloc(sizeof(char) * strlen(argv[3]));
            fileName = argv[3];
        } else{
            //format is specifie in argv[1]
            //./words -s.md docs
            
            fileExtension = malloc(sizeof(char) * strlen(argv[1])-2+1);
            strncpy(fileExtension, argv[1] + 2, strlen(argv[1])-2);

            //fileName = malloc(sizeof(char) * strlen(argv[2]));
            fileName = argv[2];
        }
    }

    //printf("FileName Line 258: %s\n", fileName);
    //printf("File Extension: %s\n", fileExtension);

    struct stat data;


    //printf("fileName: %s\n", fileName);

    int r = stat (fileName, &data);
    if (r < 0) {
        char* a = malloc((strlen(fileName)+1) * sizeof(char)); strcpy(a, fileName);
        char* b = malloc((strlen(fileExtension)+1) * sizeof(char)); strcpy(b, fileExtension);

        //printf("a: %s\n", a);
        //printf("b: %s\n", b);
        
        int fuck = stat(strcat(a,b), &data);
        if(fuck < 0){
            perror(fileName);
            exit(EXIT_FAILURE);
        }

        free(a);
        free(b);
    }

    //printf("made it through to line 315\n");

    //printf("FileName Line 270: %s\n", fileName);

    if(S_ISDIR(data.st_mode)){
        //there's multiple penises
        //printf("File Name Pre: %s \n", fileName);
        dynamicFileNameStorage = addFromDir(dynamicFileNameStorage, &fileNameCount, fileName, fileExtension);
    } else {
        //there's one penis
        dynamicFileNameStorage[0] = strcat(fileName,fileExtension);
        
        fileNameCount = 1;
        //printf("FileName Line 277: %s\n", fileName);
        //printf("File Storage Line 277: %s\n", dynamicFileNameStorage[0]);
    }

    for(int i = 0; i < fileNameCount; i++){
        //printf("%s\n", dynamicFileNameStorage[i]);
    }    



    /*if (argc != 2) {
        fprintf(stderr, "Usage: %s FILE\n", argv[0]);
        return 1;
    }*/

    //printf("reading files now\n");

    for(int filesRead = 0; filesRead < fileNameCount; filesRead++){

        //printf("Reading file: %s\n", dynamicFileNameStorage[filesRead]);
        //printf("FileName Line 258: %s\n", fileName);

        int curFile = open(dynamicFileNameStorage[filesRead], O_RDONLY);
        if (curFile == -1) {
            perror(dynamicFileNameStorage[filesRead]);
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
            freeWordTable(&table);
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
        free(dynamicFileNameStorage[filesRead]);
    }

    free(dynamicFileNameStorage);
    free(fileExtension);
    //free(fileName);

    return 0;
}
