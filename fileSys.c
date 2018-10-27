#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define disk_name "Drive10MB"
#define DISKSIZE  10000000

/*
#define disk_name "Drive2MB"
#define DISKSIZE  2000000
 */

#define METASIZE 24
#define BLOCKSIZE 512
#define MAXDIRENTRY (BLOCKSIZE/METASIZE)-1

enum bool {
    true, false
};

typedef struct {
    char name[8];
    char ext[3];
    enum bool is_dir;
    short start_block;
    int file_size;
} meta_file;

typedef struct {
    meta_file folder[BLOCKSIZE / METASIZE];
    //max folder entries??
} dir;

typedef struct {
    short table[(DISKSIZE / BLOCKSIZE) - 1];
} fatstr;

typedef struct {
    FILE *ptr; //ptr to start block of file data
    int file_start_block; //start block of file data
    int dir_index; //index of file within parent directory
    int dir_start_block; //start block of parent directory
    meta_file meta_data; //meta struct of file
    dir dir_data; //dir struct of parent directory
} op_file;

fatstr load_fat();
void write_fat(fatstr fat);
void print_disk_fat(int entries);
void print_mem_fat(fatstr *mem_fat, int entries);
int disk_empty(FILE *ptr);
void format_disk(FILE *disk);
void print_folder_contents(FILE * disk, char path[], fatstr *fatptr);
void process_cmd(char **args, fatstr *fatptr, FILE *disk);
//void print_folder_contents(FILE * disk, char folder[], fatstr *fatptr);
void user_input(fatstr *fatptr, FILE* disk);
char **sh_splitLine(char *line);
char *sh_readLine(void);

op_file open_file(char name[], char ext[], char path[], fatstr *fatptr);
void create_file(char name[], char ext[], char path[], fatstr *fatptr);
void create_folder(char name[], char path[], fatstr *fatptr);
void write_file(char source[], char name[], char ext[], char path[], fatstr *fatptr);
void read_file(char destination[], char name[], char ext[], char path[], fatstr *fatptr);
void delete_file(char name[], char ext[], char path[], fatstr *fatptr);

int main(int argc, char** argv) {
    FILE *disk = fopen(disk_name, "r+");
    fatstr fat_working;
    int root_block;

    if (disk_empty(disk)) {
        format_disk(disk);
    } else {
        printf("disk is already formatted\n");
    }
    //load fat
    fat_working = load_fat();
    //load starting block for rot dir
    root_block = fat_working.table[0] * -1;



/*
    //---------CREATE FOLDER, FILE & WRITE--------------
    //Create folder in root
    //            foldername, sub_dir, FAT                
    create_folder("NewDir", NULL, &fat_working);

    //Create file in new folder
    //           filename, ext  , sub dir , FAT
    create_file("NewFile", "txt", "NewDir", &fat_working);

    //--------------file 2-------------
    //Create file in new folder
    //           filename, ext  , sub dir , FAT
    create_file("File2", "txt", "NewDir", &fat_working);

    //writes data from source to file
    //         source file, dest file, ext  , sub dir , FAT
    write_file("book.txt", "NewFile", "txt", "NewDir", &fat_working);

    //--------------file 2-------------
    //writes data from source to file
    //         source file, dest file, ext  , sub dir , FAT
    write_file("short.txt", "File2", "txt", "NewDir", &fat_working);
*/


    //----------READS FROM DISK, SENDS TO FILE----------
    //reads froms file on disk, creates and writes to file outside of disk
    //           dst file , src file , ext  , sub dir , FAT
    read_file("bookread", "NewFile", "txt", "NewDir", &fat_working);

    //reads froms file on disk, creates and writes to file outside of disk
    //           dst file , src file , ext  , sub dir , FAT
    read_file("shortread", "File2", "txt", "NewDir", &fat_working);


/*
            //-----------DELETES FILE----------------
            //deletes file from FAT, erases data on disk, removes meta file
            //          file to del, ex   , sub dir  , FAT  
            delete_file("NewFile", "txt", "NewDir" , &fat_working);
*/




    print_folder_contents(disk, NULL, &fat_working);
    print_folder_contents(disk, "NewDir", &fat_working);


    //print non zero fat entries//
    print_mem_fat(&fat_working, DISKSIZE / BLOCKSIZE - 1); //////////////
    write_fat(fat_working);
    fclose(disk);
    return (EXIT_SUCCESS);
}

