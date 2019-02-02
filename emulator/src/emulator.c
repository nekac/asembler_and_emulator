#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>
#include <unistd.h> 
#include <pthread.h> 

FILE* debug;

#define MAX_SIZE 1000
#define STACK_SIZE 256
#define DEFAULT_IV 0x80

#define LOCAL 'L'
#define GLOBAL 'G'

// za for petlje golobalni brojaci
int i, j;

// simbol koji se unosi u tabelu simbola

typedef struct {
    int id;
    char name[MAX_SIZE];
    int section;
    int value;
    int size;
    char visibility;
} Symbol;


char* token;
char* delim = " ,\t\n\r\b"; // sve vrste delimitera
char line[MAX_SIZE];

Symbol symbolTable[MAX_SIZE]; // predefined
int stp = 0; // symbol table pointer
int currentSection;
int LC; // local counter

typedef struct {
    int address;
    char type;
    int symbol;
} Relocation;

Relocation relocationTableText[MAX_SIZE];
int rtpText = 0;
Relocation relocationTableData[MAX_SIZE];
int rtpData = 0;
Relocation relocationTableROdata[MAX_SIZE];
int rtpROData = 0;
Relocation relocationTableBss[MAX_SIZE];
int rtpBss = 0;
Relocation *relocationTableCurrent;
int* rtp;

void parseSymbolTable() {
    // id
    symbolTable[stp].id = strtol(token, 0, 0);
    // name
    token = strtok(0, delim);
    strcpy(&symbolTable[stp].name, token);
    // sec
    token = strtok(0, delim);
    symbolTable[stp].section = strtol(token, 0, 0);
    // size
    token = strtok(0, delim);
    symbolTable[stp].size = strtol(token, 0, 0);
    // value
    token = strtok(0, delim);
    symbolTable[stp].value = strtol(token, 0, 0);
    // vis
    token = strtok(0, delim);
    symbolTable[stp].visibility = token[0];

    stp++;

    // u tabelu treba dodati samo globalne simbole i sekcije
    // u suprotnom, ne povecamo stp i sledeci red ce da ga "overwrite"/
    //if ((symbolTable[stp].visibility == GLOBAL) || (symbolTable[stp].name[0] == '.')) {
    //    stp++;
    //}

}

void calculateAddresses() {
    for (i = 0; i < stp; i++) {
        if (symbolTable[i].name[0] != '.') {
            symbolTable[i].value += symbolTable[symbolTable[i].section].value;
        }
    }
}

// Procesor
int end;
#define MEM_INPUT 0xFFFC
#define MEM_OUTPUT 0xFFFE
int memoryOffset = 0;
int cmp = 0; // current memory pointer
int memorySize;
unsigned char *memory;
unsigned char reg_in; // peripheral register
unsigned short r[8];
unsigned short psw;

unsigned short ivt[8];
int intreq[8];

#define pc r[7]
#define sp r[6]

unsigned char stack[STACK_SIZE];

int level = 0; //level of nested function calls

unsigned char pop() {
    if (sp <= 0) {
        fprintf(debug, "Stack underflow\n");
        return 0;
    }
    return stack[--sp];
}

void push(unsigned char data) {
    if (sp >= STACK_SIZE) {
        fprintf(debug, "Stack overflow\n");
        return 0;
    }
    stack[sp++] = data;
}

unsigned short memRead(int address) {
    fprintf(debug, "Reading memory on address: %x", address);

    if (address == MEM_INPUT) {
        fprintf(debug, " (%04x)\n", reg_in);
        return reg_in;
    }

    if (address == MEM_OUTPUT) {
        return 0;
    }
    
    if (address == DEFAULT_IV) {
        fprintf(debug, " (default interrupt)\n");
        return 0x00f0;
    }

    if (address >= 0 && address <= 16) {
        fprintf(debug, " (%04x)\n", ivt[address / 2]);
        return ivt[address / 2];
    }

    if (address < memoryOffset) {
        fprintf(debug, "\nInvalid address: %x", address);
        exit(1);
    }
    if (address >= memoryOffset + memorySize) {
        fprintf(debug, "\nInvalid address: %x", address);
        exit(1);
    }
    unsigned short read = (memory[address - memoryOffset]) | (memory[address + 1 - memoryOffset] << 8);
    fprintf(debug, " (%04x)\n", read);
    return read;
}

