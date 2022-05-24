#include <stdio.h>
#include <stdlib.h>

#define MEMSIZE (65536 >> 2)    //Lot of memory that can hold a lot of instructions
#define REGSIZE 32              //32 data registers
#define PC (Reg[REGSIZE +0])    //Sets Program Counter (PC) to 33rd register, which tells us which instruction is being performed

//Memory that stores the instructions
static unsigned Mem[MEMSIZE];       //Array containing all the instructions

//Registers that store the data
static unsigned Reg[REGSIZE + 4];   //Array containing the data of all registers

unsigned long int extended_value;

//32 bit long instruction
unsigned long int instruction = 0xffffffff; //32 bits in hex

//instruction parts
unsigned char op,       // [31-26]
              reg1,     // [25-21]
              reg2,     // [20-16]
              reg3,     // [15-11]
              shamt,    // [10-6]
              funct,    // [5-0]
              offset,   // [15-0]
              jsec;     // [25-0]

//controls --> sets which datapaths will be used
char RegDst,    //HIGH when 3 registers are used
     Jump,      //HIGH when when jumping to a new instruction
     Branch,    //HIGH when branching is used
     MemRead,   //value depends on how the memory is read
     MemtoReg,  //LOW when data resulting from the ALU must be stored in a register -> write-back
     ALUOp,     //value depends on what task the ALU should perform
     MemWrite,  //HIGH when data must be written to memory
     RegWrite;  //HIGH when data must be written to a register

//ALU outputs
int ALUresult, Zero;

//data
unsigned int data1, data2;
unsigned memData;

int memAccessed = 0;    //logs register access
int regAccessed = 0;    //logs memory access

//Fetches instruction using the PC, which depends on the current index in memory
void instructionFetch() {
    //printf("---INSTRUCTION FETCH---\n");
    unsigned Index = PC >> 2;   //When PC = 4, Index = 1 --> (PC = 100 --> shift 2 bits to the right --> Index = 1)
    instruction = Mem[Index];
    //printf("instruction: %u\n", instruction);
}

//Divides the instruction in a way that depends on its first 6 bits: the opcode
void instructionDivide() {
    //printf("---INSTRUCTION DIVIDE---\n");
    unsigned bits5		    = 0x1f;         //5 bits total
    unsigned bits6	        = 0x0000003f;   //6 bits total
    unsigned bits16	        = 0x0000ffff;   //16 bits total
    unsigned bits26	        = 0x03ffffff;   //26 bits total

    reg3 = 0;
    shamt = 0;
    funct = 0;
    offset = 0;

        op		    = (instruction >> 26)   & bits6;	   // instruction [31-26] #need 6 bits

    //Puts the instruction in an R-Type format (divides it into 6 parts total)
    if (op == 1 || op == 2 || op == 3 || op == 4 || op == 5 || op == 9 || op == 10 || op == 11) {
        //printf("R-type instruction\n");
        reg1	    = (instruction >> 21)   & bits5;       // instruction [25-21] #need 5 bits
        reg2		= (instruction >> 16)   & bits5;       // instruction [20-16] #need 5 bits
        reg3    	= (instruction >> 11)   & bits5;       // instruction [15-11] #need 5 bits
        shamt    	= (instruction >> 6)    & bits5;       // instruction [10-6]  #need 5 bits
        funct	    = instruction           & bits6;       // instruction [5-0]   #need 6 bits
    }

    //Puts the instruction in an I-Type format (divides it into 4 parts total)
    else if (op == 7 || op == 8) {
        //printf("I-type instruction\n");
        reg1	    = (instruction >> 21)   & bits5;       // instruction [25-21] #need 5 bits
        reg2		= (instruction >> 16)   & bits5;       // instruction [20-16] #need 5 bits
        offset    	= instruction           & bits16;      // instruction [15-0]  #need 16 bits
    }

    //Puts the instruction in a J-Type format (divides it into 2 parts total)
    else if (op == 6) {
        //printf("J-type instruction\n");
        jsec	    = instruction           & bits26;    // instruction [25-0]  #need 26 bits
    }
    /*
    printf("op: %u\n", op);
    printf("reg1: %u\n", reg1);
    printf("reg2: %u\n", reg2);
    printf("reg3: %u\n", reg3);
    printf("shamt: %u\n", shamt);
    printf("funct: %u\n", funct);
    printf("offset: %u\n", offset);
    printf("jsec: %u\n",jsec);
    */
}

