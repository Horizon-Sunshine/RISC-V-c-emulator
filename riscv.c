#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_OF_REGS 32
#define STAGES 5
#define LINE_SIZE 50
#define MEMORY_SIZE 2084
#define PC_SIZE 100
#define STARTING_ADDRESS 1000

struct{
    char type;
    char opcode[5];
    int op1;
    int op2;
    int op3;
    int ALU_res;
}typedef  Instruction;

int registers[NUM_OF_REGS] = { 0 };
Instruction programMemory[MEMORY_SIZE] = { 0 };
int mainMemory[1000] = { 0 };
Instruction pipeline[STAGES] = { 0 };
int cpuCycle = 1;
int pc = STARTING_ADDRESS;
int numOfLoops = 0;

enum stage {
    FETCH, ID, EXE, MEM, WB
};

void startSimulator(int forwarding, int branchRes);
void shiftStatus(Instruction *newLine);
void printStatus();
int ALU(int opBypassed, int forwardValue);
int R_type(char* opcode, int reg1, int reg2);
int R_type_forward(char* opcode, int reg1, int reg2, int regBypassed, int forValue);
int I_type(char* opcode, int op2, int op3);
int I_type_forward(char* opcode, int op2, int op3, int regBypassed, int forValue);
int S_type(int offset, int reg);
int S_type_forward(int offset, int forValue);
int B_type(char* opcode, int reg1, int reg2);
int B_type_forward(char* opcode, int reg1, int reg2, int regBypaassed, int forValue);
void Memory();
void WriteBack();
void stallPipeline();
void resolveBranch(int hit, int branchRes);

void main(int argc, char* argv[])
{
    // will be args from main later
    int forwarding = atoi(argv[1]);
    int branchRes = atoi(argv[2]);

    char line[LINE_SIZE];
    int address;

    FILE *file = fopen("trace1.txt", "r");
    if (!file)
    {
        printf("Error opening file");
        exit(1);
    }

    while (fgets(line, sizeof(line), file))
    {
        Instruction fetch;
        sscanf(line, "%d %s %*c%d", &address, &fetch.opcode, &fetch.op1);
        if (!strcmp(fetch.opcode, "add") || !strcmp(fetch.opcode, "sub") || !strcmp(fetch.opcode, "or") || !strcmp(fetch.opcode, "and"))
        {
            fetch.type = 'R';
            sscanf(line, "%*d %*s %*c%*d %*c%d %*c%d", &fetch.op2, &fetch.op3);
        }
        else
            if (!strcmp(fetch.opcode, "addi") || !strcmp(fetch.opcode, "subi") || !strcmp(fetch.opcode, "ori") || !strcmp(fetch.opcode, "andi"))
            {
                fetch.type = 'I';
                sscanf(line, "%*d %*s %*c%*d %*c%d %d", &fetch.op2, &fetch.op3);
            }
            else
                if (!strcmp(fetch.opcode, "lw"))
                {
                    fetch.type = 'I';
                    sscanf(line, "%*d %*s %*c%*d %d %*c%d", &fetch.op2, &fetch.op3);
                }
                else
                    if (!strcmp(fetch.opcode, "sw"))
                    {
                        fetch.type = 'S';
                        sscanf(line, "%*d %*s %*c%*d %d %*c%d", &fetch.op2, &fetch.op3);
                    }
                    else
                        if (!strcmp(fetch.opcode, "beq") || !strcmp(fetch.opcode, "bneq"))
                        {
                            fetch.type = 'B';
                            sscanf(line, "%*d %*s %*c%*d %*c%d", &fetch.op2);
                            fetch.op3 = STARTING_ADDRESS;
                            numOfLoops++;
                        }
        programMemory[address] = fetch;
    }
    fclose(file);
    startSimulator(forwarding, branchRes);
}