void memWrite(int address, unsigned short data) {
    fprintf(debug, "Writing memory on address: %x (%04x)\n", address, data);

    if (address == MEM_INPUT) {
        return;
    }

    if (address == MEM_OUTPUT) {
	if (data == 0x10) {
		printf("%c%c",13,10);	
	}
	else {
        	printf("%c", data);
	}        
	return;
    }

    if (address >= 0 && address <= 16) {
        ivt[address / 2] = data;
	return;
    }

    if (address < memoryOffset) {
        fprintf(debug, "Invalid address: %x\n", address);
        exit(1);
    }
    if (address >= memoryOffset + memorySize) {
        fprintf(debug, "Invalid address: %x\n", address);
        exit(1);
    }
    memory[address - memoryOffset] = data;
    memory[address - memoryOffset + 1] = data  >> 8;

}

void handleInterrupt(int i) {
    fprintf(debug, "Interrupt %d occured!\n", i);
    push(psw >> 8);
    push(psw);
    push(pc >> 8);
    push(pc);
    pc = ivt[i];
}

void *periodicInterrupt(void *vargp) 
{ 
    for(;;) {
        sleep(1); 
	intreq[1] = 1;
    }
    return NULL; 
}

void *readInputInterrupt(void *vargp) 
{ 

    char c;
    for(;;) {
	reg_in = getchar();
	//getchar();
	// Ctrl+C prekida  emulator.
	if (reg_in == 3) {
		end = 1;
		return NULL;
	}
	fprintf(debug, "Read input: %c [%04x]\n", reg_in, reg_in);
	intreq[2] = 1;
    }
    return NULL; 
} 

