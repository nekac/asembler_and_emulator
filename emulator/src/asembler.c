#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// *** STRUKTURE PODATAKA ***

#define MAX_SIZE 1000 

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


Symbol symbolTable[MAX_SIZE]; // predefined
int stp = 0; // symbol table pointer
int currentSection;
int LC; // local counter

unsigned char sectionText[MAX_SIZE] = {0};
unsigned char sectionData[MAX_SIZE] = {0};
unsigned char sectionROData[MAX_SIZE] = {0};
unsigned char sectionBss[MAX_SIZE] = {0};
unsigned char *sectionCurrent;

#define PC_RELATIVE 'R'
#define ABSOLUTE 'A'


// relokacija koja se unosi u tabelu relokacija

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



// *** TOKENIZACIJA ***

char* token;
char* delim = " ,\t\n\r\b"; // sve vrste delimitera
int theEnd = 0; // da li se naislo na .end u ulaznom programu

// prepoznavanje labele

int isLabel(char* token) {
    return token[strlen(token) - 1] == ':';
}

// prepoznavanje direktive

int isDirective(char* token) {
    return token[0] == '.';
}



// ***************** PRVI PROLAZ *****************

void parseLabel1(char* token) {
    char cleanToken[MAX_SIZE];
    strcpy(&cleanToken, token);
    cleanToken[strlen(token) - 1] = '\0';

    // printf("[%s] - [%s]\n", token, cleanToken);

    for (i = 0; i < stp; i++) {
        if (strcmp(symbolTable[i].name, &cleanToken) == 0) {
            // printf("[%s] - [%s]\n", symbolTable[i].name, cleanToken);
            if (symbolTable[i].section == 0) { // da li je globalan ili lokalno definisan	
                symbolTable[i].section = currentSection;
                symbolTable[i].value = LC - symbolTable[currentSection].value;
                return;
            } else {
                printf("GRESKA! [%s]\n", cleanToken);
                exit(1);
            }
        }
    }
    symbolTable[stp].id = stp;
    strcpy(&symbolTable[stp].name, token);
    // brisanje dvotacke (':')
    symbolTable[stp].name[strlen(token) - 1] = '\0';
    symbolTable[stp].section = currentSection;
    symbolTable[stp].size = 0;
    symbolTable[stp].value = LC - symbolTable[currentSection].value;
    symbolTable[stp].visibility = LOCAL;
    stp++;
}

void parseDirective1(char* token) {
    // .global
    if (strcmp(token, ".global") == 0) {
        while (token = strtok(0, delim)) {
            for (i = 0; i < stp; i++) {
                if (strcmp(symbolTable[i].name, token) == 0) {
                    printf("GRESKA! [%s]\n", token);
                    exit(1);
                }
            }
            symbolTable[stp].id = stp;
            strcpy(&symbolTable[stp].name, token);
            symbolTable[stp].section = currentSection;
            symbolTable[stp].size = 0;
            symbolTable[stp].value = 0;
            symbolTable[stp].visibility = GLOBAL;
            stp++;
        }
        return;
    }

    // .section
    if ((strcmp(token, ".text") == 0)
            ||
            (strcmp(token, ".data") == 0)
            ||
            (strcmp(token, ".rodata") == 0)
            ||
            (strcmp(token, ".bss") == 0)) {

        if (currentSection) // velicina tekuce sekcije je trenutna lokacija - adresa pocetka sekcije
            symbolTable[currentSection].size = LC - symbolTable[currentSection].value;

        symbolTable[stp].id = stp;
        strcpy(&symbolTable[stp].name, token);
        symbolTable[stp].section = stp;
        symbolTable[stp].size = 0;
        symbolTable[stp].value = LC;
        symbolTable[stp].visibility = LOCAL;
        currentSection = stp;
        stp++;
        return;
    }

    // .char
    if (strcmp(token, ".char") == 0) {
        if (strcmp(symbolTable[currentSection].name, ".bss") == 0) {
            printf("GRESKA! [%s] u BSS\n", token);
            exit(1);
        }
        token = strtok(0, delim);
        LC += 1;
        return;
    }

    // .word
    if (strcmp(token, ".word") == 0) {
        if (strcmp(symbolTable[currentSection].name, ".bss") == 0) {
            printf("GRESKA! [%s] u BSS\n", token);
            exit(1);
        }
        token = strtok(0, delim);
        LC += 2;
        return;
    }

    // .long
    if (strcmp(token, ".long") == 0) {
        if (strcmp(symbolTable[currentSection].name, ".bss") == 0) {
            printf("GRESKA! [%s] u BSS\n", token);
            exit(1);
        }
        token = strtok(0, delim);
        LC += 4;
        return;
    }

    // .align
    if (strcmp(token, ".align") == 0) {
        token = strtok(0, delim);
        int a = LC;
        int b = strtol(token, 0, 0);
        ; // string to int
        // prvi najblizi stepen 2 (token MORA da bude stepen 2, pravilo postavke)
        int c = (a % b == 0) ? 0 : (b - a % b);
        LC += c;
        return;
    }

    // .skip
    if (strcmp(token, ".skip") == 0) {
        token = strtok(0, delim);
        int skip = strtol(token, 0, 0);
        LC += skip; // koliko da preskoci, doda na brojac
        return;
    }

    // .end
    if (strcmp(token, ".end") == 0) {
        symbolTable[currentSection].size = LC - symbolTable[currentSection].value;
        theEnd = 1;
        return;
    }

    // pogresna direktiva
    printf("GRESKA: Nije pravilno zadata direktiva u testu! [%s]\n", token);
    exit(1);
}