//Decodes the instruction based on its opcode -> this tells the CPU what kind of task to perform
int instructionDecode(){
    //printf("---INSTRUCTION DECODE---\n");
    switch(op){

        //STORE --> stores 10 bit data value (reg1 + reg2) in Reg[reg3]
        case 1: // 000 001
            //printf("STORING\n");
            RegDst = 1;
            Jump = 0;
            Branch = 0;
            MemRead = 2;
            MemtoReg = 0;
            ALUOp = 1;
            MemWrite = 0;
            RegWrite = 1;
            break;

        //ADD --> adds value of Reg[reg1] with value of Reg[reg2] and stores result in register address = Reg[reg3]
        case 2: //000 010
            //printf("ADDING\n");
            RegDst = 1;
            Jump = 0;
            Branch = 0;
            MemRead = 1;
            MemtoReg = 0;
            ALUOp = 2;
            MemWrite = 0;
            RegWrite = 1;
            break;

        //SUB --> subtracts value of Reg[reg1] by value of Reg[reg2] and stores result in register address = Reg[reg3]
        case 3: //000 011
            //printf("SUBTRACTING\n");
            RegDst = 1;
            Jump = 0;
            Branch = 0;
            MemRead = 1;
            MemtoReg = 0;
            ALUOp = 3;
            MemWrite = 0;
            RegWrite = 1;
            break;

        //SLT --> If value in Reg[reg1] < value in Reg[reg2], value in Reg[reg3] = 1, else value in Reg[reg3] = 0
        case 4: //000 100
            //printf("SETTING LESS THAN\n");
            RegDst = 1;
            Jump = 0;
            Branch = 0;
            MemRead = 1;
            MemtoReg = 0;
            ALUOp = 4;
            MemWrite = 0;
            RegWrite = 1;
            break;

        //MOD --> Stores remainder of (value in Reg[reg1] / value in Reg[reg2]) in Reg[reg3]
        case 5: //000 101
            //printf("MODULO OPERATION\n");
            RegDst = 1;
            Jump = 0;
            Branch = 0;
            MemRead = 1;
            MemtoReg = 0;
            ALUOp = 5;
            MemWrite = 0;
            RegWrite = 1;
            break;

        //JUMP --> Jumps to a new instruction
        case 6: //000 110
            RegDst = 0;
            Jump = 1;
            Branch = 0;
            MemRead = 0;
            MemtoReg = 0;
            ALUOp = 0;
            MemWrite = 0;
            RegWrite = 0;
            break;

        //BEQ --> Branch equal --> if value in Reg[reg1] = value in Reg[reg2], jumps to instruction where PC = offset
        case 7: //000 111
            //printf("BRANCH EQUAL\n");
            RegDst = 2;
            Jump = 0;
            Branch = 1;
            MemRead = 1;
            MemtoReg = 2;
            ALUOp = 6;
            MemWrite = 0;
            RegWrite = 0;
            break;

        //BNE --> Branch not equal --> if value in Reg[reg1] != value in Reg[reg2], jumps to instruction where PC = offset
        case 8: //001 000
           // printf("BRANCH NOT EQUAL\n");
            RegDst = 2;
            Jump = 0;
            Branch = 1;
            MemRead = 1;
            MemtoReg = 2;
            ALUOp = 7;
            MemWrite = 0;
            RegWrite = 0;
            break;

        //REGOUT --> Output the value in Reg[reg1] as OUTPUT
        //       --> Prints data stored in all registers and in memory
        case 9: //001 001
            //printf("OUTPUTTING REGISTER VALUE\n");
            RegDst = 0;
            Jump = 0;
            Branch = 0;
            MemRead = 0;
            MemtoReg = 0;
            ALUOp = 8;
            MemWrite = 0;
            RegWrite = 0;
            break;

        //SAVE --> Saves data in Reg[reg1] to memory
        case 10: //001 010
            RegDst = 2;
            Jump = 0;
            Branch = 0;
            MemRead = 0;
            MemtoReg = 2;
            ALUOp = 0;
            MemWrite = 1;
            RegWrite = 0;
            break;

        //LOAD --> Loads data from memory to Reg[reg3]
        case 11: //001 011
            RegDst = 1;
            Jump = 0;
            Branch = 0;
            MemRead = 0;
            MemtoReg = 1;
            ALUOp = 0;
            MemWrite = 0;
            RegWrite = 1;
            break;

        default://Return 1 if Halt
            return 1;
    }
    return 0;
}

