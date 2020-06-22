
#include <stdio.h>
#include <fcntl.h>
#include "elfdef.h"

#define BUFFER_MAX_LEN    100000000

static uint32_t readElfSection(const char *filename, const char *sectionName, uint8_t **buf, unsigned long *buflen)
{
    static uint8_t filebuf[BUFFER_MAX_LEN];
    static uint8_t elfmagic[] = {0x7f, 'E', 'L', 'F'};
    int inputfd = 0;   /* default input for '-' is stdin */

    if (!filename)
        return -1;
    if (strcmp(filename, "-")) {
        inputfd = open(filename, O_RDONLY);
        if (inputfd == -1) {
            printf("fpgajtag: Unable to open file '%s'\n", filename);
            exit(-1);
        }
    }
    unsigned long input_filesize = read(inputfd, filebuf, sizeof(filebuf));
    uint8_t *input_fileptr = filebuf;
    close(inputfd);
    if (input_filesize <= 0 || input_filesize >= sizeof(filebuf) - 1)
        goto badlen;
    if (!memcmp(input_fileptr, elfmagic, sizeof(elfmagic))) {
        int found = 0;
        int entry;
        ELF_HEADER *elfh = (ELF_HEADER *)input_fileptr;
#define IS64() (elfh->h32.e_ident[4] == ELFCLASS64)
#define HELF(A) (IS64() ? elfh->h64.A : elfh->h32.A)
#define SELF(ENT, A) (IS64() ? sech->s64[ENT].A : sech->s32[ENT].A)
        printf("fpgajtag: elf input file, len %ld class %d\n", input_filesize, elfh->h32.e_ident[4]);
        int shnum = HELF(e_shnum);
        ELF_SECTION *sech = (ELF_SECTION *)&input_fileptr[HELF(e_shoff)];
        uint8_t *stringTable = &input_fileptr[SELF(HELF(e_shstrndx), sh_offset)];
        for (entry = 0; entry < shnum; ++entry) {
            char *name = (char *)&stringTable[SELF(entry, sh_name)];
            if (!strcmp(name, sectionName)) {
                *buf = &input_fileptr[SELF(entry, sh_offset)];
                *buflen = SELF(entry, sh_size);
                found = 1;
                return 0;
            }
        }
        if (!found) {
            printf("ERROR: attempt to use elf file, but no '%s' section found\n", sectionName);
            exit(-1);
        }
    }
badlen:;
printf("[%s:%d] size %ld\n", __FUNCTION__, __LINE__, (long)input_filesize);
    return 1;
}