// *** VRSTE ADRESIRANJA ***

// samo u ovom slucaju ima jos 2B i dodaju se na mestu parsiranja prilikom 1. ili 2. prolaza

int isRegDir(char* token) { // r3 – direktno registarsko
    return
    (strcmp(token, "r0") == 0)
            ||
            (strcmp(token, "r1") == 0)
            ||
            (strcmp(token, "r2") == 0)
            ||
            (strcmp(token, "r3") == 0)
            ||
            (strcmp(token, "r4") == 0)
            ||
            (strcmp(token, "r5") == 0)
            ||
            (strcmp(token, "r6") == 0)
            ||
            (strcmp(token, "r7") == 0)
            ||
            (strcmp(token, "psw") == 0);
}

int isMemDir(char* token) { // x – memorijsko direktno adresiranje
    for (i = 0; i < stp; i++) {
        if (strcmp(symbolTable[i].name, token) == 0)
            return 1;
    }
    return 0;
}

int isMemDirLoc(char* token) { // *20 – lokacija sa adrese 20
    return token[0] == '*';
}

int isRegDirOff(char* token) { // r4[32] – registarsko indirektno sa pomerajem
    if (strlen(token) < 4) return 0;
    char token2[MAX_SIZE];
    strcpy(token2, token);
    if ((token2[0] == 'r')
            ||
            (token2[2] == '[')
            ||
            (token2[strlen(token2) - 1] == ']')
            ) {
        // jeste rx[...], da li je rx[100] ili rx[n]
        char* cleanToken = &token2[3];
        cleanToken[strlen(cleanToken) - 1] = '\0';
        for (i = 0; i < stp; i++) {
            if (strcmp(cleanToken, symbolTable[i].name) == 0) {
                return 0;
            }
        }
        return 1;
    }
    // nije rx[...]
    return 0;
}

int isRegDirSym(char* token) { // r5[x] – registarsko indirektno sa pomerajem
    if (strlen(token) < 4) return 0;
    char token2[MAX_SIZE];
    strcpy(token2, token);
    if ((token2[0] == 'r')
            ||
            (token2[2] == '[')
            ||
            (token2[strlen(token) - 1] == ']')
            ) {
        // jeste rx[...], da li je rx[100] ili rx[n]
        char* cleanToken = &token2[3];
        cleanToken[strlen(cleanToken) - 1] = '\0';
        for (i = 0; i < stp; i++) {
            if (strcmp(cleanToken, symbolTable[i].name) == 0) {
                return 1;
            }
        }
        return 0;
    }
    // nije rx[...]
    return 0;
}

int isPCRel(char* token) { // $x – PC relativno adresiranje promenljive x
    return token[0] == '$';
}

int isImmSym(char* token) { // &x – vrednost simbola x
    return token[0] == '&';
}

int additional = 0; // dodatna 2B
int additionalFlag = 0; // da li nam trebaju dodatna 2B flag