int disk_empty(FILE * ptr) {
    short check[1];
    fread(check, sizeof (check), 1, ptr);
    //printf("check = %d\n", check[0]);
    if (check[0] == 0) {
        //printf("ret 1\n");
        return 1;
    } else {
        //printf("ret 0\n");
        return 0;
    }
}

void format_disk(FILE * disk) {
    rewind(disk);
    fatstr fat_actual;

    int fat_byte_size = sizeof (fat_actual);
    printf("fat_byte_size = %d\n", fat_byte_size);

    int fat_block_size = fat_byte_size / BLOCKSIZE;
    printf("fat_block_size (block needed for FAT) = %d\n", fat_block_size);

    int fat_entries = (DISKSIZE / BLOCKSIZE) - 1;
    printf("fat_entries (total FAT entries) = %d\n", fat_entries);

    int fat_entries_avail = fat_entries - fat_block_size;
    printf("fat_entries_avail (total - entries needed for FAT) = %d\n", fat_entries_avail);

    //-----------------------set up fat-------------------------
    //sets fat entries to -2 for blocks containing FAT.
    //ie fat[0] - fat[fat_block_size]
    int i;
    for (i = 0; i <= fat_block_size; i++) {
        fat_actual.table[i] = -2;
    }

    //-----------------------set up root dir---------------------
    //create root dir
    dir dir_root;
    dir dir_buffer;
    printf("dir_root size = %d\n", sizeof (dir_root));

    //record the block of the root dir
    int root_dir_block = fat_block_size + 1;
    printf("root_dir_block  = %d\n", root_dir_block);

    //set fat entry for root dir to -1
    fat_actual.table[root_dir_block] = -1;

    //hide the the negative of the root dr block in fat[-]
    fat_actual.table[0] = -1 * root_dir_block;

    //zero out non zero entries after the root dir block
    for (i = root_dir_block + 1; i <= fat_entries; i++) {
        fat_actual.table[i] = 0;
    }
    fat_actual.table[fat_entries] = 0;

    //----------------write FAT to disk-----------------
    //write fat to disk
    if (disk != NULL) {
        fwrite(fat_actual.table, sizeof (fat_actual), 1, disk);
    } else {
        printf("file open disk error\n");
    }
    //----------------write root dir to disk------------
    //set ptr to block of root dir
    fseek(disk, (root_dir_block * BLOCKSIZE), SEEK_SET);
    //write fat struct to disk
    if (disk != NULL) {
        fwrite(dir_root.folder, sizeof (dir_root), 1, disk);
        fclose(disk);
    } else {
        printf("file open disk error\n");
    }

    printf("disk has been formatted\n");

}

fatstr load_fat() {
    fatstr fat_buffer;
    int i;
    FILE *fp2 = fopen(disk_name, "r+");
    if (fp2 != NULL) {
        fread(fat_buffer.table, sizeof (fat_buffer), 1, fp2);
        fclose(fp2);
    } else {
        printf("file open fp2 error\n");
    }

    return fat_buffer;
}

void write_fat(fatstr fat) {

    FILE *fp = fopen(disk_name, "r+");
    if (fp != NULL) {
        fwrite(fat.table, sizeof (fat), 1, fp);
        fclose(fp);
    } else {
        printf("file open disk error\n");
    }

}

void print_disk_fat(int entries) {
    fatstr fat_buffer;
    int i;
    FILE *fp2 = fopen(disk_name, "r+");
    if (fp2 != NULL) {
        fread(fat_buffer.table, sizeof (fat_buffer), 1, fp2);
        fclose(fp2);

        for (i = 0; i <= entries; i++) {
            printf("%d = %d\n", i, fat_buffer.table[i]);
        }

    } else {
        printf("file open fp2 error\n");
    }

}