void startSimulator(int forwarding, int branchRes)
{
    int instCount = 0;
    do
    {
        Instruction fetch = programMemory[pc];// Fetch next instruction according to the pc
        int opToBypass = 0; // orphand causing a data hazard (0 is none)
        int forwardedValue = 0;

        int BropToPass = 0; // if branchRes = 1

        shiftStatus(&fetch);// Advance pipeline

        // No forwarding support
        if (forwarding == 0)
        {
            // Check for data hazards
            if (pipeline[ID].type == 'R' || pipeline[ID].type == 'I') // hazard on ID with the new instruction
            {
                if (pipeline[FETCH].type == 'R' && (pipeline[FETCH].op2 == pipeline[ID].op1 || pipeline[FETCH].op3 == pipeline[ID].op1))
                    stallPipeline();
                else
                    if (pipeline[FETCH].type == 'I')
                    {
                        if (!strcmp(pipeline[FETCH].opcode, "lw"))
                            if (pipeline[ID].op1 == pipeline[FETCH].op3)
                                stallPipeline();
                        else
                            if (pipeline[ID].op1 == pipeline[FETCH].op2)
                                stallPipeline();
                    }
                    else
                        if (pipeline[FETCH].type == 'S' && (pipeline[ID].op1 == pipeline[FETCH].op1 || pipeline[ID].op1 == pipeline[FETCH].op3))
                            stallPipeline();
                        else
                            if (pipeline[FETCH].type == 'B' && (pipeline[ID].op1 == pipeline[FETCH].op1 || pipeline[ID].op1 == pipeline[FETCH].op2))
                                stallPipeline();
            }

            if (pipeline[EXE].type == 'R' || pipeline[EXE].type == 'I') // hazard on EXE with the new instruction
            {
                if (pipeline[FETCH].type == 'R' && (pipeline[FETCH].op2 == pipeline[EXE].op1 || pipeline[FETCH].op3 == pipeline[EXE].op1))
                    stallPipeline();
                else
                    if (pipeline[FETCH].type == 'I')
                    {
                        if (!strcmp(pipeline[FETCH].opcode, "lw"))
                            if (pipeline[EXE].op1 == pipeline[FETCH].op3)
                                stallPipeline();
                        else
                            if (pipeline[EXE].op1 == pipeline[FETCH].op2)
                                stallPipeline();
                    }
                    else
                        if (pipeline[FETCH].type == 'S' && (pipeline[EXE].op1 == pipeline[FETCH].op1 || pipeline[EXE].op1 == pipeline[FETCH].op3))
                            stallPipeline();
                        else
                            if (pipeline[FETCH].type == 'B' && (pipeline[EXE].op1 == pipeline[FETCH].op1 || pipeline[EXE].op1 == pipeline[FETCH].op2))
                                stallPipeline();
            }
        }
        else // Forwarding IS supported
        {
            if (!strcmp(pipeline[ID].opcode, "lw")) // with forwarding lw is the only potential data hazard
            {
                switch (pipeline[FETCH].type)
                {
                case 'R':
                    if (pipeline[FETCH].op2 == pipeline[ID].op1 || pipeline[FETCH].op3 == pipeline[ID].op1)
                        stallPipeline();
                    break;
                case 'I':
                    if (!strcmp(pipeline[FETCH].opcode, "lw"))
                    {
                        if (pipeline[FETCH].op3 == pipeline[ID].op1)
                            stallPipeline();
                    }
                    else
                        if (pipeline[FETCH].op2 == pipeline[ID].op1)
                            stallPipeline();
                    break;
                case 'S':
                    if (pipeline[ID].op1 == pipeline[FETCH].op1 || pipeline[ID].op1 == pipeline[FETCH].op3)
                        stallPipeline();
                    break;
                case 'B':
                    if (pipeline[ID].op1 == pipeline[FETCH].op1 || pipeline[ID].op1 == pipeline[FETCH].op2)
                        stallPipeline();
                    break;
                }
            }

            // check if forwarding is required
            if (pipeline[WB].type == 'R' || pipeline[WB].type == 'I')
            {
                forwardedValue = pipeline[WB].ALU_res;
                switch (pipeline[EXE].type)
                {
                case 'R':
                    if (pipeline[EXE].op2 == pipeline[WB].op1)
                        opToBypass = 2;
                    else
                        if (pipeline[EXE].op3 == pipeline[WB].op1)
                            opToBypass = 3;
                    break;
                case 'I':
                    if (!strcmp(pipeline[EXE].opcode, "lw"))
                    {
                        if (pipeline[EXE].op3 == pipeline[WB].op1)
                            opToBypass = 3;
                    }
                    else
                        if (pipeline[EXE].op2 == pipeline[WB].op1)
                            opToBypass = 2;
                case 'S':
                    if (pipeline[EXE].op1 == pipeline[WB].op1)
                        opToBypass = 1;
                    else
                        if (pipeline[EXE].op3 == pipeline[WB].op1)
                            opToBypass = 3;
                    break;
                case 'B':
                    if (pipeline[EXE].op1 == pipeline[WB].op1)
                        opToBypass = 1;
                    else
                        if (pipeline[EXE].op2 == pipeline[WB].op1)
                            opToBypass = 2;
                    break;
                default:
                    break;
                }
                if (branchRes == 1 && pipeline[ID].type == 'B')
                {
                    if (pipeline[ID].op1 == pipeline[WB].op1)
                        BropToPass = 1;
                    else
                        if (pipeline[ID].op2 == pipeline[WB].op1)
                            BropToPass = 2;
                }
            }

            if (pipeline[MEM].type == 'R' || pipeline[MEM].type == 'I')
            {
                forwardedValue = pipeline[MEM].ALU_res;
                switch (pipeline[EXE].type)
                {
                case 'R':
                    if (pipeline[EXE].op2 == pipeline[MEM].op1)
                        opToBypass = 2;
                    else
                        if (pipeline[EXE].op3 == pipeline[MEM].op1)
                            opToBypass = 3;
                    break;
                case 'I':
                    if (!strcmp(pipeline[EXE].opcode, "lw"))
                    {
                        if (pipeline[EXE].op3 == pipeline[MEM].op1)
                            opToBypass = 3;
                    }
                    else
                        if (pipeline[EXE].op2 == pipeline[MEM].op1)
                            opToBypass = 2;
                case 'S':
                    if (pipeline[EXE].op1 == pipeline[MEM].op1)
                        opToBypass = 1;
                    else
                        if (pipeline[EXE].op3 == pipeline[MEM].op1)
                            opToBypass = 3;
                    break;
                case 'B':
                    if (pipeline[EXE].op1 == pipeline[MEM].op1)
                        opToBypass = 1;
                    else
                        if (pipeline[EXE].op2 == pipeline[MEM].op1)
                            opToBypass = 2;
                    break;
                default:
                    break;
                }
                if (branchRes == 1 && pipeline[ID].type == 'B')
                {
                    if (pipeline[ID].op1 == pipeline[WB].op1)
                        BropToPass = 1;
                    else
                        if (pipeline[ID].op2 == pipeline[WB].op1)
                            BropToPass = 2;
                }
            }
        }
        pipeline[EXE].ALU_res = ALU(opToBypass, forwardedValue);
        WriteBack();
        // Resolve branch if needed
        if (pipeline[ID].type == 'B' && branchRes == 1) // Branch is resolved in ID
        {
            if (forwarding == 1)
            {
                pipeline[ID].ALU_res = B_type_forward(&pipeline[ID].opcode, pipeline[ID].op1, pipeline[ID].op2, BropToPass, forwardedValue);
                resolveBranch(pipeline[MEM].ALU_res, branchRes);
            }
            else
            {
                if (!strcmp(pipeline[ID].opcode, "beq"))
                    resolveBranch(registers[pipeline[ID].op1] == registers[pipeline[ID].op2] ? 1 : 0, branchRes);
                else
                    resolveBranch(registers[pipeline[ID].op1] != registers[pipeline[ID].op2] ? 1 : 0, branchRes);
            }
        }
        else // Branch is resolved in MEM
            if (pipeline[MEM].type == 'B' && branchRes == 0)
                resolveBranch(pipeline[MEM].ALU_res, branchRes);
        Memory();
        // Assuming suppurted hardware for write and read to regfile in 1 cycle to avoid structural hazards
        printStatus();
        pc += 4;
        cpuCycle++;
        if (pipeline[WB].type != 0)
        {
            if (pipeline[WB].type == 'B')
                numOfLoops--;
            instCount++;
        }
        
        printf("End of cpu cycle\n");
    } while ((pipeline[FETCH].opcode[0] || pipeline[WB].opcode[0]) && numOfLoops > 0); // While pipeline is not empty
    printf("End of program\n");

    FILE* file = fopen("output.txt", "a");
    if (!file)
    {
        printf("Error with output file");
        exit(1);
    }
    fprintf(file, "CPI: %d", (cpuCycle-1)/instCount);
    fclose(file);
}