void parseInstruction1(char* token) { // rezervisanje prostora u zavisnosti od instrukcije
    char* cond = &token[strlen(token) - 2];
    char cleanToken[MAX_SIZE];
    strcpy(&cleanToken, token);

    if (strcmp(symbolTable[currentSection].name, ".bss") == 0) {
        printf("GRESKA! [%s] u BSS\n", token);
        exit(1);
    }

    if ((strcmp(cond, "eq") == 0)
            ||
            (strcmp(cond, "ne") == 0)
            ||
            (strcmp(cond, "gt") == 0)
            ||
            (strcmp(cond, "al") == 0)
            ) {
        cleanToken[strlen(token) - 2] = '\0';
    }

    if ((strcmp(cleanToken, "add") == 0)
            ||
            (strcmp(cleanToken, "sub") == 0)
            ||
            (strcmp(cleanToken, "mul") == 0)
            ||
            (strcmp(cleanToken, "div") == 0)
            ||
            (strcmp(cleanToken, "cmp") == 0) // samo setuje PSW
            ||
            (strcmp(cleanToken, "and") == 0)
            ||
            (strcmp(cleanToken, "or") == 0)
            ||
            (strcmp(cleanToken, "not") == 0)
            ||
            (strcmp(cleanToken, "test") == 0) // samo setuje PSW
            ||
            (strcmp(cleanToken, "mov") == 0)
            ||
            (strcmp(cleanToken, "shl") == 0)
            ||
            (strcmp(cleanToken, "shr") == 0)
            ) {
        additionalFlag = 0;
        LC += 2;
        token = strtok(0, delim);
        if (!isRegDir(token)) {
            LC += 2;
            additionalFlag = 1;
        }
        token = strtok(0, delim);
        if (!isRegDir(token)) {
            if (additionalFlag) {
                printf("GRESKA! [%s]\n", token);
                exit(1);
            }
            LC += 2;
        }
        return;
    }

    if ((strcmp(cleanToken, "push") == 0)
            ||
            (strcmp(cleanToken, "pop") == 0)
            ||
            (strcmp(cleanToken, "call") == 0)
            ||
            (strcmp(cleanToken, "jmp") == 0)) // kao mov ili add
    {
        LC += 2;
        token = strtok(0, delim);
        if (!isRegDir(token)) {
            LC += 2;
        }
        return;
    }

    if ((strcmp(cleanToken, "iret") == 0)
            ||
            (strcmp(cleanToken, "ret") == 0)) // kao pop
    {
        LC += 2;
        return;
    }

    printf("GRESKA! [%s]\n", token);
    exit(1);
}






// ***************** DRUGI PROLAZ *****************