void print_mem_fat(fatstr *fat, int entries) {
    printf("fat in mem:\n");
    int i;
    for (i = 0; i < entries; i++) {
        if (fat->table[i] != 0 && fat->table[i] != -2) {
            printf("%d = %d\n", i, fat->table[i]);
        }
    }

    printf("end fat in mem\n");
}

void print_folder_contents(FILE * disk, char path[], fatstr *fatptr) {
    dir dir_buffer;
    char *folder_name;
    int k;

    int root_block = fatptr->table[0] * -1;
    fseek(disk, root_block * BLOCKSIZE, SEEK_SET);
    fread(dir_buffer.folder, sizeof (dir_buffer), 1, disk);


    if (path != NULL) {
        int flag = 0;

        for (k = 0; k < MAXDIRENTRY; k++) {
            if ((strcmp(dir_buffer.folder[k].name, path) == 0) && (dir_buffer.folder[k].is_dir == true)) {
                //folder_name = dir_buffer.folder[k].name;
                printf("\nfolder '%s' contents:\n\n", dir_buffer.folder[k].name);
                //printf("(open)folder found = %s\n", dir_buffer.folder[k].name);
                flag = 1;
                //set pointer to start of path dir block
                //dir_start_block = dir_buffer.folder[k].start_block;
                fseek(disk, dir_buffer.folder[k].start_block * BLOCKSIZE, SEEK_SET);
                //load path dir into buffer
                fread(dir_buffer.folder, sizeof (dir_buffer), 1, disk);
                break;
            }
        }

        if (flag == 0) {
            printf("(open)folder '%s' not found\n", path);
            return;
        }

    } else {
        folder_name = "Root";
        printf("\nfolder '%s' contents:\n\n", folder_name);
    }


    for (k = 0; k < MAXDIRENTRY; k++) {
        if (dir_buffer.folder[k].start_block != 0) {
            printf("folder index = %d\n", k);

            if (dir_buffer.folder[k].is_dir == true) {
                printf("Folder: %s\n", dir_buffer.folder[k].name);
            } else {
                printf("File: %s.%s\n", dir_buffer.folder[k].name, dir_buffer.folder[k].ext);
            }
            printf("Start block: %d\n", dir_buffer.folder[k].start_block);
            printf("File size: %d\n\n", dir_buffer.folder[k].file_size);

            if (dir_buffer.folder[k + 1].start_block == 0) {
                k = MAXDIRENTRY;
            }
        }
    }
}

void read_file(char destination[], char name[], char ext[], char path[], fatstr * fatptr) {
    op_file source = open_file(name, ext, path, fatptr);
    FILE *dst_ptr = fopen(destination, "w+");
    char buffer[BLOCKSIZE];

    int blocks_to_read = source.meta_data.file_size / BLOCKSIZE; //////////check this
    if (source.meta_data.file_size % BLOCKSIZE != 0) {
        blocks_to_read++;
    }
    //printf("source.meta_data.file_size = %d\n", source.meta_data.file_size);
    //printf("blocks_to_read = %d\n", blocks_to_read);

    int block_num;
    int file_block = source.meta_data.start_block;
    //printf("file_block = %d\n", file_block);

    for (block_num = 1; block_num <= blocks_to_read; block_num++) {

        //read from disk to buffer
        fseek(source.ptr, file_block * BLOCKSIZE, SEEK_SET);
        fread(buffer, BLOCKSIZE, 1, source.ptr);
        //printf("buff = %s\n", buffer);
        //write from buffer to file
        fwrite(buffer, BLOCKSIZE, 1, dst_ptr);
        file_block = fatptr->table[file_block];

        //printf("file_block = %d\n", file_block);

        memset(buffer, 0, BLOCKSIZE);
    }

    fclose(dst_ptr);
}