void shiftStatus(Instruction *newLine)
{
    for (int i = STAGES - 1; i > 0; --i)
        pipeline[i] = pipeline[i - 1];
    pipeline[FETCH] = *newLine;
}

void printStatus()
{
    // If pipeline is empty don't print anything
    if (!(pipeline[FETCH].opcode[0] || pipeline[WB].opcode[0]))
        return;

    FILE* file = fopen("output.txt", "a");
    if (!file)
    {
        printf("Error with output file");
        exit(1);
    }

    fprintf(file, "Cycle %d\n", cpuCycle);
    for (int i = 0; i < STAGES; ++i)
    {
        // Print stage of pipeline
        switch (i)
        {
        case 0:
            fprintf(file, "Fetch instruction: ");
            break;
        case 1:
            fprintf(file, "Decode instruction: ");
            break;
        case 2:
            fprintf(file, "Execute instruction: ");
            break;
        case 3:
            fprintf(file, "Memory instruction: ");
            break;
        case 4:
            fprintf(file, "Writeback instruction: ");
            break;
        default:
            break;
        }

        // If opcode is nop write stall
        if (!strcmp(pipeline[i].opcode, "nop"))
            fprintf(file, "stall");
        else
            if (!strcmp(pipeline[i].opcode, "flsh"))
                fprintf(file, "flush");

        // Print the instruction
        switch (pipeline[i].type)
        {
        case 'R':
            fprintf(file, "%s x%d x%d x%d\n", pipeline[i].opcode, pipeline[i].op1, pipeline[i].op2, pipeline[i].op3);
            break;
        case 'I':
            if (!strcmp(pipeline[i].opcode, "lw"))
                fprintf(file, "%s x%d %d x%d\n", pipeline[i].opcode, pipeline[i].op1, pipeline[i].op2, pipeline[i].op3);
            else
                fprintf(file, "%s x%d x%d %d\n", pipeline[i].opcode, pipeline[i].op1, pipeline[i].op2, pipeline[i].op3);
            break;
        case 'S':
            fprintf(file, "%s x%d %d x%d\n", pipeline[i].opcode, pipeline[i].op1, pipeline[i].op2, pipeline[i].op3);
            break;
        case 'B':
            fprintf(file, "%s x%d x%d loop\n", pipeline[i].opcode, pipeline[i].op1, pipeline[i].op2);
            break;
        default:
            fprintf(file, "\n");
            break;
        }
    }
    fprintf(file, "\n");
    fclose(file);
}