void parseDirective2(char* token) {
    // .global
    if (strcmp(token, ".global") == 0) {
        return;
    }
    // .section
    if (strcmp(token, ".text") == 0) {
        sectionCurrent = sectionText;
        relocationTableCurrent = relocationTableText;
        rtp = &rtpText;
        // nadji u tabeli simbola
        for (i = 0; i < stp; i++) {
            if (strcmp(token, symbolTable[i].name) == 0) {
                currentSection = i;
                // LC = symbolTable[i].value;
                break;
            }
        }
        return;
    }

    if (strcmp(token, ".data") == 0) {
        sectionCurrent = sectionData;
        relocationTableCurrent = relocationTableData;
        rtp = &rtpData;
        // nadji u tabeli simbola
        for (i = 0; i < stp; i++) {
            if (strcmp(token, symbolTable[i].name) == 0) {
                currentSection = i;
                // LC = symbolTable[i].value;
                break;
            }
        }
        return;
    }

    if (strcmp(token, ".rodata") == 0) {
        sectionCurrent = sectionROData;
        relocationTableCurrent = relocationTableROdata;
        rtp = &rtpROData;
        // nadji u tabeli simbola
        for (i = 0; i < stp; i++) {
            if (strcmp(token, symbolTable[i].name) == 0) {
                currentSection = i;
                // LC = symbolTable[i].value;
                break;
            }
        }
        return;
    }

    if (strcmp(token, ".bss") == 0) {
        sectionCurrent = sectionBss;
        relocationTableCurrent = relocationTableBss;
        rtp = &rtpBss;
        // nadji u tabeli simbola
        for (i = 0; i < stp; i++) {
            if (strcmp(token, symbolTable[i].name) == 0) {
                currentSection = i;
                // LC = symbolTable[i].value;
                break;
            }
        }
        return;
    }

    // .char
    if (strcmp(token, ".char") == 0) {
        unsigned char data = 0;
        token = strtok(0, delim);
        if (isMemDir(token)) {
            // simbol (.char x)
            for (i = 0; i < stp; i++) {
                if (strcmp(token, symbolTable[i].name) == 0) {
                    data = symbolTable[i].value;
                    relocationTableCurrent[*rtp].address = LC - symbolTable[currentSection].value;
                    relocationTableCurrent[*rtp].type = ABSOLUTE;

                    if (symbolTable[i].visibility == GLOBAL) {
                        relocationTableCurrent[*rtp].symbol = symbolTable[i].id; // referenca ka tom simbolu
                    } else {
                        relocationTableCurrent[*rtp].symbol = symbolTable[i].section; // referenca ka sekciji u kojoj je
                    }
                    *rtp = *rtp + 1;
                    break;
                }
            }
        } else {
            // vrednost (.char 100)
            data = strtol(token, 0, 0);
        }
        // printf("[%s] -> %x (%d)\n", token, data, data);
        sectionCurrent[LC - symbolTable[currentSection].value] = data;
        // printf("%d = %d - %d\n", LC - symbolTable[currentSection].value, LC, symbolTable[currentSection].value);
        LC += 1;
        return;
    }

    // .word
    if (strcmp(token, ".word") == 0) {
        token = strtok(0, delim);
        unsigned short data = 0;
        if (isMemDir(token)) {
            // simbol (.word x)
            for (i = 0; i < stp; i++) {
                if (strcmp(token, symbolTable[i].name) == 0) {
                    data = symbolTable[i].value;
                    relocationTableCurrent[*rtp].address = LC - symbolTable[currentSection].value;
                    relocationTableCurrent[*rtp].type = ABSOLUTE;

                    if (symbolTable[i].visibility == GLOBAL) {
                        relocationTableCurrent[*rtp].symbol = symbolTable[i].id; // referenca ka tom simbolu
                    } else {
                        relocationTableCurrent[*rtp].symbol = symbolTable[i].section; // referenca ka sekciji u kojoj je
                    }
                    *rtp = *rtp + 1;
                    break;
                }
            }
        } else {
            // vrednost (.word 100)
            data = strtol(token, 0, 0);
        }
        sectionCurrent[LC - symbolTable[currentSection].value + 1] = (data & 0xFF00) >> 8;
        sectionCurrent[LC - symbolTable[currentSection].value] = (data & 0x00FF);
        LC += 2;
        return;
    }

    // .long
    if (strcmp(token, ".long") == 0) {
        token = strtok(0, delim);
        unsigned long data = 0;
        if (isMemDir(token)) {
            // simbol (.long x)
            for (i = 0; i < stp; i++) {
                if (strcmp(token, symbolTable[i].name) == 0) {
                    data = symbolTable[i].value;
                    relocationTableCurrent[*rtp].address = LC - symbolTable[currentSection].value;
                    relocationTableCurrent[*rtp].type = ABSOLUTE;

                    if (symbolTable[i].visibility == GLOBAL) {
                        relocationTableCurrent[*rtp].symbol = symbolTable[i].id; // referenca ka tom simbolu
                    } else {
                        relocationTableCurrent[*rtp].symbol = symbolTable[i].section; // referenca ka sekciji u kojoj je
                    }
                    *rtp = *rtp + 1;
                    break;
                }
            }
        } else {
            // vrednost (.long 100)
            data = strtol(token, 0, 0);
        }
        sectionCurrent[LC - symbolTable[currentSection].value + 3] = (data & 0xFF000000) >> 24;
        sectionCurrent[LC - symbolTable[currentSection].value + 2] = (data & 0x00FF0000) >> 16;
        sectionCurrent[LC - symbolTable[currentSection].value + 1] = (data & 0x0000FF00) >> 8;
        sectionCurrent[LC - symbolTable[currentSection].value] = (data & 0x000000FF);
        LC += 4;
        return;
    }

    // .align
    if (strcmp(token, ".align") == 0) {
        token = strtok(0, delim);
        int a = LC;
        int b = strtol(token, 0, 0);
        // prvi najblizi stepen 2 (token MORA da bude stepen 2, pravilo)
        int c = (a % b == 0) ? 0 : (b - a % b);
        for (i = 0; i < c; i++) {
            sectionCurrent[LC - symbolTable[currentSection].value + i] = 0;
        }
        LC += c;
        return;
    }

    // .skip
    if (strcmp(token, ".skip") == 0) {
        token = strtok(0, delim);
        int skip = strtol(token, 0, 0);
        for (i = 0; i < skip; i++) {
            sectionCurrent[LC - symbolTable[currentSection].value + i] = 0;
        }
        LC += skip;
        return;
    }

    // .end
    if (strcmp(token, ".end") == 0) {
        theEnd = 1;
        return;
    }

    // pogresna direktiva
    printf("GRESKA: Nije pravilno zadata direktiva u testu! [%s]\n", token);
    exit(1);
}



int opCode = 0;
int dst = 0;
int src = 0;
unsigned short instruction = 0;