void write_file(char source[], char name[], char ext[], char path[], fatstr * fatptr) {
    FILE *src = fopen(source, "r");
    op_file dst = open_file(name, ext, path, fatptr);
    char buffer[BLOCKSIZE];

    int fat_index = dst.meta_data.start_block;
    int prev_fat_index = 0;

    int count = 0;

    while (1) {
        //read from source to buffer
        fread(buffer, BLOCKSIZE, 1, src);

        //check for end of file
        if (strlen(buffer) == 0) {
            //printf("no more blocks to read from source\n");
            break;
        }
        //printf("buffer = %s\n", buffer);

        //set fat entry for prev block to next block
        //set fat entry for next block to -1
        if (prev_fat_index != 0) {
            fatptr->table[prev_fat_index] = fat_index;
            fatptr->table[fat_index] = -1;
        }

        //write from buffer to disk
        fseek(dst.ptr, fat_index * BLOCKSIZE, SEEK_SET);
        fwrite(buffer, BLOCKSIZE, 1, dst.ptr);

        //update file size
        dst.meta_data.file_size += strlen(buffer); //1char = 1byte -> len = size
        count++;

        //find next free block in fat
        prev_fat_index = fat_index;
        while (fatptr->table[fat_index] != 0) {
            fat_index++;
        }

        //memset buffer
        memset(buffer, 0, BLOCKSIZE);
    }

    //inserts updated meta file into the dir/folder that contains it
    dst.dir_data.folder[dst.dir_index] = dst.meta_data; //old = [dir_start_block]

    //write updated directory containing new meta struct to disk
    fseek(dst.ptr, dst.dir_start_block * BLOCKSIZE, SEEK_SET);
    fwrite(dst.dir_data.folder, sizeof (dst.dir_data), 1, dst.ptr);

    //printf("file size %d\n", dst.meta_data.file_size);
    //fclose(readfile);
    fclose(src);
    fclose(dst.ptr);
}

op_file open_file(char name[], char ext[], char path[], fatstr * fatptr) {
    if (strlen(name) > 8 || strlen(ext) > 3) {
        printf("ERROR: size of name and/or extention is too long\n");
        return;
    }

    //open disk
    FILE *disk = fopen(disk_name, "r+");
    //create dir buffer
    dir dir_buffer;


    //load root dir block
    int dir_start_block;
    int root_block = dir_start_block = fatptr->table[0] * -1;

    //set pointer to start of root dir block
    fseek(disk, root_block * BLOCKSIZE, SEEK_SET);
    //load dir into buffer
    fread(dir_buffer.folder, sizeof (dir_buffer), 1, disk);

    //follow path
    if (path != NULL) {
        int k, flag;
        flag = 0;
        for (k = 0; k < MAXDIRENTRY; k++) {
            if ((strcmp(dir_buffer.folder[k].name, path) == 0) && (dir_buffer.folder[k].is_dir == true)) {
                //printf("(open)folder found = %s\n", dir_buffer.folder[k].name);
                flag = 1;
                //set pointer to start of path dir block
                dir_start_block = dir_buffer.folder[k].start_block;
                fseek(disk, dir_start_block * BLOCKSIZE, SEEK_SET);
                //load path dir into buffer
                fread(dir_buffer.folder, sizeof (dir_buffer), 1, disk);
                break;
            }
        }

        if (flag == 0) {
            printf("folder '%s' not found\n", path);
            return;
        }
    }

    if (path == NULL) {
        path = "Root";
    }

    int j = 0;
    while (strcmp(dir_buffer.folder[j].name, name) != 0) {
        j++;
        if (j == MAXDIRENTRY) {
            printf("ERROR: no file '%s' was found in folder '%s'\n", name, path);
            return;
        }
    }

    //printf("file '%s' found!\n", dir_buffer.folder[j].name);
    //printf("start block = %d\n\n", dir_buffer.folder[j].start_block);

    fseek(disk, dir_buffer.folder[j].start_block * BLOCKSIZE, SEEK_SET);

    op_file output;
    output.ptr = disk;
    output.meta_data = dir_buffer.folder[j];
    output.dir_data = dir_buffer;
    output.dir_start_block = dir_start_block;
    output.dir_index = j;
    return output;

}

