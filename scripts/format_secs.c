#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>


#define DEFAULT_INPUT_NAME "sec.tmp"
#define DEFAULT_OUTPUT_NAME "nautilus.secs"
#define MAX_SYM_SIZE 1024

#define ST_MAGIC    0x00952800

typedef struct seclist{
    uint64_t value;
	uint32_t sec_size;
    uint64_t offset;
    struct seclist* next;
}Seclist;

typedef struct secentry{
    uint64_t value;
	uint32_t sec_size;
    uint64_t offset;
}Secentry;

//this function receives a pointer to the ascii reperesentation of 64bit addr 
//as parameter, and converts it to the binary representation of that addr
uint64_t atob(uint8_t* p){
    uint64_t opt = 0;   //output
    uint64_t buf = 0;   //buffer
    int i;
    for(i = 0;i < 16;i++){
        buf = p[i];
        if(buf <= 0x39){
            buf -= 0x30;
        }
        if(0x30 < buf && buf <= 0x66){
            buf -= 0x57;
        }
        buf <<= (15 - i) * 4;
        opt |= buf;
    }
    return opt;
}


uint32_t atob_size(uint8_t* p){
    uint32_t opt = 0;   //output
    uint32_t buf = 0;   //buffer
    int i;
    for(i = 0;i < 6;i++){
        buf = p[i];
        if(buf <= 0x39){
            buf -= 0x30;
        }
        if(0x30 < buf && buf <= 0x66){
            buf -= 0x57;
        }
        buf <<= (5 - i) * 4;
        opt |= buf;
    }
    return opt;
}

int 
main (int argc,char** argv) {

    FILE* fp = NULL;
    FILE* nsfp = NULL;
    void* buf = NULL;

    if (argc == 1) {
        fp = fopen(DEFAULT_INPUT_NAME, "r");
        if (!fp) {
            perror("cannot open file");
            exit(EXIT_FAILURE);
        }
    } else if (argc == 2) {
        fp = fopen(argv[1], "r");

        if (!fp) {
            perror("cannot open file");
            exit(EXIT_FAILURE);
        }
    }

    // output file pointer, this file contains binary 
    // representation of the symbol table.
    nsfp = fopen(DEFAULT_OUTPUT_NAME, "wb");
    if (!nsfp) {
        perror("Cannot create output file");
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    
    buf = (void*) malloc(MAX_SYM_SIZE); //buffer for each line from inputfile
    bzero(buf, MAX_SYM_SIZE);
    
    uint32_t offset = 0;    //symbol name offset
	uint32_t sec_size = 0;
    uint64_t value = 0;     //actual addr of the symbol
    Seclist* head = (Seclist*) malloc(sizeof(Seclist));
    Seclist* pre = head;
    uint32_t noe = 0;       //number of entries in the symbol table
    char* strp = NULL;      
    size_t length = 0;

    uint32_t magicnum = ST_MAGIC;
    fwrite(&magicnum,4,1,nsfp);
    fwrite(&offset,4,1,nsfp);//placeholder
    fwrite(&noe,4,1,nsfp);   //placeholder


    while(fgets(buf,MAX_SYM_SIZE,fp)){
	/*
        if((value = atob((uint8_t*) buf)) == 0){   //discard entry whose value = 0
           continue;
        }
	*/
		value = atob((uint8_t*) buf);
		sec_size = atob_size((uint8_t*) buf+0x11);

        Seclist* tmp = (Seclist*) malloc(sizeof(Seclist));
        tmp->next = NULL;
        tmp->value = value;
		tmp->sec_size = sec_size;
        tmp->offset = offset;
        
		pre->next = tmp;
        pre = tmp;

        strp = (char*)buf + 0x18;
        length = strlen(strp);
        strp[length - 1] = 0x0;
        fwrite(strp,1,length,nsfp);
        offset += length;
        noe++;
        //printf("%s\n",buf);
    }
    fclose(fp);

    Seclist* sp = head->next;
/*
	mergesort_symlist(&sp);
	Symlist* tmp_sp = sp;
	while(tmp_sp) {
        fwrite(tmp_sp->sym,1,tmp_sp->sym_len,nsfp);
		tmp_sp->offset = offset;
        offset += tmp_sp->sym_len;
		tmp_sp = tmp_sp->next;
	}
*/
    //updates offset and noe records
    fseek(nsfp,4,SEEK_SET);
    offset += 12;
    fwrite(&offset,4,1,nsfp);
    fseek(nsfp,8,SEEK_SET);
    fwrite(&noe,4,1,nsfp);
    fseek(nsfp,0,SEEK_END);

	Secentry entry = {0,0,0};
    while(sp){
        entry.value = sp->value;
		entry.sec_size = sp->sec_size;
        entry.offset = sp->offset;
        fwrite(&entry,sizeof(Secentry),1,nsfp);
        sp = sp->next;
    }
    fclose(nsfp);
    return 0;
}