int ALU(int opBypassed, int forwardValue) // orphand to be bypassed is 2 or 3 if needed, else no forwarding
{
    if (pipeline[EXE].type == 0)
        return;
    // If forwarding is required
    if (opBypassed == 1 || opBypassed == 2 || opBypassed == 3)
    {
        switch (pipeline[EXE].type)
        {
        case 'R':
            return R_type_forward(&pipeline[EXE].opcode, pipeline[EXE].op2, pipeline[EXE].op3, opBypassed, forwardValue);
            break;
        case 'I':
            return I_type_forward(&pipeline[EXE].opcode, pipeline[EXE].op2, pipeline[EXE].op3, opBypassed, forwardValue);
            break;
        case 'S':
            return S_type_forward(pipeline[EXE].op2, forwardValue);
            break;
        case 'B':
            return B_type_forward(&pipeline[EXE].opcode, pipeline[EXE].op1, pipeline[EXE].op2, opBypassed, forwardValue);
            break;
        default:
            break;
        }
    }
    else
    {// Normal operation without forwarding
        switch (pipeline[EXE].type)
        {
        case 'R':
            return R_type(&pipeline[EXE].opcode, pipeline[EXE].op2, pipeline[EXE].op3);
            break;
        case 'I':
            return I_type(&pipeline[EXE].opcode, pipeline[EXE].op2, pipeline[EXE].op3);
            break;
        case 'S':
            return S_type(pipeline[EXE].op2, pipeline[EXE].op3);
            break;
        case 'B':
            return B_type(&pipeline[EXE].opcode, pipeline[EXE].op1, pipeline[EXE].op2);
            break;
        default:
            break;
        }
    }
}

int R_type(char* opcode, int reg1, int reg2) // Normal R type operation
{
    if (!strcmp(opcode, "add"))
        return registers[reg1] + registers[reg2];
    if (!strcmp(opcode, "sub"))
        return registers[reg1] - registers[reg2];
    if (!strcmp(opcode, "or"))
        return registers[reg1] | registers[reg2];
    if (!strcmp(opcode, "and"))
        return registers[reg1] & registers[reg2];
    return NULL;
}