void create_file(char name[], char ext[], char path[], fatstr * fatptr) {

    if (strlen(name) > 8 || strlen(ext) > 3) {
        printf("ERROR: size of name and/or extention is too long\n");
        return;
    }

    //open disk
    FILE *disk = fopen(disk_name, "r+");
    //create dir buffer
    dir dir_buffer;
    //block of meta dir to be modified
    int meta_dir_block;

    //load root dir block
    int root_block = meta_dir_block = fatptr->table[0] * -1;
    //calc root start byte
    int root_start_byte = root_block * BLOCKSIZE;
    //printf("root_start_byte = %d\n", root_start_byte);


    //set pointer to start of root dir block
    fseek(disk, root_start_byte, SEEK_SET);
    //load dir into buffer
    fread(dir_buffer.folder, sizeof (dir_buffer), 1, disk);

    //follow path
    if (path != NULL) {
        int k, flag;
        flag = 0;
        for (k = 0; k < MAXDIRENTRY; k++) {
            if ((strcmp(dir_buffer.folder[k].name, path) == 0) && (dir_buffer.folder[k].is_dir == true)) {

                //printf("strcmp = %d\n", (strcmp(dir_buffer.folder[k].name, path) == 0) );
                //printf("is_dir = %d\n", dir_buffer.folder[k].is_dir == true );
                //printf("folder found = %s\n", dir_buffer.folder[k].name);
                flag = 1;
                //set pointer to start of path dir block
                meta_dir_block = dir_buffer.folder[k].start_block;
                fseek(disk, meta_dir_block * BLOCKSIZE, SEEK_SET);
                //load path dir into buffer
                fread(dir_buffer.folder, sizeof (dir_buffer), 1, disk);
                break;
            }
        }
        if (flag == 0) {
            printf("folder '%s' not found\n", path);
            for (k = 0; k < MAXDIRENTRY; k++) {
            }
            return;
        }
    }

    //find next free block in fat
    int file_block = 0;
    while (fatptr->table[file_block] != 0) {
        file_block++;
    }

    //set fat entry for new file to -1
    fatptr->table[file_block] = -1;
    //printf("new entry block = %d\n", file_block);

    // create new meta struct for file
    meta_file newMeta;
    strcpy(newMeta.name, name);
    strcpy(newMeta.ext, ext);
    newMeta.file_size = 0;
    newMeta.start_block = file_block;
    newMeta.is_dir = false;

    //find empty index within dir for new meta struct
    int j = 0;
    while (strlen(dir_buffer.folder[j].name) != 0) {
        j++;
    }
    //printf("folder index for new meta file = %d\n", j);

    //insert meta struct into dir buffer
    dir_buffer.folder[j] = newMeta;

    //write updated directory containing new meta struct to disk
    fseek(disk, meta_dir_block * BLOCKSIZE, SEEK_SET);
    fwrite(dir_buffer.folder, sizeof (dir_buffer), 1, disk);

    fclose(disk);

}