//Offsets instruction by 16 bits
void signExtend() {
    unsigned extend1s = 0xFFFF0000;
    unsigned int negative = offset >> 15;

    if (negative == 1)
        extended_value = offset | extend1s;
    else
        extended_value = offset & 0x0000ffff;
    return;
}

//Reads data
void read() {
    //printf("---READ---\n");
    //Reads data1 in register at address reg1, reads data2 in register at address reg2
    if (MemRead == 1) {
        data1 = Reg[reg1];
        data2 = Reg[reg2];
        regAccessed++;      //logging register access
    //Sets data1 as value in reg1 and sets data2 as value in reg2
    } else if (MemRead == 2) {
        data1 = reg1;
        data2 = reg2;
    }
    /*
    for (int i; i < REGSIZE + 4; i++) {
        printf("Register%d: %u\n", i, Reg[i]);
    }*/
}

//ALU operations
void ALU() {
    //printf("---ALU---\n");
    switch (ALUOp) {
        //STORE --> Combines 5 bits in reg1 with 5 bits in reg2 to create a set of 10 bits (allows for more values to be stored)
        case 1:
            ALUresult = (data1 << 5) + data2;
            break;
        //ADD --> Adds data together
        case 2:
            ALUresult = data1 + data2;
            break;
        //SUB --> Subtracts data
        case 3:
            ALUresult = data1 - data2;
            break;
        //SLT --> Compares to see if data1 is smaller than data2, stores HIGH or LOW in result depending on the inputs
        case 4:
            if (data1 < data2) {
                ALUresult = 1;
            } else {
                ALUresult = 0;
            }
            break;
        //MOD --> Does the modulo operation between data1 anf data2
        case 5:
            ALUresult = data1 % data2;
            break;
        //BEQ --> Checks if data1 and data2 are equal
        case 6:
            if (data1 == data2) {
                Zero = 1;
            } else {
                Zero = 0;
            }
            break;
        //BNE --> Checks if data1 and data2 aren't equal
        case 7:
            if (data1 != data2) {
                Zero = 1;
            } else {
                Zero = 0;
            }
            break;
        //REGOUT --> Prints OUTPUT, all registers, memory and memory accesses
        case 8:
            printf("Memory: %u\n", memData);
            for (int i = 0; i < REGSIZE + 4; i++) {
                printf("Register %d: %u\n", i, Reg[i]);
            }
            printf("====================> OUTPUT: %u\n", Reg[reg1]);
            printf("Registers accessed %d times\n", regAccessed);
            printf("Memory accessed %d times\n", memAccessed);
            printf("---------------------------------------------\n");
            break;
    }
    //printf("ALUresult: %d\n", ALUresult);
    //printf("Zero: %d\n", Zero);
}

//Accesses memory
void memoryAccess() {
    //printf("---MEMORY ACCESS---\n");
    //writes data at register address reg1 into memory
    if (MemWrite == 1) {
        memData = Reg[reg1];
        memAccessed++;
        regAccessed++;
    }
    //printf("Memory: %u\n", memData);
}

