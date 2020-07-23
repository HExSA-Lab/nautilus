#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>


#define DEFAULT_INPUT_NAME "sym.tmp"
#define DEFAULT_OUTPUT_NAME "nautilus.syms"
#define MAX_SYM_SIZE 1024

#define ST_MAGIC    0x00952700

typedef struct symlist{
    uint64_t value;
    uint64_t offset;
	char* sym;
	int sym_len;
    struct symlist* next;
}Symlist;

typedef struct symentry{
    uint64_t value;
    uint64_t offset;
}Symentry;

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

void front_back_split(Symlist* source, 
		Symlist** frontRef, Symlist** backRef) 
{ 
	Symlist* fast; 
	Symlist* slow; 
	slow = source; 
	fast = source->next; 

	/* Advance 'fast' two nodes, and advance 'slow' one node */
	while (fast != NULL) { 
		fast = fast->next; 
		if (fast != NULL) { 
			slow = slow->next; 
			fast = fast->next; 
		} 
	} 

	/* 'slow' is before the midpoint in the list, so split it in two 
	   at that point. */
	*frontRef = source; 
	*backRef = slow->next; 
	slow->next = NULL; 
} 

Symlist* sorted_merge(Symlist* a, Symlist* b) 
{ 
	Symlist* result = NULL; 

	/* Base cases */
	if (a == NULL) 
		return (b); 
	else if (b == NULL) 
		return (a); 

	/* Pick either a or b, and recur */
	if (a->value <= b->value) { 
		result = a; 
		result->next = sorted_merge(a->next, b); 
	} 
	else { 
		result = b; 
		result->next = sorted_merge(a, b->next); 
	} 
	return (result); 
} 

void mergesort_symlist(Symlist** headRef) {
	Symlist* head = *headRef; 
	Symlist* a; 
	Symlist* b; 

	/* Base case -- length 0 or 1 */
	if ((head == NULL) || (head->next == NULL)) { 
		return; 
	} 

	/* Split head into 'a' and 'b' sublists */
	front_back_split(head, &a, &b); 

	/* Recursively sort the sublists */
	mergesort_symlist(&a); 
	mergesort_symlist(&b); 

	/* answer = merge the two sorted lists together */
	*headRef = sorted_merge(a, b); 
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
    uint64_t value = 0;     //actual addr of the symbol
    Symlist* head = (Symlist*) malloc(sizeof(Symlist));
    Symlist* pre = head;
    uint32_t noe = 0;       //number of entries in the symbol table
    char* strp = NULL;      
    size_t length = 0;

    uint32_t magicnum = ST_MAGIC;
    fwrite(&magicnum,4,1,nsfp);
    fwrite(&offset,4,1,nsfp);//placeholder
    fwrite(&noe,4,1,nsfp);   //placeholder


    while(fgets(buf,MAX_SYM_SIZE,fp)){
        if((value = atob((uint8_t*) buf)) == 0){   //discard entry whose value = 0
           continue;
        }
        strp = (char*)buf + 0x11;
        length = strlen(strp);
        strp[length - 1] = 0x0;

        Symlist* tmp = (Symlist*) malloc(sizeof(Symlist));
        tmp->next = NULL;
        tmp->value = value;
        tmp->offset = offset;
		tmp->sym = (char*) malloc(sizeof(char)*length);
		strcpy(tmp->sym, strp);
		tmp->sym_len = length;
        
		pre->next = tmp;
        pre = tmp;

        //fwrite(strp,1,length,nsfp);
        //offset += length;
        noe++;
        //printf("%s\n",buf);
    }
    fclose(fp);

    Symlist* sp = head->next;
	
	// Sort the linked list by address value
	mergesort_symlist(&sp);
	
	Symlist* tmp_sp = sp;
	while(tmp_sp) {
        fwrite(tmp_sp->sym,1,tmp_sp->sym_len,nsfp);
		tmp_sp->offset = offset;
        offset += tmp_sp->sym_len;
		tmp_sp = tmp_sp->next;
	}

    //updates offset and noe records
    fseek(nsfp,4,SEEK_SET);
    offset += 12;
    fwrite(&offset,4,1,nsfp);
    fseek(nsfp,8,SEEK_SET);
    fwrite(&noe,4,1,nsfp);
    fseek(nsfp,0,SEEK_END);

	Symentry entry = {0,0};
    while(sp){
        entry.value = sp->value;
        entry.offset = sp->offset;
        fwrite(&entry,sizeof(Symentry),1,nsfp);
        sp = sp->next;
    }
    fclose(nsfp);
    return 0;
}