void create_folder(char name[], char path[], fatstr *fatptr) {

    if (strlen(name) > 8) {
        printf("ERROR: size of name is too long\n");
        return;
    }

    //open disk
    FILE *disk = fopen(disk_name, "r+");
    //create dir buffer
    dir dir_buffer;
    //block of meta dir to be modified
    int meta_dir_block;

    //load root dir block
    int root_block = meta_dir_block = fatptr->table[0] * -1;
    //set pointer to start of root dir block
    fseek(disk, root_block * BLOCKSIZE, SEEK_SET);
    //load dir into buffer
    fread(dir_buffer.folder, sizeof (dir_buffer), 1, disk);

    //check path
    if (path != NULL) {
        int k;
        int flag;
        flag = 0;
        for (k = 0; k < MAXDIRENTRY; k++) {
            if ((strcmp(dir_buffer.folder[k].name, path) == 0) && (dir_buffer.folder[k].is_dir == true)) {
                //printf("folder found = %s\n", dir_buffer.folder[k].name);
                flag = 1;
                //set pointer to start of path dir block
                meta_dir_block = dir_buffer.folder[k].start_block;
                fseek(disk, meta_dir_block * BLOCKSIZE, SEEK_SET);
                //load path dir into buffer
                fread(dir_buffer.folder, sizeof (dir_buffer), 1, disk);
                break;
            }
        }
        if (flag == 0) {
            printf("folder '%s' not found\n", path);
            for (k = 0; k < MAXDIRENTRY; k++) {
            }
            return;
        }
    }

    //find next free block in fat
    int i = 0;
    while (fatptr->table[i] != 0) {
        i++;
    }
    //find starting block of updated dir
    int dir_block = i;
    // create new meta struct for file
    meta_file newMeta;
    strcpy(newMeta.name, name);
    newMeta.file_size = sizeof (dir_buffer);
    newMeta.start_block = dir_block;
    newMeta.is_dir = true;


    //set fat entry for new folder to -1
    fatptr->table[dir_block] = -1;
    //printf("new entry block = %d\n", dir_block);

    //find empty index within dir for new meta struct
    int j = 0;
    while (strlen(dir_buffer.folder[j].name) != 0) {
        j++;
    }
    //printf("folder index for new meta file = %d\n", j);

    //write blank directory to fat block
    dir dir_blank;
    fseek(disk, newMeta.start_block * BLOCKSIZE, SEEK_SET);
    fwrite(dir_blank.folder, sizeof (dir_buffer), 1, disk);


    //insert meta struct into dir buffer
    dir_buffer.folder[j] = newMeta;
    //write updated directory containing new meta struct to disk
    fseek(disk, meta_dir_block * BLOCKSIZE, SEEK_SET);
    fwrite(dir_buffer.folder, sizeof (dir_buffer), 1, disk);

    fclose(disk);
}

void delete_file(char name[], char ext[], char path[], fatstr *fatptr) {
    op_file file_data = open_file(name, ext, path, fatptr);
    char buffer[BLOCKSIZE];
    int i;
    for (i = 0; i < BLOCKSIZE; i++) {
        buffer[i] = 0;
    }
    //printf("buff size for del = %d\n", sizeof (buffer));

    int blocks = file_data.meta_data.file_size / BLOCKSIZE;
    if (file_data.meta_data.file_size % BLOCKSIZE != 0) {
        blocks++;
    }
    //printf("file size to be deleted = %d\n", file_data.meta_data.file_size);
    //printf("blocks to be deleted = %d\n", blocks);

    int fat_index = file_data.meta_data.start_block;
    int nxt_fat_index = fat_index;

    i = 1; //i <= blocks
    while (nxt_fat_index != -1) {

        fat_index = nxt_fat_index;

        //write blank buffer to disk
        fseek(file_data.ptr, fat_index * BLOCKSIZE, SEEK_SET);
        fwrite(buffer, BLOCKSIZE, 1, file_data.ptr);
        //printf("block deleted = %d\n", fat_index);


        nxt_fat_index = fatptr->table[fat_index];

        //zero fat entry for cleared block
        fatptr->table[fat_index] = 0;

        i++;
    }

    //inserts blank meta file into the dir/folder that contains it
    meta_file blank;

    file_data.dir_data.folder[file_data.dir_index] = blank; //dst.meta_data;

    //write updated directory containing new meta struct to disk
    fseek(file_data.ptr, file_data.dir_start_block * BLOCKSIZE, SEEK_SET);
    fwrite(file_data.dir_data.folder, sizeof (file_data.dir_data), 1, file_data.ptr);


}

void close_file(op_file op_file_str) {
    fclose(op_file_str.ptr);
}

// takes input from user returns a pointer to it

char *sh_readLine(void) {
    char *line = NULL;
    ssize_t bufsize = 0;
    getline(&line, &bufsize, stdin);
    return line;
}


#define BUFSIZE 64
#define DELIM "/. \t\r\n\a"
//input: pointer to user entered commands
//output: pointer to individual arguments broken up by a set of delimiters
//        defined in 'DELIM'