jmp_flag = 0;
int getOperand(char* token) { // dohvatanje dst ili src za operacije, podela prema adresiranju

    int operand; // lokalna za dst ili src vrednost

    if (isImmSym(token)) {
        operand = 0b00000;
        char* cleanToken = &token[1];
        for (i = 0; i < stp; i++) {
            if (strcmp(cleanToken, symbolTable[i].name) == 0) {
                additional = symbolTable[i].value;
                additionalFlag = 1;
                // pravi apsolutnu relokaciju u odnosu na nesto
                relocationTableCurrent[*rtp].address = LC - symbolTable[currentSection].value;
                relocationTableCurrent[*rtp].type = ABSOLUTE;

                if (symbolTable[i].visibility == GLOBAL) {
                    relocationTableCurrent[*rtp].symbol = symbolTable[i].id;
                    additional = 0;
                } else {
                    relocationTableCurrent[*rtp].symbol = symbolTable[i].section;
                }
                *rtp = *rtp + 1;
                break;
            }
        }
    } else if (isMemDir(token)) {
        operand = 0b10000;
        for (i = 0; i < stp; i++) {
            if (strcmp(token, symbolTable[i].name) == 0) {
                additional = symbolTable[i].value;
                additionalFlag = 1;
                // pravi apsolutnu relokaciju u odnosu na nesto
                relocationTableCurrent[*rtp].address = LC - symbolTable[currentSection].value;
                relocationTableCurrent[*rtp].type = ABSOLUTE;

                if (symbolTable[i].visibility == GLOBAL) {
                    relocationTableCurrent[*rtp].symbol = symbolTable[i].id;
                    additional = 0;
                } else {
                    relocationTableCurrent[*rtp].symbol = symbolTable[i].section;
                }
                *rtp = *rtp + 1;
                break;
            }
        }
    } else if (isMemDirLoc(token)) {
        operand = 0b10000;
        additional = strtol(&token[1], 0, 0);
        additionalFlag = 1;
    } else if (isRegDir(token)) {
        if (strcmp(token, "psw") == 0) {
            operand = 0b00111;
        } else {
            int regnum = strtol(&token[1], 0, 0);
            operand = 0b01000 | regnum;
        }
    } else if (isRegDirOff(token)) {
        int regnum = token[1] - '0';
        operand = 0b11000 | regnum;
        char cleanToken[MAX_SIZE];
        strcpy(cleanToken, &token[3]);
        cleanToken[strlen(cleanToken) - 1] = '\0';
        additional = strtol(cleanToken, 0, 0);
        additionalFlag = 1;
    } else if (isRegDirSym(token)) {
        int regnum = token[1] - '0';
        operand = 0b11000 | regnum;
        char cleanToken[MAX_SIZE];
        strcpy(cleanToken, &token[3]);
        cleanToken[strlen(cleanToken) - 1] = '\0';
        for (i = 0; i < stp; i++) {
            if (strcmp(symbolTable[i].name, cleanToken) == 0) {
                additional = symbolTable[i].value;
                additionalFlag = 1;
                // pravi apsolutnu relokaciju u odnosu na nesto
                relocationTableCurrent[*rtp].address = LC - symbolTable[currentSection].value;
                relocationTableCurrent[*rtp].type = ABSOLUTE;

                if (symbolTable[i].visibility == GLOBAL) {
                    relocationTableCurrent[*rtp].symbol = symbolTable[i].id;
                    additional = 0;
                } else {
                    relocationTableCurrent[*rtp].symbol = symbolTable[i].section;
                }
                *rtp = *rtp + 1;
                break;
            }
        }
    } else if (isPCRel(token)) {
        operand = 0b11000; //reg ind pom
        char* cleanToken = &token[1];
        for (i = 0; i < stp; i++) {
            if (strcmp(cleanToken, symbolTable[i].name) == 0) {
                additional = symbolTable[i].value - 2;
                additionalFlag = 1;
		// ako je simbol u istoj sekciji, i u pitanju je skok, ne pravi relokaciju
		if (jmp_flag) {
		    operand = 0b00000;
		    if (symbolTable[i].section == currentSection) {
                    	additional = symbolTable[currentSection].value + symbolTable[i].value - (LC+2);
		    	break;
                }

		}
                // pravi reativnu relokaciju u odnosu na nesto
                relocationTableCurrent[*rtp].address = LC - symbolTable[currentSection].value;
                relocationTableCurrent[*rtp].type = PC_RELATIVE;

                if (symbolTable[i].visibility == GLOBAL) {
                    relocationTableCurrent[*rtp].symbol = symbolTable[i].id;
                    additional = -2;
                } else {
                    relocationTableCurrent[*rtp].symbol = symbolTable[i].section;
                }
                *rtp = *rtp + 1;
                break;
            }
        }
    } else /* poslednja opcija isImm(token) */ { // 20 – neposredna vrednost 20
        operand = 0b00000;
        additional = strtol(token, 0, 0);
        additionalFlag = 1;
    }

    return operand;
}