void main(int argc, char** argv) {

    char* inputFile = "TestIzlaz.txt";

    FILE* file_in = fopen(inputFile, "r");

    debug = fopen("EmulatorDebug.txt", "w");

    FILE* sym_tab_and_rel_test = fopen("EmulatorTables.txt", "w");


    // preskoci prvi red (#id...)
    fgets(line, MAX_SIZE, file_in);

    while (fgets(line, MAX_SIZE, file_in)) {
        // ucitaj tabelu simbola
        // samo globalne simbole i sekcije
        token = strtok(line, delim);
        if (token[0] == '#') break;
        parseSymbolTable();
    }
    calculateAddresses();

    // inicijalizacija memorije
    for (i = 0; i < stp; i++) {
        if (strcmp(symbolTable[i].name, ".text") == 0) {
            memorySize += symbolTable[i].size;
            if (!memoryOffset) memoryOffset = symbolTable[i].value;
        }
        if (strcmp(symbolTable[i].name, ".data") == 0) {
            memorySize += symbolTable[i].size;
            if (!memoryOffset) memoryOffset = symbolTable[i].value;
        }
        if (strcmp(symbolTable[i].name, ".rodata") == 0) {
            memorySize += symbolTable[i].size;
            if (!memoryOffset) memoryOffset = symbolTable[i].value;
        }
        if (strcmp(symbolTable[i].name, ".bss") == 0) {
            memorySize += symbolTable[i].size;
            if (!memoryOffset) memoryOffset = symbolTable[i].value;
        }
    }

    // init memory
    memory = calloc(memorySize, sizeof (unsigned char));
    // init ivt
    for(i = 0; i < 8; i++) {
        ivt[i] = DEFAULT_IV;
    }


    int moreLines;
    // ucitavanje relokacija
    // linija je sada "#.ret.text" ili "#.text" (ili neka druga sekcija)
    // drugim recima, linija je VEC ucitana
    do {
        int isRealocation = 0;
        int isContent = 0;
        if (strcmp(token, "#.ret.text") == 0) {
            relocationTableCurrent = relocationTableText;
            rtp = &rtpText;
            isRealocation = 1;
        }
        if (strcmp(token, "#.ret.data") == 0) {
            relocationTableCurrent = relocationTableData;
            rtp = &rtpData;
            isRealocation = 1;
        }
        if (strcmp(token, "#.ret.rodata") == 0) {
            relocationTableCurrent = relocationTableROdata;
            rtp = &rtpROData;
            isRealocation = 1;
        }
        if (strcmp(token, "#.ret.bss") == 0) {
            relocationTableCurrent = relocationTableBss;
            rtp = &rtpBss;
            isRealocation = 1;
        }
        if (strcmp(token, "#.text") == 0) {
            isContent = 1;
        }
        if (strcmp(token, "#.data") == 0) {
            isContent = 1;
        }
        if (strcmp(token, "#.rodata") == 0) {
            isContent = 1;
        }
        if (strcmp(token, "#.bss") == 0) {
            isContent = 1;
        }

        if (isRealocation) {
            // preskoci sledecu liniju
            fgets(line, MAX_SIZE, file_in);
            while ((moreLines = fgets(line, MAX_SIZE, file_in)) != 0) {
                token = strtok(line, delim);
                if (token[0] == '#') break;
                relocationTableCurrent[*rtp].address = strtol(token, 0, 16);
                token = strtok(0, delim);
                relocationTableCurrent[*rtp].type = token[0];
                token = strtok(0, delim);
                relocationTableCurrent[*rtp].symbol = strtol(token, 0, 0);
                *rtp = *rtp + 1;
            }
        }

        if (isContent) {
            while ((moreLines = fgets(line, MAX_SIZE, file_in)) != 0) {
                token = strtok(line, delim);
                if (token == 0) {
                    moreLines = 0;
                    break;
                }
                if (token[0] == '#') break;
                while (token) {
                    memory[cmp++] = strtol(token, 0, 16);
                    token = strtok(0, delim);
                }
            }
        }

    } while (moreLines);

    // razresi relokacije 
    int startAddress;
    // text
    for (i = 0; i < stp; i++) {
        if (strcmp(symbolTable[i].name, ".text") == 0) {
            startAddress = symbolTable[i].value;
        }
    }
    relocationTableCurrent = relocationTableText;
    rtp = &rtpText;
    for (i = 0; i < *rtp; i++) {
        if (relocationTableCurrent[i].type == 'A') {
            int adjustment = symbolTable[relocationTableCurrent[i].symbol].value;
            int address = startAddress + relocationTableCurrent[i].address - memoryOffset;
            unsigned short original = memory[address] + (memory[address + 1] << 8);
            unsigned short changed = original + adjustment;


            /*
            fprintf(debug,"i=%d, sym = %d, val = %0d\n",i, relocationTableCurrent[i].symbol, symbolTable[relocationTableCurrent[i].symbol - 1].value);
            fprintf(debug,"A: %02x (%0d)\n", adjustment, adjustment);
            fprintf(debug,"O: %02x\n", original);
            fprintf(debug,"C: %02x (%02x %02x)\n", changed, changed >> 8, changed);
             */


            memory[address] = changed;
            memory[address + 1] = changed >> 8;
        } else {
            // pc-relative
            int adjustment = symbolTable[relocationTableCurrent[i].symbol].value + relocationTableCurrent[i].address;
            int address = startAddress + relocationTableCurrent[i].address - memoryOffset;
            unsigned short original = memory[address] + (memory[address + 1] << 8);
            unsigned short changed = original + adjustment;
            memory[address] = changed;
            memory[address + 1] = changed >> 8;
        }
    }


    // TABELA SIMBOLA

    // header tabele simbola
    fprintf(sym_tab_and_rel_test, "%s\t", "#id");
    fprintf(sym_tab_and_rel_test, "%s\t", "#name");
    fprintf(sym_tab_and_rel_test, "%s\t", "#sec");
    fprintf(sym_tab_and_rel_test, "%s\t", "#size");
    fprintf(sym_tab_and_rel_test, "%s\t", "#value");
    fprintf(sym_tab_and_rel_test, "%s\n", "#vis");

    for (i = 0; i < stp; i++) {
        fprintf(sym_tab_and_rel_test, "%d\t", symbolTable[i].id);
        fprintf(sym_tab_and_rel_test, "%s\t", symbolTable[i].name);
        fprintf(sym_tab_and_rel_test, "%x\t", symbolTable[i].section);
        fprintf(sym_tab_and_rel_test, "%x\t", symbolTable[i].size);
        fprintf(sym_tab_and_rel_test, "%x\t", symbolTable[i].value);
        fprintf(sym_tab_and_rel_test, "%c\n", symbolTable[i].visibility);
    }



    // TABELE RELOKACIJA 

    for (i = 0; i < stp; i++) {
        if (strcmp(symbolTable[i].name, ".text") == 0) {
            fprintf(sym_tab_and_rel_test, "%s\n", "#.ret.text");
            // header tabele relokacija .text
            fprintf(sym_tab_and_rel_test, "%s\t", "#add");
            fprintf(sym_tab_and_rel_test, "%s\t", "#type");
            fprintf(sym_tab_and_rel_test, "%s\n", "#sym");

            for (j = 0; j < rtpText; j++) {
                fprintf(sym_tab_and_rel_test, "%x\t", relocationTableText[j].address);
                fprintf(sym_tab_and_rel_test, "%c\t", relocationTableText[j].type);
                fprintf(sym_tab_and_rel_test, "%d\n", relocationTableText[j].symbol);
            }

        }
    }


    for (i = 0; i < stp; i++) {
        if (strcmp(symbolTable[i].name, ".data") == 0) {
            fprintf(sym_tab_and_rel_test, "%s\n", "#.ret.data");
            // header tabele relokacija .data
            fprintf(sym_tab_and_rel_test, "%s\t", "#add");
            fprintf(sym_tab_and_rel_test, "%s\t", "#type");
            fprintf(sym_tab_and_rel_test, "%s\n", "#sym");

            for (j = 0; j < rtpData; j++) {
                fprintf(sym_tab_and_rel_test, "%x\t", relocationTableData[j].address);
                fprintf(sym_tab_and_rel_test, "%c\t", relocationTableData[j].type);
                fprintf(sym_tab_and_rel_test, "%d\n", relocationTableData[j].symbol);
            }
        }
    }


    for (i = 0; i < stp; i++) {
        if (strcmp(symbolTable[i].name, ".rodata") == 0) {
            fprintf(sym_tab_and_rel_test, "%s\n", "#.ret.rodata");
            // header tabele relokacija .rodata
            fprintf(sym_tab_and_rel_test, "%s\t", "#add");
            fprintf(sym_tab_and_rel_test, "%s\t", "#type");
            fprintf(sym_tab_and_rel_test, "%s\n", "#sym");

            for (j = 0; j < rtpROData; j++) {
                fprintf(sym_tab_and_rel_test, "%x\t", relocationTableROdata[j].address);
                fprintf(sym_tab_and_rel_test, "%c\t", relocationTableROdata[j].type);
                fprintf(sym_tab_and_rel_test, "%d\n", relocationTableROdata[j].symbol);
            }
        }
    }


    for (i = 0; i < memorySize; i++) {
        fprintf(sym_tab_and_rel_test, "%02x ", memory[i]);
        if (i % 8 == 7) fprintf(sym_tab_and_rel_test, "\n");
    }


    fclose(sym_tab_and_rel_test);
    //debug = stdout;
    debug = fopen("EmulatorDebug.txt", "a");

    // nadji "START"
    pc = 0;
    for (i = 0; i < stp; i++) {
        if (strcmp(symbolTable[i].name, "START") == 0) {
            pc = symbolTable[i].value;
            break;
        }
    }
    if (pc == 0) {
        fprintf(debug, "Nije definisan simbol \"START\"\n");
        exit(1);
    }



    intreq[0] = 1;

    // Pretpostavka: prekidi su na pocetku dozvoljeni (svi, a posebno timer).
    psw = 0xA000;
    // Kraj pretpostavke.

    // Napravi thread-ove
    pthread_t periodic_thread_id; 
    pthread_create(&periodic_thread_id, NULL, periodicInterrupt, NULL);


    system("/bin/stty -echo");
    system("/bin/stty raw");
    pthread_t input_thread_id; 
    pthread_create(&input_thread_id, NULL, readInputInterrupt, NULL);


    end = 0;
    while (!end) {

        for (i = 0; i < 8; i++) {
            if (psw & 0x8000) { // najvisi bit sluzi za maskiranje prekida
                if (intreq[i]) {
                    // prekidna rutina na ulazu 1 se izvrsava periodicno samo ako je bit 13 u PSW registru postavljen na 1
                    if ((i == 1) && (psw & 0x2000) == 0) {
                        continue;
                    }
                    handleInterrupt(i);
                    intreq[i] = 0;
                }
            }
        }

        fprintf(debug, "pc=%02x\n", pc);
        unsigned short inst = memRead(pc);
        pc += 2;
        inst = (inst >> 8) | ((inst << 8) & 0xff00);
        int condcode = inst >> 14;
        int opcode = (inst >> 10) & (0x000f);
        int dstcode = (inst >> 5) & (0x001f);
        int srccode = (inst) & (0x001f);
        int dstkind = dstcode >> 3;
        int srckind = srccode >> 3;

        int additional = 0;

        int ignoreDst = 0;
        int ignoreSrc = 0;
        switch (opcode) {
            case 9: // push
                ignoreDst = 1;
                break;
            case 10: // pop
                ignoreSrc = 1;
                break;
            case 11: // call
                ignoreDst = 1;
                break;
            case 12: //iret
                ignoreDst = ignoreSrc = 1;
                break;
        }
        if (((!ignoreDst) && (dstkind != 01)) || ((!ignoreSrc) && (srckind != 01))) {
            additional = memRead(pc);
            pc += 2;
        }

        int condition = 0;
        // uslov
        switch (condcode) {
            case 0:
                // Z = 1
                condition = psw & 1;
                break;
            case 1:
                // Z = 0
                condition = !(psw & 1);
                break;
            case 2:
                // Z = 0, N = 0
                condition = (psw & (psw >> 3) & 1);
                break;
            case 3:
                condition = 1;
                break;
        }

        if (condition) {
            unsigned short src = 0;
	    if (!ignoreSrc) {
            switch (srckind) {
                case 0:
                    src = (srccode == 0x07) ? psw : additional;
                    break;
                case 1:
                    src = r[srccode & 0x07];
                    break;
                case 2:
                    src = memRead(additional);
                    break;
                case 3:
                    ;
                    int address = r[srccode & 0x07] + additional;
                    src = memRead(address);
                    break;
            }
	    }

            unsigned short dst = 0;
	    if (!ignoreDst) {
            switch (dstkind) {
                case 0:
                    dst = psw;
                    break;
                case 1:
                    dst = r[dstcode & 0x07];
                    break;
                case 2:
                    if (additional != MEM_OUTPUT) {
                        dst = memRead(additional);
                    }
                    break;
                case 3:
                    dst = memRead(r[dstcode & 0x07] + additional);
                    break;
            }
	    }

            unsigned long result;
            switch (opcode) {

                case 0:
                    result = dst + src;
                    // add               
                    switch (dstkind) {
                        case 0:
                            psw = result;
                            break;
                        case 1:
                            r[dstcode & 0x07] = result;
                            break;
                        case 2:
                            memWrite(additional, result);
                            break;
                        case 3:
                            memWrite(r[dstcode & 0x07] + additional, result);
                            break;
                    }

                    // flags
                    int z = (result == 0) & 1;
                    int o = (result >> 16) & 1;
                    int c = (result >> 16) & 1;
                    int n = (result < 0) & 1;
                    // z: psw[0]
                    psw = (z ? (psw | (1 << 0)) : (psw & ~(1 << 0)));
                    // o: psw[1]
                    psw = (o ? (psw | (1 << 1)) : (psw & ~(1 << 1)));
                    // c: psw[2]
                    psw = (c ? (psw | (1 << 2)) : (psw & ~(1 << 2)));
                    // n: psw[3]
                    psw = (n ? (psw | (1 << 3)) : (psw & ~(1 << 3)));
                    break;
                case 1:
                    result = dst - src;
                    // sub               
                    switch (dstkind) {
                        case 0:
                            psw = result;
                            break;
                        case 1:
                            r[dstcode & 0x07] = result;
                            break;
                        case 2:
                            memWrite(additional, result);
                            break;
                        case 3:
                            memWrite(r[dstcode & 0x07] + additional, result);
                            break;
                    }
                    // flags
                    z = (result == 0) & 1;
                    o = (result >> 16) & 1;
                    c = (result >> 16) & 1;
                    n = (result < 0) & 1;
                    // z: psw[0]
                    psw = (z ? (psw | (1 << 0)) : (psw & ~(1 << 0)));
                    // o: psw[1]
                    psw = (o ? (psw | (1 << 1)) : (psw & ~(1 << 1)));
                    // c: psw[2]
                    psw = (c ? (psw | (1 << 2)) : (psw & ~(1 << 2)));
                    // n: psw[3]
                    psw = (n ? (psw | (1 << 3)) : (psw & ~(1 << 3)));
                    break;
                case 2:
                    result = dst * src;
                    // mul               
                    switch (dstkind) {
                        case 0:
                            psw = result;
                            break;
                        case 1:
                            r[dstcode & 0x07] = result;
                            break;
                        case 2:
                            memWrite(additional, result);
                            break;
                        case 3:
                            memWrite(r[dstcode & 0x07] + additional, result);
                            break;
                    }
                    // flags
                    z = (result == 0) & 1;
                    n = (result < 0) & 1;
                    // z: psw[0]
                    psw = (z ? (psw | (1 << 0)) : (psw & ~(1 << 0)));
                    // n: psw[3]
                    psw = (n ? (psw | (1 << 3)) : (psw & ~(1 << 3)));
                    break;
                case 3:
                    result = dst / src;
                    // div               
                    switch (dstkind) {
                        case 0:
                            psw = result;
                            break;
                        case 1:
                            r[dstcode & 0x07] = result;
                            break;
                        case 2:
                            memWrite(additional, result);
                            break;
                        case 3:
                            memWrite(r[dstcode & 0x07] + additional, result);
                            break;
                    }
                    // flags
                    z = (result == 0) & 1;
                    n = (result < 0) & 1;
                    // z: psw[0]
                    psw = (z ? (psw | (1 << 0)) : (psw & ~(1 << 0)));
                    // n: psw[3]
                    psw = (n ? (psw | (1 << 3)) : (psw & ~(1 << 3)));
                    break;
                case 4:
                    result = dst - src;
                    // cmp
                    // only update flags
                    // flags
                    z = (result == 0) & 1;
                    o = (result >> 16) & 1;
                    c = (result >> 16) & 1;
                    n = (result < 0) & 1;
                    // z: psw[0]
                    psw = (z ? (psw | (1 << 0)) : (psw & ~(1 << 0)));
                    // o: psw[1]
                    psw = (o ? (psw | (1 << 1)) : (psw & ~(1 << 1)));
                    // c: psw[2]
                    psw = (c ? (psw | (1 << 2)) : (psw & ~(1 << 2)));
                    // n: psw[3]
                    psw = (n ? (psw | (1 << 3)) : (psw & ~(1 << 3)));
                    break;
                case 5:
                    result = dst & src;
                    // and               
                    switch (dstkind) {
                        case 0:
                            psw = result;
                            break;
                        case 1:
                            r[dstcode & 0x07] = result;
                            break;
                        case 2:
                            memWrite(additional, result);
                            break;
                        case 3:
                            memWrite(r[dstcode & 0x07] + additional, result);
                            break;
                    }
                    // flags
                    z = (result == 0) & 1;
                    n = (result < 0) & 1;
                    // z: psw[0]
                    psw = (z ? (psw | (1 << 0)) : (psw & ~(1 << 0)));
                    // n: psw[3]
                    psw = (n ? (psw | (1 << 3)) : (psw & ~(1 << 3)));
                    break;
                case 6:
                    result = dst | src;
                    // or               
                    switch (dstkind) {
                        case 0:
                            psw = result;
                            break;
                        case 1:
                            r[dstcode & 0x07] = result;
                            break;
                        case 2:
                            memWrite(additional, result);
                            break;
                        case 3:
                            memWrite(r[dstcode & 0x07] + additional, result);
                            break;
                    }
                    // flags
                    z = (result == 0) & 1;
                    n = (result < 0) & 1;
                    // z: psw[0]
                    psw = (z ? (psw | (1 << 0)) : (psw & ~(1 << 0)));
                    // n: psw[3]
                    psw = (n ? (psw | (1 << 3)) : (psw & ~(1 << 3)));
                    break;
                case 7:
                    result = ~src;
                    // not               
                    switch (dstkind) {
                        case 0:
                            psw = result;
                            break;
                        case 1:
                            r[dstcode & 0x07] = result;
                            break;
                        case 2:
                            memWrite(additional, result);
                            break;
                        case 3:
                            memWrite(r[dstcode & 0x07] + additional, result);
                            break;
                    }
                    // flags
                    z = (result == 0) & 1;
                    n = (result < 0) & 1;
                    // z: psw[0]
                    psw = (z ? (psw | (1 << 0)) : (psw & ~(1 << 0)));
                    // n: psw[3]
                    psw = (n ? (psw | (1 << 3)) : (psw & ~(1 << 3)));
                    break;
                case 8:
                    result = dst & src;
                    // test               
                    // flags
                    z = (result == 0) & 1;
                    n = (result < 0) & 1;
                    // z: psw[0]
                    psw = (z ? (psw | (1 << 0)) : (psw & ~(1 << 0)));
                    // n: psw[3]
                    psw = (n ? (psw | (1 << 3)) : (psw & ~(1 << 3)));
                    break;
                case 9:
                    // push
                    push(src >> 8);
                    push(src);
		    break;
                case 10:
                    // pop
                    // r7 = [01][111] = 0f
                    if (dstcode == 0x0f) {
                        if (--level < 0) {
                            end = 1;
                            break;
                        };
                    }
                    result = pop() | (pop() << 8);
                    switch (dstkind) {
                        case 0:
                            psw = result;
                            break;
                        case 1:
                            r[dstcode & 0x07] = result;
                            break;
                        case 2:
                            memWrite(additional, result);
                            break;
                        case 3:
                            memWrite(r[dstcode & 0x07] + additional, result);
                            break;
                    }
                    break;
                case 11:
                    // call
                    level++;
                    push(pc >> 8);
                    push(pc);
                    pc = src;
                    break;
                case 12:
                    //iret
                    pc = pop() | (pop() << 8);
                    psw = pop() | (pop() << 8);
                    break;
                case 13:
                    //mov
                    result = src;

                    switch (dstkind) {
                        case 0:
                            psw = result;
                            break;
                        case 1:
                            r[dstcode & 0x07] = result;
                            break;
                        case 2:
                            memWrite(additional, result);
                            break;
                        case 3:
                            memWrite(r[dstcode & 0x07] + additional, result);
                            break;
                    }
                    // flags
                    z = (result == 0) & 1;
                    n = (result < 0) & 1;
                    // z: psw[0]
                    psw = (z ? (psw | (1 << 0)) : (psw & ~(1 << 0)));
                    // n: psw[3]
                    psw = (n ? (psw | (1 << 3)) : (psw & ~(1 << 3)));
                    //end = 1;
                    break;
                case 14:
                    result = dst << src;
                    // shl               
                    switch (dstkind) {
                        case 0:
                            psw = result;
                            break;
                        case 1:
                            r[dstcode & 0x07] = result;
                            break;
                        case 2:
                            memWrite(additional, result);
                            break;
                        case 3:
                            memWrite(r[dstcode & 0x07] + additional, result);
                            break;
                    }

                    // flags
                    z = (result == 0) & 1;
                    c = (result >> 16) & 1;
                    n = (result < 0) & 1;
                    // z: psw[0]
                    psw = (z ? (psw | (1 << 0)) : (psw & ~(1 << 0)));
                    // c: psw[2]
                    psw = (c ? (psw | (1 << 2)) : (psw & ~(1 << 2)));
                    // n: psw[3]
                    psw = (n ? (psw | (1 << 3)) : (psw & ~(1 << 3)));
                    break;
                case 15:
                    result = dst >> src;
                    // shr               
                    switch (dstkind) {
                        case 0:
                            psw = result;
                            break;
                        case 1:
                            r[dstcode & 0x07] = result;
                            break;
                        case 2:
                            memWrite(additional, result);
                            break;
                        case 3:
                            memWrite(r[dstcode & 0x07] + additional, result);
                            break;
                    }

                    // flags
                    z = (result == 0) & 1;
                    c = (result >> 16) & 1;
                    n = (result < 0) & 1;
                    // z: psw[0]
                    psw = (z ? (psw | (1 << 0)) : (psw & ~(1 << 0)));
                    // c: psw[2]
                    psw = (c ? (psw | (1 << 2)) : (psw & ~(1 << 2)));
                    // n: psw[3]
                    psw = (n ? (psw | (1 << 3)) : (psw & ~(1 << 3)));
                    break;
            }



            //debug
            fprintf(debug, "\tPC: %d (%04x)\n\tCond: %d (%d)\n\tOpcode: %d\n\tDst: %04x (%02x)\n\tSrc: %04x (%02x)\n\tA: %04x\n", pc, inst, condcode, condition, opcode, dst, dstcode, src, srccode, additional);
            for (i = 0; i < 8; i++) {
                fprintf(debug, "\t\tr%d = %04x\n", i, r[i]);
            }
            fprintf(debug, "\t\tpsw= %04x\n", psw);

        } // if <cond>
    } // while

    pthread_cancel(periodic_thread_id);
    pthread_cancel(input_thread_id);
    system("/bin/stty cooked");
    system("/bin/stty echo");

    fclose(debug);




    return (EXIT_SUCCESS);
}