int R_type_forward(char* opcode, int reg1, int reg2, int regBypassed, int forValue)
{// R type where 1 of the registers was bypassed
    if (regBypassed == 2)
    {
        if (!strcmp(opcode, "add"))
            return forValue + registers[reg2];
        if (!strcmp(opcode, "sub"))
            return forValue - registers[reg2];
        if (!strcmp(opcode, "or"))
            return forValue | registers[reg2];
        if (!strcmp(opcode, "and"))
            return forValue & registers[reg2];
    }
    if (regBypassed == 3)
    {
        if (!strcmp(opcode, "add"))
            return registers[reg1] + forValue;
        if (!strcmp(opcode, "sub"))
            return registers[reg1] - forValue;
        if (!strcmp(opcode, "or"))
            return registers[reg1] | forValue;
        if (!strcmp(opcode, "and"))
            return registers[reg1] & forValue;
    }
    return NULL;
}

int I_type(char* opcode, int op2, int op3) // Normal I type operation
{
    if (!strcmp(opcode, "lw"))
        return op2 + registers[op3];
    if (!strcmp(opcode, "addi"))
        return registers[op2] + op3;
    if (!strcmp(opcode, "subi"))
        return registers[op2] - op3;
    if (!strcmp(opcode, "ori"))
        return registers[op2] | op3;
    if (!strcmp(opcode, "andi"))
        return registers[op2] & op3;
    return NULL;
}

int I_type_forward(char* opcode, int op2, int op3, int regBypassed, int forValue)
{ // I type with forwarding
    if (regBypassed == 2)
    {
        if (!strcmp(opcode, "addi"))
            return forValue + op3;
        if (!strcmp(opcode, "subi"))
            return forValue - op3;
        if (!strcmp(opcode, "ori"))
            return forValue | op3;
        if (!strcmp(opcode, "andi"))
            return forValue & op3;
    }
    if (regBypassed == 3)
    {
        if (!strcmp(opcode, "lw"))
            return op2 + forValue;
    }
    return NULL;
}

int S_type(int offset, int reg)
{
    return offset + registers[reg];
}

int S_type_forward(int offset, int forValue)
{
    return offset + forValue;
}

int B_type(char* opcode, int reg1, int reg2)
{
    if (!strcmp(opcode, "beq"))
        return registers[reg1] == registers[reg2] ? 1 : 0;
    if (!strcmp(opcode, "bneq"))
        return registers[reg1] != registers[reg2] ? 1 : 0;
    return NULL;
}

int B_type_forward(char* opcode, int reg1, int reg2, int regBypaassed, int forValue)
{
    if (regBypaassed == 1)
    {
        if (!strcmp(opcode, "beq"))
            return forValue == registers[reg2] ? 1 : 0;
        if (!strcmp(opcode, "bneq"))
            return forValue != registers[reg2] ? 1 : 0;
    }
    if (regBypaassed == 2)
    {
        if (!strcmp(opcode, "beq"))
            return registers[reg1] == forValue ? 1 : 0;
        if (!strcmp(opcode, "bneq"))
            return registers[reg1] != forValue ? 1 : 0;
        return NULL;
    }
    return NULL;
}

void Memory()
{
    if (!strcmp(pipeline[MEM].opcode, "sw"))
        mainMemory[pipeline[MEM].ALU_res] = registers[pipeline[MEM].op1];
    else
        if (!strcmp(pipeline[MEM].opcode, "lw"))
            pipeline[MEM].ALU_res = mainMemory[pipeline[MEM].ALU_res];
}

void WriteBack()
{
    if (pipeline[WB].type == 'R' || pipeline[WB].type == 'I')
        registers[pipeline[WB].op1] = pipeline[WB].ALU_res;
}

void stallPipeline()
{
    pc -= 4;
    pipeline[FETCH].type = 0;
    strcpy(pipeline[FETCH].opcode, "nop");
}

void resolveBranch(int hit, int branchRes)
{
    if (hit == 0)
        return;
    if (branchRes == 0)
    {
        for (int i = FETCH; i < MEM; ++i)
        {
            pipeline[i].type = 0;
            strcpy(pipeline[i].opcode, "flsh");
        }
        pc = pipeline[MEM].op3 - 4;
    }
    else
    {
        pipeline[FETCH].type = 0;
        strcpy(pipeline[FETCH].opcode, "flsh");
        pc = pipeline[ID].op3 - 4;
    }
}