void parseInstruction2(char* token) {
    char* cond = &token[strlen(token) - 2];
    char cleanToken[MAX_SIZE];
    strcpy(&cleanToken, token);


    // USLOVI
    dst = 0;
    src = 0;

    int condCode = 0b11; // al po defaultu, bezuslovno
    if (strcmp(cond, "eq") == 0) {
        condCode = 0b00;
        cleanToken[strlen(token) - 2] = '\0';
    }
    if (strcmp(cond, "ne") == 0) {
        condCode = 0b01;
        cleanToken[strlen(token) - 2] = '\0';
    }
    if (strcmp(cond, "gt") == 0) {
        condCode = 0b10;
        cleanToken[strlen(token) - 2] = '\0';
    }
    if (strcmp(cond, "al") == 0) {
        condCode = 0b11;
        cleanToken[strlen(token) - 2] = '\0';
    }


    // OPERACIJE

    additional = 0;
    additionalFlag = 0;

    opCode = 0;
    dst = 0;
    src = 0;
    instruction = 0;

    if (strcmp(cleanToken, "add") == 0) {
        LC += 2;
        opCode = 0b0000; // menja se za svaku operaciju

        // dst
        token = strtok(0, delim);
        dst = getOperand(token);

        // src
        token = strtok(0, delim);
        src = getOperand(token);
    }

    if (strcmp(cleanToken, "sub") == 0) {
        LC += 2;
        opCode = 0b0001; // menja se za svaku operaciju

        // dst
        token = strtok(0, delim);
        dst = getOperand(token);

        // src
        token = strtok(0, delim);
        src = getOperand(token);
    }

    if (strcmp(cleanToken, "mul") == 0) {
        LC += 2;
        opCode = 0b0010; // menja se za svaku operaciju

        // dst
        token = strtok(0, delim);
        dst = getOperand(token);

        // src
        token = strtok(0, delim);
        src = getOperand(token);
    }

    if (strcmp(cleanToken, "div") == 0) {
        LC += 2;
        opCode = 0b0011; // menja se za svaku operaciju

        // dst
        token = strtok(0, delim);
        dst = getOperand(token);

        // src
        token = strtok(0, delim);
        src = getOperand(token);
    }

    if (strcmp(cleanToken, "cmp") == 0) {
        LC += 2;
        opCode = 0b0100; // menja se za svaku operaciju

        // dst
        token = strtok(0, delim);
        dst = getOperand(token);

        // src
        token = strtok(0, delim);
        src = getOperand(token);
    }

    if (strcmp(cleanToken, "and") == 0) {
        LC += 2;
        opCode = 0b0101; // menja se za svaku operaciju

        // dst
        token = strtok(0, delim);
        dst = getOperand(token);

        // src
        token = strtok(0, delim);
        src = getOperand(token);
    }

    if (strcmp(cleanToken, "or") == 0) {
        LC += 2;
        opCode = 0b0110; // menja se za svaku operaciju

        // dst
        token = strtok(0, delim);
        dst = getOperand(token);

        // src
        token = strtok(0, delim);
        src = getOperand(token);
    }

    if (strcmp(cleanToken, "not") == 0) {
        LC += 2;
        opCode = 0b0111; // menja se za svaku operaciju

        // dst
        token = strtok(0, delim);
        dst = getOperand(token);

        // src
        token = strtok(0, delim);
        src = getOperand(token);
    }

    if (strcmp(cleanToken, "test") == 0) {
        LC += 2;
        opCode = 0b1000; // menja se za svaku operaciju

        // dst
        token = strtok(0, delim);
        dst = getOperand(token);

        // src
        token = strtok(0, delim);
        src = getOperand(token);
    }

    if (strcmp(cleanToken, "mov") == 0) {
        LC += 2;
        opCode = 0b1101; // menja se za svaku operaciju

        // dst
        token = strtok(0, delim);
        dst = getOperand(token);

        // src
        token = strtok(0, delim);
        src = getOperand(token);
    }

    if (strcmp(cleanToken, "shl") == 0) {
        LC += 2;
        opCode = 0b1110; // menja se za svaku operaciju

        // dst
        token = strtok(0, delim);
        dst = getOperand(token);

        // src
        token = strtok(0, delim);
        src = getOperand(token);
    }

    if (strcmp(cleanToken, "shr") == 0) {
        LC += 2;
        opCode = 0b1111; // menja se za svaku operaciju

        // dst
        token = strtok(0, delim);
        dst = getOperand(token);

        // src
        token = strtok(0, delim);
        src = getOperand(token);
    }

    if (strcmp(cleanToken, "push") == 0) {
        LC += 2;
        opCode = 0b1001;
        token = strtok(0, delim);
        src = getOperand(token);
    }

    if (strcmp(cleanToken, "pop") == 0) {
        LC += 2;
        opCode = 0b1010;
        token = strtok(0, delim);
        dst = getOperand(token);
    }

    if (strcmp(cleanToken, "call") == 0) {
        LC += 2;
        opCode = 0b1011;
        token = strtok(0, delim);
	jmp_flag = 1;
        src = getOperand(token);
	jmp_flag = 0;
    }

    if (strcmp(cleanToken, "jmp") == 0) { // kao mov ili add
        LC += 2;
        dst = 0b01111; // reg_dir, r7 (01-111)
        token = strtok(0, delim);
        opCode = (isPCRel(token) ? 0b0000 : 0b1101); // jmp $x -> add pc, off(x), jmp x -> mov pc, x
        jmp_flag = 1;
        src = getOperand(token);
        jmp_flag = 0;
    }

    if (strcmp(cleanToken, "iret") == 0) {
        LC += 2;
        opCode = 0b1100;
    }

    if (strcmp(cleanToken, "ret") == 0) { // kao pop
        LC += 2;
        opCode = 0b1010; // pop pc (r7)
        dst = 0b01111; // reg dir, pc=r7 (01-111)
    }

    // na kraju svih if-ova, na kraju parseInstruction, ovo za svaku funkciju
    instruction = condCode << 14;
    instruction |= opCode << 10;
    instruction |= dst << 5;
    instruction |= src;
    // printf("Adddr = %d, instruction = %02x\n", (LC - 2 - symbolTable[currentSection].value), instruction);
    sectionCurrent[LC - 2 - symbolTable[currentSection].value] = instruction >> 8;
    sectionCurrent[LC - 2 - symbolTable[currentSection].value + 1] = instruction;

    if (additionalFlag) {
        sectionCurrent[LC - symbolTable[currentSection].value] = additional;
        sectionCurrent[LC - symbolTable[currentSection].value + 1] = additional >> 8;
        LC += 2;
    }
}




