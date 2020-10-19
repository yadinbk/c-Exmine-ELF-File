#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <elf.h>

typedef struct fun_desc {
char *name;
void (*fun)();
} funDesc;

int currFd = -1;
int D;
int shstrtab_off, strtab_off, dynstr_off;
struct stat fd_stat;
Elf32_Ehdr* header;
Elf32_Shdr* section;
Elf32_Sym* symbol;
Elf32_Rel* relocation;
Elf32_Sym *dynsym;

void* map_start;

void readElf(Elf32_Ehdr* header);


void debugMode(){
    D = (D+1)%2;
    if(D) printf("Debug flag now on\n");
    else printf("Debug flag now off\n");
}

void examineElfFile(){
    int fd;
    char fn[100];
    fgetc(stdin); //dummy
    printf("Please enter file name: ");
    if(fgets(fn, 100, stdin)<0){
        perror("ERROR fgets: ");
        return;
    }
    fn[strlen(fn)-1] = 0;
    if(D) printf("Debug: file name set to %s\n", fn);

    if(currFd>0){
        close(currFd);
        munmap(map_start, fd_stat.st_size);
    }
    if((fd = open(fn, O_RDWR))<0){
        perror("ERROR open: ");
        return;
    }
    if(fstat(fd, &fd_stat)!=0){
        perror("ERROR fstat: ");
    }
    if((map_start = mmap(0, fd_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED){
        perror("ERROR mmap: ");
        return;
    }

    currFd = fd;
    header = (Elf32_Ehdr*) map_start;

    readElf(header);
}

void readElf(Elf32_Ehdr* header){
    printf("\nELF Header:\n");
    char m1,m2,m3, data;
    short shnum, phnum, shentsize, phentsize;
    int entry, shoff, phoff;
    /*1*/
    m1 = header->e_ident[EI_MAG1];
    m2 = header->e_ident[EI_MAG2];
    m3 = header->e_ident[EI_MAG3];
    if(m1!='E' || m2!='L' || m3!='F'){
        perror("ERROR: not ELF file");
        return;
    }
    printf("  Magic (3 digits):                  %X %X %X\n",m1,m2,m3);
    /*2*/
    data = header->e_ident[EI_DATA];
    printf("  Data: %42s\n",(data==1)?"little endian":"big endian");
    /*3*/
    entry = header->e_entry;
    printf("  Entry point address:               %#X\n", entry);
    /*4*/
    shoff = header->e_shoff;
    printf("  Start of section headers:          %d (bytes into file)\n", shoff);
    /*5*/
    shnum = header->e_shnum;
    printf("  Number of section headers:         %d\n", shnum);
    /*6*/
    shentsize = header->e_shentsize;
    printf("  Size of section headers:           %d\n", shentsize);
    /*7*/
    phoff = header->e_phoff;
    printf("  Start of program headers:          %d (bytes into file)\n", phoff);
    /*8*/
    phnum = header->e_phnum;
    printf("  Number of program headers:         %d\n", phnum);
    /*9*/
    phentsize = header->e_phentsize;
    printf("  Size of program headers:           %d\n", phentsize);
}

char* getShname(int i){
    void* strtable = (void*)(map_start + shstrtab_off);
    char* shname = (char*)strtable;   
    return shname + section[i].sh_name;
}

char* getShtype(int i){
    return
    (section[i].sh_type == SHT_NULL)? "NULL":
    (section[i].sh_type == SHT_PROGBITS)? "PROGBITS":
    (section[i].sh_type == SHT_SYMTAB)? "SYMTAB":
    (section[i].sh_type == SHT_STRTAB)? "STRTAB":
    (section[i].sh_type == SHT_DYNAMIC)? "DYNAMIC":
    (section[i].sh_type == SHT_NOTE)? "NOTE":
    (section[i].sh_type == SHT_NOBITS)? "NOBITS":
    (section[i].sh_type == SHT_REL)? "REL":
    (section[i].sh_type == SHT_DYNSYM)? "DYNSYM":

    "ELSE";
}

void printSectionNames(){
    if(currFd<0){
        printf("ERROR: No file opened\n");
        return;
    }
    section = (Elf32_Shdr*)(map_start + header->e_shoff);
    char *shname, *shtype;
    int i, shnum = header->e_shnum;
    shstrtab_off = section[header->e_shstrndx].sh_offset;
    printf("\n[Nr]  Name                   Addr      Off       Size      Type\n");
    for(i=0; i<shnum; i++){
        shname = getShname(i);
        shtype = getShtype(i);
        printf("[%02d]  %-20s   %08X  %08X  %08X  %-s\n",i, shname, section[i].sh_addr, section[i].sh_offset, section[i].sh_size, shtype);
    }
}

char* getSymname(int i){
    void* strtable =  (void*)(map_start+strtab_off);
    char* symname = (char*)strtable;   
    return symname + symbol[i].st_name;
}

void printSymbols(){
    if(currFd<0){
        printf("ERROR: No file opened\n");
        return;
    }
    char* shname, *symname;
    int i, j, symnum, shnum = header->e_shnum;
    for(i=0; i<shnum; i++){
        if(section[i].sh_type == SHT_SYMTAB){
            symbol = (Elf32_Sym*)(map_start + section[i].sh_offset);
            symnum = section[i].sh_size / sizeof(symbol[0]);
            for(int l=i; l<shnum; l++){ 
                if(section[l].sh_type == SHT_STRTAB){
                    if(strcmp(getShname(l), ".strtab")==0){  
                        strtab_off = section[l].sh_offset;
                    }
                }
            }
            printf("\nnumber of enteries: %d\n", symnum);
            printf("[Nr]  Value     Ndx  SecName          SymName\n");
            for(j=0; j<symnum; j++){
                int shi = symbol[j].st_shndx;
                if(shi != 65521){ //ABS
                    shname = getShname(shi);
                    symname = getSymname(j);
                    printf("[%02d]  %08X  %02d  %-15s  %-15s\n", j, symbol[j].st_value, shi, shname, symname);
                    if(D) printf("sym size: %d" ,symbol[j].st_size);
                }
                else{
                    printf("[%02d]  %08X  ABS                 \n", j, symbol[j].st_value);
                    if(D) printf("sym size: %d" ,symbol[j].st_size);
                }
            }
        }
    }
}


char* getDyname(int i){
    void* strtable =  (void*)(map_start+dynstr_off);
    char* symname = (char*)strtable;   
    return symname + i;
}

void relocationTables(){
    if(currFd<0){
        printf("ERROR: No file opened\n");
        return;
    }
    int i, j, dynvalue, dynndx, relnum, symtype, shnum = header->e_shnum;
    char* dynname;
    for(i=0; i<shnum; i++){
        if(section[i].sh_type == SHT_DYNSYM){
            dynsym = (Elf32_Sym*)(map_start + section[i].sh_offset);
        }
        if(section[i].sh_type == SHT_STRTAB){
            if(strcmp(getShname(i), ".dynstr")==0){  
                dynstr_off = section[i].sh_offset;
            }
        }
    }
    for(i=0; i<shnum; i++){
        if(section[i].sh_type == SHT_REL){
            relocation = (Elf32_Rel*)(map_start + section[i].sh_offset);
            relnum = section[i].sh_size / sizeof(relocation[0]);
            printf("\nnumber of enteries: %d\n", relnum);
            printf("Offset    Info      Type  Sym.value   Sym.name\n");
            for(j=0; j<relnum; j++){
                symtype = ELF32_R_TYPE(relocation[j].r_info);
                dynndx = ELF32_R_SYM(relocation[j].r_info);
                int namendx = dynsym[dynndx].st_name;
                dynname = getDyname(namendx);
                dynvalue = dynsym[dynndx].st_value;
                printf("%08X  %08X  %d     %08d    %s\n",relocation[j].r_offset, relocation[j].r_info, symtype, dynvalue, dynname);
            }
        }
    }
}


void quit(){
    munmap(map_start, fd_stat.st_size);
    if(currFd>0) close(currFd);
    if(D) printf("quiting\n");
    exit(0);
}

int main(int argc, char **argv){
	char c=0;
	int f=0;

	funDesc menu[] =   {{ "Toggle Debug Mode", debugMode }, { "Exmine ELF File", examineElfFile },
	                    {"Print Section Names", printSectionNames}, {"Print Symbols", printSymbols},
                        {"Relocation Tables", relocationTables}, {"Quit", quit}, { NULL, NULL } };

	int menuSize = (sizeof(menu))/(sizeof(funDesc))-1;
	while(1){
		f=1;
		printf("\nPlease choose a function:\n");
		for(int i=0; i < menuSize; ++i){
			printf("%d) %s\n", i, menu[i].name);
		}
		printf("Option: ");
		while((c=fgetc(stdin))!=EOF && f){
			if(c!='\n'){
				if(c>='0' && c<='9'){ 
					c-='0';
					int j=c;
                    menu[j].fun();
				}
				else{
					printf("Not within bounds - please enter number between 0-%d\n", menuSize-1); //todo make sure
				}
			}
			c=0;
			f=0;
		}
	}

}