//Writes data to registers
void writeBack() {
    //printf("---WRITE BACK---\n");
    if(RegWrite == 1){
        //Data in memory gets stored in the register at address reg3
        if (MemtoReg == 1 && RegDst == 1){
            Reg[reg3] = memData;
            memAccessed++;
            regAccessed++;
        }
        //Data output of the ALU is stored at the register address reg3
        else if (MemtoReg == 0 && RegDst == 1){
            Reg[reg3] = ALUresult;
            regAccessed++;
        }
    }
    /*
    for (int i; i < REGSIZE + 4; i++) {
        printf("Register%d: %u\n", i, Reg[i]);
    }*/
}

//Handles the Program Counter
void PCUpdate() {
    //printf("---PC UPDATE----\n");
    //PC increments by 4 at the end of an instruction
    PC += 4;
    //PC is set to be equal to the value in offset if certain branch conditions are met
    if(Branch == 1 && Zero == 1) {
        PC = offset;
    }

    if(Jump == 1) {
        PC = (jsec << 2) | (PC & 0xf0000000);
    }
    //printf("PC: %u\n", PC);
}

int main() {

    int choice;
    printf("Type a number to pick which instruction you would like the CPU to perform:\n"
                   "1. Calculate the squares of all integers between 0 and 99\n"
                   "2. Calculate all the prime numbers between 1 and 1000\n"
                   "3. Calculate all numbers that are multiples of 3 between 0 and 300 in a descending order\n"
                   "4. Memory access test: Storing 10 from a register 1 into memory, then loading it into register 3 from memory\n");
    scanf("%d", &choice);

        //All programs (sets of instructions)
        switch(choice) {

            case 1:
                //SQUARES
                printf("Calculating the squares of all integers between 0 and 99\n");
                Mem[0]  = 0b00000100000000010010000000000000;
                Mem[1]  = 0b00000100000000100010100000000000;
                Mem[2]  = 0b00000100000000010001000000000000;
                Mem[3]  = 0b00000100011001000011000000000000;
                Mem[4]  = 0b00000100000000010000100000000000;
                Mem[5]  = 0b00001000010000110001100000000000;
                Mem[6]  = 0b00001000001001000000100000000000;
                Mem[7]  = 0b00001000010001010001000000000000;
                Mem[8]  = 0b00100100011000000000000000000000;
                Mem[9]  = 0b00100000001001100000000000010100;
                Mem[10] = 0b00000000000000000000000000000000;
                break;

            case 2:
                //PRIMES
                printf("Calculating all the prime numbers between 1 and 1000\n");
                Mem[0]  = 0b00000100000000010000100000000000;
                Mem[1]  = 0b00000100000000000001000000000000;
                Mem[2]  = 0b00000100000000000001100000000000;
                Mem[3]  = 0b00000100000000000010000000000000;
                Mem[4]  = 0b00000111111010000010100000000000;
                Mem[5]  = 0b00000100000000100011000000000000;
                Mem[6]  = 0b00001000100000010010000000000000;
                Mem[7]  = 0b00011100100001010000000001010100;
                Mem[8]  = 0b00001000110000010011000000000000;
                Mem[9]  = 0b00010000110001000011100000000000;
                Mem[10] = 0b00011100111010000000000000111100;
                Mem[11] = 0b00010100100001100100100000000000;
                Mem[12] = 0b00100001001010000000000000100000;
                Mem[13] = 0b00000100000000010001000000000000;
                Mem[14] = 0b00011100001000010000000000100000;
                Mem[15] = 0b00100000010010000000000001001100;
                Mem[16] = 0b00011100011001000000000001001000;
                Mem[17] = 0b00100100100000000000000000000000;
                Mem[18] = 0b00001000100010000001100000000000;
                Mem[19] = 0b00000100000000000001000000000000;
                Mem[20] = 0b00011100001000010000000000010000;
                Mem[21] = 0b00000000000000000000000000000000;
                break;

            case 3:
                //DESCENDING MULTIPLE OF 3
                printf("Calculating all numbers that are multiples of 3 between 0 and 300 in a descending order\n");
                Mem[0] = 0b00000101001011110000100000000000;
                Mem[1] = 0b00000100000000110001000000000000;
                Mem[2] = 0b00001100001000100000100000000000;
                Mem[3] = 0b00100100001000000000000000000000;
                Mem[4] = 0b00100000001001000000000000001000;
                Mem[5] = 0b00000000000000000000000000000000;
                break;

            case 4:
                //MEMORY TESTING
                printf("Storing 10 from a register 1 into memory, then loading it back into register 3 from memory\n");
                Mem[0] = 0b00000100000010100000100000000000;
                Mem[1] = 0b00100100011000000000000000000000;
                Mem[2] = 0b00101000001000000000000000000000;
                Mem[3] = 0b00100100011000000000000000000000;
                Mem[4] = 0b00101100000000000001100000000000;
                Mem[5] = 0b00100100011000000000000000000000;
                Mem[6] = 0b00000000000000000000000000000000;
            /*
            case 5:
                //TESTING
                Mem[0]  = 0b00000100000000110000100000000000;    // STORE 3 in R1
                Mem[1]  = 0b00000100000010010001000000000000;    // STORE 9 in R2
                Mem[2]  = 0b00001000001000100001100000000000;    // ADD R1 + R2 = R3
                Mem[3]  = 0b00100100011000000000000000000000;    // REGOUT R3
                Mem[4]  = 0b00000100000010100010000000000000;    // STORE 10 in R4
                Mem[5]  = 0b00001100011001000010100000000000;    // SUB R3 - R4 = R5
                Mem[6]  = 0b00100100101000000000000000000000;    // REGOUT R5
                Mem[7]  = 0b00010000101001000011000000000000;    // SLT R5 < R4 = R6
                Mem[8]  = 0b00100100110000000000000000000000;    // REGOUT R6
                Mem[9]  = 0b00010000100001010011100000000000;    // SLT E4 < R5 = R7
                Mem[10] = 0b00100100111000000000000000000000;    // REGOUT R7
                Mem[11] = 0b00010100100000010100000000000000;    // MOD R4 % R1 = R8
                Mem[12] = 0b00100101000000000000000000000000;    // REGOUT R8
                Mem[13] = 0b00101000100000000000000000000000;    // SAVE R4 to MEMORY
                Mem[14] = 0b00101100000000000100100000000000;    // LOAD MEMORY to R9
                Mem[15] = 0b00100101001000000000000000000000;    // REGOUT R9
                Mem[16] = 0b00011100110001110000000001011000;    // BEQ R6 == R7 -> PC = 88
                Mem[17] = 0b00100100000000000000000000000000;    // REGOUT R0
                Mem[18] = 0b00011100110010000000000001011000;    // BEQ R6 == R8 -> PC = 88
                Mem[22] = 0b00100100000000000000000000000000;    // REGOUT R0
                Mem[23] = 0b00100000110010000000000001110100;    // BEQ R6 != R8 -> PC = 116
                Mem[24] = 0b00100100000000000000000000000000;    // REGOUT R0
                Mem[25] = 0b00100000110001110000000001110100;    // BEQ R6 != R7 -> PC = 116
                Mem[29] = 0b00100100000000000000000000000000;    // REGOUT R0
                Mem[30] = 0b00000000000000000000000000000000;    // END
                break;
                */
        }

    //Loop that runs each instruction through all cycles
    for (int i = 0; i < MEMSIZE; i++) {
        instruction = Mem[i];
        while (PC <= (i << 2)) {
            while (instruction != 0) {
                //printf("--------------------> Instruction START\n");
                instructionFetch();
                instructionDivide();
                instructionDecode();
                read();
                ALU();
                memoryAccess();
                writeBack();
                PCUpdate();
                //printf("--------------------> Instruction END\n");
            }
            return 1;
        }
    }
}