// ***************** MAIN PROGRAM POZIV I KREIRANJE *****************

void main(int argc, char* argv[]) {
    char* inputFile = "Test.txt";
    LC = strtol(argv[1], 0, 0);

    FILE* file_in = fopen(inputFile, "r");
    char line[MAX_SIZE];


    // dodaje se UND kao prva sekcija u tabeli simbola
    symbolTable[0].id = stp++;
    strcpy(&symbolTable[0].name, "UND");
    symbolTable[0].section = 0;
    symbolTable[0].size = 0;
    symbolTable[0].value = 0;
    symbolTable[0].visibility = LOCAL;

    // prvi prolaz
    theEnd = 0;
    while (!theEnd && fgets(line, MAX_SIZE, file_in)) {
	//printf("1st pass: %s\n", line);
        token = strtok(line, delim);
        if (token && isLabel(token)) {
            parseLabel1(token);
            token = strtok(0, delim);
        }
        if (token) {
            if (isDirective(token))
                parseDirective1(token);
            else
                parseInstruction1(token);
        }

        token = strtok(0, delim);
        if (token) {
            // GRESKA!
            printf("GRESKA! [%s]\n", token);
            exit(1);
        }
    }

    // drugi prolaz
    rewind(file_in);
    theEnd = 0;
    LC = strtol(argv[1], 0, 0);
    while (!theEnd && fgets(line, 1024, file_in)) {
	//printf("1st pass: %s\n", line);
        token = strtok(line, delim);
        if (token && isLabel(token)) {
            token = strtok(0, delim);
        }
        if (token) {
            if (isDirective(token))
                parseDirective2(token);
            else
                parseInstruction2(token);
        }
    }





    // ******** ISPIS TABELE, SEKCIJA I RELOKACIJA ******** 

    //FILE* output = stdout;
    FILE* output = fopen("TestIzlaz.txt", "w");



    // TABELA SIMBOLA

    // header tabele simbola
    fprintf(output, "%s\t", "#id");
    fprintf(output, "%s\t", "#name");
    fprintf(output, "%s\t", "#sec");
    fprintf(output, "%s\t", "#size");
    fprintf(output, "%s\t", "#value");
    fprintf(output, "%s\n", "#vis");

    for (i = 0; i < stp; i++) {
        fprintf(output, "%d\t", symbolTable[i].id);
        fprintf(output, "%s\t", symbolTable[i].name);
        fprintf(output, "%d\t", symbolTable[i].section);
        fprintf(output, "%d\t", symbolTable[i].size);
        fprintf(output, "%d\t", symbolTable[i].value);
        fprintf(output, "%c\n", symbolTable[i].visibility);
    }



    // TABELE RELOKACIJA 

    for (i = 0; i < stp; i++) {
        if (strcmp(symbolTable[i].name, ".text") == 0) {
            fprintf(output, "%s\n", "#.ret.text");
            // header tabele relokacija .text
            fprintf(output, "%s\t", "#add");
            fprintf(output, "%s\t", "#type");
            fprintf(output, "%s\n", "#sym");

            for (j = 0; j < rtpText; j++) {
                fprintf(output, "%x\t", relocationTableText[j].address);
                fprintf(output, "%c\t", relocationTableText[j].type);
                fprintf(output, "%d\n", relocationTableText[j].symbol);
            }

        }
    }


    for (i = 0; i < stp; i++) {
        if (strcmp(symbolTable[i].name, ".data") == 0) {
            fprintf(output, "%s\n", "#.ret.data");
            // header tabele relokacija .data
            fprintf(output, "%s\t", "#add");
            fprintf(output, "%s\t", "#type");
            fprintf(output, "%s\n", "#sym");

            for (j = 0; j < rtpData; j++) {
                fprintf(output, "%x\t", relocationTableData[j].address);
                fprintf(output, "%c\t", relocationTableData[j].type);
                fprintf(output, "%d\n", relocationTableData[j].symbol);
            }
        }
    }


    for (i = 0; i < stp; i++) {
        if (strcmp(symbolTable[i].name, ".rodata") == 0) {
            fprintf(output, "%s\n", "#.ret.rodata");
            // header tabele relokacija .rodata
            fprintf(output, "%s\t", "#add");
            fprintf(output, "%s\t", "#type");
            fprintf(output, "%s\n", "#sym");

            for (j = 0; j < rtpROData; j++) {
                fprintf(output, "%x\t", relocationTableROdata[j].address);
                fprintf(output, "%c\t", relocationTableROdata[j].type);
                fprintf(output, "%d\n", relocationTableROdata[j].symbol);
            }
        }
    }

    // .bss sekcija se ne ispisuje

    //for (i = 0; i < stp; i++) {
    //	if (strcmp(symbolTable[i].name, ".bss") == 0) {
    //		fprintf(output, "%s\n", "#.ret.bss");
    //		// header tabele relokacija .bss
    //		fprintf(output, "%s\t", "#add");
    //		fprintf(output, "%s\t", "#type");
    //		fprintf(output, "%s\n", "#sym");

    //		for (j = 0; j < rtpBss; j++) {
    //			fprintf(output, "%d\t", relocationTableBss[j].address);
    //			fprintf(output, "%c\t", relocationTableBss[j].type);
    //			fprintf(output, "%d\n", relocationTableBss[j].symbol);
    //		}

    //		fprintf(output, "\n");
    //	}
    //}


    // TABELE SEKCIJA

    for (i = 0; i < stp; i++) {
        if (strcmp(symbolTable[i].name, ".text") == 0) {
            fprintf(output, "%s\n", "#.text");
            for (j = 0; j < symbolTable[i].size; j++) {
                fprintf(output, "%02x ", sectionText[j]);
                if (j % 8 == 7) fprintf(output, "\n");
            }
            fprintf(output, "\n");
            break;
        }
    }

    for (i = 0; i < stp; i++) {
        if (strcmp(symbolTable[i].name, ".data") == 0) {
            fprintf(output, "%s\n", "#.data");
            for (j = 0; j < symbolTable[i].size; j++) {
                fprintf(output, "%02x ", sectionData[j]);
                if (j % 8 == 7) fprintf(output, "\n");
            }
            fprintf(output, "\n");
            break;
        }
    }

    for (i = 0; i < stp; i++) {
        if (strcmp(symbolTable[i].name, ".rodata") == 0) {
            fprintf(output, "%s\n", "#.rodata");
            for (j = 0; j < symbolTable[i].size; j++) {
                fprintf(output, "%02x ", sectionROData[j]);
                if (j % 8 == 7) fprintf(output, "\n");
            }
            fprintf(output, "\n");
            break;
        }
    }

    // .bss sekcija se ne ispisuje

    //for (i = 0; i < stp; i++) {
    //	if (strcmp(symbolTable[i].name, ".bss") == 0) {
    //		fprintf(output, "%s\n", "#.bss");
    //		for (j = 0; j < symbolTable[i].size; j++) {
    //			fprintf(output, "%02x ", sectionBss[j]);
    //			if (j % 8 == 7) fprintf(output, "\n");
    //		}
    //		fprintf(output, "\n");
    //		break;
    //	}
    //}

}