char **sh_splitLine(char *line) {
    int bufsize = BUFSIZE;
    int index = 0;
    char **tokenSet = malloc(bufsize * sizeof (char*));
    char *token;
    char **backupTokenSet;

    // strtok() breaks up a pointer according to the set 'DELIM'
    token = strtok(line, DELIM);
    while (token != NULL) {
        tokenSet[index] = token;
        index++;

        // if set_of_tokens is longer then the memory allocated for it, it is
        // saved into token_backup, then the memory for set_of_tokens is
        // increased by 'BUFSIZE' 
        if (index >= bufsize) {
            bufsize += BUFSIZE;
            backupTokenSet = tokenSet;
            tokenSet = realloc(tokenSet, bufsize * sizeof (char*));
        }

        token = strtok(NULL, DELIM);
    }
    tokenSet[index] = NULL;
    return tokenSet;
}

void user_input(fatstr *fatptr, FILE* disk) {
    char *line;
    char **args;
    int status = 1;

    do {

        printf("> ");
        line = sh_readLine(); // request input from user
        args = sh_splitLine(line); // split input into individual args

        // process commands
        if (args[0] == NULL) {
            // no input given by user
            printf("No command enetered\n");
        } else if (!strcmp(args[0], "exit") || !strcmp(args[0], "quit")) {
            // exit or quit entered by user then exit the shell
            printf("Now exiting...\n");
            status = 0;
            break;
        }
        /*
                else {
                    int i = 0;
                    while (args[i] != NULL) {
                        printf("%d = %s\n", i, args[i]);
                        i++;
                    }
                }
         */
        process_cmd(args, fatptr, disk);
        write_fat(*fatptr);

        free(line);
        free(args);
    } while (status);
}

void process_cmd(char **args, fatstr *fatptr, FILE *disk) {
    int i = 0;
    while (args[i] != NULL) {
        i++;
    }
    i--;
    // printf("args = %d\n", i);

    if (strcmp(args[0], "create_file") == 0) {

        if (i == 3) {
            create_file(args[2], args[3], args[1], fatptr);
        } else if (i == 2) {
            create_file(args[1], args[2], NULL, fatptr);
        } else {
            printf("wrong number of argument\n");
        }

    } else if (strcmp(args[0], "create_folder") == 0) {

        if (i == 2) {
            create_folder(args[2], args[1], fatptr);
        } else if (i == 1) {
            create_folder(args[1], NULL, fatptr);
        } else {
            printf("wrong number of argument\n");
        }
    } else if (strcmp(args[0], "open_file") == 0) {

        if (i == 3) {
            open_file(args[2], args[3], args[1], fatptr);
        } else if (i == 2) {
            open_file(args[1], args[2], NULL, fatptr);
        } else {
            printf("wrong number of argument\n");
        }
    } else if (strcmp(args[0], "read_file") == 0) {

        if (i == 4) {
            read_file(args[4], args[2], args[3], args[1], fatptr);
        } else if (i == 3) {
            read_file(args[3], args[1], args[2], NULL, fatptr);
        } else {
            printf("wrong number of argument\n");
        }
    } else if (strcmp(args[0], "write_file") == 0) {

        if (i == 4) {
            write_file(args[4], args[2], args[3], args[1], fatptr);
        } else if (i == 3) {
            write_file(args[3], args[1], args[2], NULL, fatptr);
        } else {
            printf("wrong number of argument\n");
        }
    } else if (strcmp(args[0], "delete_file") == 0) {
        if (i == 3) {
            delete_file(args[2], args[3], args[1], fatptr);
        } else if (i == 2) {
            delete_file(args[1], args[2], NULL, fatptr);
        } else {
            printf("wrong number of argument\n");
        }
    } else if (strcmp(args[0], "print_folder") == 0) {
        //print_folder_contents(FILE * disk, char path[], fatstr *fatptr)
        if (i == 0) {
            print_folder_contents(disk, NULL, fatptr);
        } else if (i == 1) {
            print_folder_contents(disk, args[1], fatptr);
        } else {
            printf("wrong number of argument\n");
        }
    } else if (strcmp(args[0], "print_fat") == 0) {
        print_mem_fat(fatptr, DISKSIZE / BLOCKSIZE - 1);
    } else {
        printf("command not valid\n");
    }



}