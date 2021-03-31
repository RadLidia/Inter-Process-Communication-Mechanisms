#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>

#define FIFO_RESPONSE "RESP_PIPE_26063"//write+message"CONNECT"
#define FIFO_REQUEST "REQ_PIPE_26063"//read, no create
#define ALIGNMENT 1024

char buf[255];
unsigned int length;
char fileName[255];
char *sharedData, *sharedFileData;
int shmFd;
int fileSize;
int fdFile;
int fd1,fd2;
unsigned char size;

struct section_header {
    char sect_name[18];
    char sect_type;
    int sect_offset;
    int sect_size;
};
void read_string()
{
    read(fd2,&size,1);
    read(fd2,buf,size);
    buf[size] = '\0';
}

void write_string(char* message)
{
    size = strlen(message);
    write(fd1,&size,1);
    write(fd1, message, size);

}
int main(int argc, char **argv)
{
    if(access(FIFO_RESPONSE,0) == 0)
    {
        printf("Deleting pipe 1\n");
        unlink(FIFO_RESPONSE);
    }

    if(mkfifo(FIFO_RESPONSE, 0600) != 0) 
    {
        perror("Cannot create pipe 1");
        exit(1);
    }

    fd2 = open(FIFO_REQUEST, O_RDONLY);
    if(fd2 == -1) 
    {
        unlink(FIFO_RESPONSE);
        unlink(FIFO_REQUEST);
        printf("ERROR\ncannot open the request");
        exit(1);
    }

    fd1 = open(FIFO_RESPONSE, O_WRONLY);
    if(fd1 == -1) 
    {
        unlink(FIFO_RESPONSE);
        unlink(FIFO_REQUEST);
        printf("ERROR\ncannot create the response pipe");
        exit(1);
    }

    write_string("CONNECT");
    printf("SUCCESS\n");
    while(1)
    {
        read_string();

        if(strcmp(buf,"PING") == 0)
        {
            write_string("PING");
            write_string("PONG");

            unsigned int nr = 26063;
            write(fd1, &nr, sizeof(unsigned int));
        }
        if(strcmp(buf,"CREATE_SHM") == 0)
        {
            read(fd2,&length,sizeof(unsigned int));

            shmFd = shm_open("/KO8m3QfY", O_CREAT | O_RDWR, 0664);

            write_string("CREATE_SHM");
            if(shmFd < 0) {
                write_string("ERROR");
                perror("Could not aquire shm");
                return 1;
            }else{
                ftruncate(shmFd, length);
                sharedData = (char*)mmap(0, length, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0); 
                if(sharedData == (void*)-1)
                {
                    write_string("ERROR");
                }
                else
                {
                    write_string("SUCCESS");
                }
            }
        }

        if(strcmp(buf,"WRITE_TO_SHM") == 0)
        {   
            unsigned int offset, value;
            read(fd2,&offset,sizeof(unsigned int));
            read(fd2,&value,sizeof(unsigned int));

            write_string("WRITE_TO_SHM");
            if(offset < 0 || offset > 2830252)
            {
                write_string("ERROR");
            }else
            {   
                if(offset + sizeof(value) > 2830252)
                {
                    write_string("ERROR");
                }
                else
                {
                    // lseek(shmFd, offset, SEEK_SET);
                    // write(shmFd, &value, sizeof(value));
                    memcpy(sharedData + offset, &value, sizeof(unsigned int));
                    write_string("SUCCESS");
                }
            }
        }
        if(strcmp(buf,"MAP_FILE") == 0)
        {
            read(fd2,&size,1);
            read(fd2,&fileName,size);

            write_string("MAP_FILE");

            fdFile = open(fileName, O_RDONLY);
            if(fdFile == -1)
            {
                write_string("ERROR");
            }else{
                fileSize = lseek(fdFile,0,SEEK_END);
                lseek(fdFile,0,SEEK_SET);

                sharedFileData = (char*)mmap(0, fileSize, PROT_READ, 
                MAP_SHARED, fdFile, 0); 
                if(sharedFileData == (void*)-1)
                {
                    write_string("ERROR");
                    perror("Could not map the shared memory");
                    return 1;
                }
                write_string("SUCCESS");
            }
        }
        if(strcmp(buf,"READ_FROM_FILE_OFFSET") == 0)
        {
            unsigned int offset, nr_of_bytes;
            read(fd2,&offset,sizeof(unsigned int));
            read(fd2,&nr_of_bytes,sizeof(unsigned int));

            write_string("READ_FROM_FILE_OFFSET");

            if(offset + nr_of_bytes > fileSize)
            {
                write_string("ERROR");

            }else{
                //memcpy(void *dest, const void *src, size_t n) 
                //copies n characters from memory area src to memory area dest.
                memcpy(sharedData,sharedFileData + offset, nr_of_bytes);
                write_string("SUCCESS");

            }
        }
        if(strcmp(buf,"READ_FROM_FILE_SECTION") == 0)
        {
            unsigned int section, offset, nr_of_bytes;
            read(fd2,&section,sizeof(unsigned int));
            read(fd2,&offset,sizeof(unsigned int));
            read(fd2,&nr_of_bytes,sizeof(unsigned int));

            int header_size = 0, version = 0;
            short int nr_sections = 0;
            char magic;
            memcpy(&magic,sharedFileData,1);
            memcpy(&header_size,sharedFileData + 1,2);
            memcpy(&version,sharedFileData + 1 + 2, 4);
            memcpy(&nr_sections,sharedFileData + 1 + 2 + 4,1);
            
            struct section_header *fileStructure = NULL;
            fileStructure = (struct section_header*)malloc(nr_sections * sizeof(struct section_header)); 
            if((magic == 'x') && (version >= 105 && version <= 198) && (nr_sections >= 7 && nr_sections <= 18))
            {    
                if(section > nr_sections)
                {
                    write_string("READ_FROM_FILE_SECTION");
                    write_string("ERROR");
                    free(fileStructure);
                }
                for(int i = 0; i < nr_sections; i++)
                {   
                    memcpy(&fileStructure[i].sect_name,sharedFileData + 8 + 26 * i, 17);
                    memcpy(&fileStructure[i].sect_type,sharedFileData + 8 + 17 + 26 * i, 1);
                    memcpy(&fileStructure[i].sect_offset,sharedFileData + 8 + 17 + 1 + 26 * i, 4);
                    memcpy(&fileStructure[i].sect_size,sharedFileData + 8 + 17 + 1 + 4 + 26 * i, 4);
                    if(fileStructure[i].sect_type != 46 && fileStructure[i].sect_type != 27)
                    {   
                        write_string("READ_FROM_FILE_SECTION");
                        write_string("ERROR");
                        free(fileStructure);
                    }
                    if(section - 1 == i)
                    {
                        if(offset + nr_of_bytes <= fileStructure[i].sect_size)
                        {
                            memcpy(sharedData, sharedFileData + fileStructure[i].sect_offset + offset, nr_of_bytes);
                            write_string("READ_FROM_FILE_SECTION");
                            write_string("SUCCESS");
                            free(fileStructure);
                            break;
                        }
                    }
                }
            }else{
                write_string("READ_FROM_FILE_SECTION");
                write_string("ERROR");
                free(fileStructure);
                break;
            }
            free(fileStructure);
        }
        if(strcmp(buf,"READ_FROM_LOGICAL_SPACE_OFFSET") == 0)
        {
            unsigned int logical_offset, nr_of_bytes;
            read(fd2,&logical_offset,sizeof(unsigned int));
            read(fd2,&nr_of_bytes,sizeof(unsigned int));
            write_string("READ_FROM_LOGICAL_SPACE_OFFSET");

            int logical_position = 0;
            int curr_logical_pos = 0;
            int logical_sect_start_pos = 0;
            bool found = false;

            int sect_offset = 0;
            int header_size = 0, version = 0;
            short int nr_sections = 0;
            char magic;
            memcpy(&magic,sharedFileData,1);
            memcpy(&header_size,sharedFileData + 1,2);
            memcpy(&version,sharedFileData + 1 + 2, 4);
            memcpy(&nr_sections,sharedFileData + 1 + 2 + 4,1);
            
            struct section_header *fileStructure;
            fileStructure = (struct section_header*)malloc(nr_sections * sizeof(struct section_header)); 
            if((magic == 'x') && (version >= 105 && version <= 198) && (nr_sections >= 7 && nr_sections <= 18))
            {    
                for(int i = 0; i < nr_sections; i++)
                {   
                    memcpy(&fileStructure[i].sect_name,sharedFileData + 8 + 26 * i, 17);
                    memcpy(&fileStructure[i].sect_type,sharedFileData + 8 + 17 + 26 * i, 1);
                    memcpy(&fileStructure[i].sect_offset,sharedFileData + 8 + 17 + 1 + 26 * i, 4);
                    memcpy(&fileStructure[i].sect_size,sharedFileData + 8 + 17 + 1 + 4 + 26 * i, 4);
                    if(fileStructure[i].sect_type != 46 && fileStructure[i].sect_type != 27)
                    {   
                        write_string("ERROR");
                        free(fileStructure);
                    }else{
                        logical_position = fileStructure[i].sect_size;
                        sect_offset = fileStructure[i].sect_offset;
                        while(logical_position > ALIGNMENT)
                        {
                            logical_position = logical_position - ALIGNMENT;
                            curr_logical_pos = curr_logical_pos + ALIGNMENT;
                        }

                        curr_logical_pos = curr_logical_pos + ALIGNMENT;
                        if(logical_sect_start_pos <= logical_offset && (logical_offset + nr_of_bytes) <= (logical_sect_start_pos + fileStructure[i].sect_size))
                        {
                            found = true;
                            memcpy(sharedData, sharedFileData + sect_offset + logical_offset - logical_sect_start_pos, nr_of_bytes);
                            write_string("SUCCESS");
                            free(fileStructure);
                        }
                        logical_sect_start_pos = curr_logical_pos;
                    }

                }
                if(found == false)
                {
                    write_string("ERROR");
                    free(fileStructure);
                }
            }
        }

        if(strcmp(buf,"EXIT") == 0)
        {
            munmap(sharedData, 2830252);
            shm_unlink("/KO8m3QfY");
            sharedData = NULL;
            munmap(sharedFileData, fileSize);
            shm_unlink(fileName);
            sharedFileData = NULL;
            close(fd1);
            close(fd2);
            exit(0);
        }
    }
    return 0;
}