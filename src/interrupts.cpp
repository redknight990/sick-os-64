#include "interrupts.h"
#include "lib.h"

IDT::GateDescriptor::GateDescriptor(){

    uint64_t * ptr = reinterpret_cast<uint64_t*>(this);

    // Zero the fields
    ptr[0] = 0;
    ptr[1] = 0;
}

IDT::GateDescriptor::GateDescriptor(uint64_t base, uint16_t selector, uint8_t typeAttr){
    this->lBase = base & 0xFFFF;
    this->selector = selector;
    this->typeAttr = typeAttr;
    this->mBase = (base >> 16) & 0xFFFF;
    this->hBase = (base >> 32) & 0xFFFFFFFF;
}

IDT::GateDescriptor::~GateDescriptor(){}

uint64_t IDT::GateDescriptor::Base(){
    uint64_t result = hBase << 32;
    result |= mBase << 16;
    result |= lBase;
    return result;
}

uint16_t IDT::GateDescriptor::Selector(){
    return selector;
}

uint8_t IDT::GateDescriptor::TypeAttributes(){
    return typeAttr;
}

IDT::IDT(){}
IDT::~IDT(){};

void IDT::SetEntry(uint8_t index, void (* handler)(), uint16_t selector, uint8_t typeAttr){
    table[index] = GateDescriptor(reinterpret_cast<uint64_t>(handler), selector, IDT_PRESENT | typeAttr);
}

IDT::GateDescriptor * IDT::GetEntry(uint8_t index){
    return table + index;
}

void IDT::LoadIDT(){

    // 80 Bit integer
    uint8_t value[10];

    // 16 Bit IDT size.
    *reinterpret_cast<uint16_t*>(value) = IDT_SIZE * IDT_ENTRY_SIZE - 1;

    // 64 Bit pointer to the IDT.
    *reinterpret_cast<uint64_t*>(&reinterpret_cast<uint16_t*>(value)[1]) = reinterpret_cast<uint64_t>(table);

    // Load the IDT in assembly.
    asm volatile(
        "lidt %0"
        :
        : "m" (value)
    );

}

InterruptHandler::InterruptHandler(){}
InterruptHandler::~InterruptHandler(){}

uint64_t InterruptHandler::HandleInterrupt(uint64_t rsp){
    return rsp;
}

InterruptManager * InterruptManager::activeManager = 0;

InterruptManager::InterruptManager(uint64_t hardwareInterruptOffset) :
    picMasterCommandPort(PIC1_COMMAND_PORT),
    picMasterDataPort(PIC1_DATA_PORT),
    picSlaveCommandPort(PIC2_COMMAND_PORT),
    picSlaveDataPort(PIC2_DATA_PORT)
{

    this->hardwareInterruptOffset = hardwareInterruptOffset;
    this->codeSegment = KERNEL_GDT_ENTRY * GDT_ENTRY_SIZE;

    for(uint32_t i = 0; i < IDT_SIZE; i++){
        idt.SetEntry(i, &IgnoreInterrupt, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
        this->handlers[i] = NULL;
    }

    idt.SetEntry(0x16, &HandleException0x00, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
    idt.SetEntry(0x16, &HandleException0x01, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
    idt.SetEntry(0x16, &HandleException0x02, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
    idt.SetEntry(0x16, &HandleException0x03, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
    idt.SetEntry(0x16, &HandleException0x04, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
    idt.SetEntry(0x16, &HandleException0x05, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
    idt.SetEntry(0x16, &HandleException0x06, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
    idt.SetEntry(0x16, &HandleException0x07, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
    idt.SetEntry(0x16, &HandleException0x08, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
    idt.SetEntry(0x16, &HandleException0x09, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
    idt.SetEntry(0x16, &HandleException0x0A, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
    idt.SetEntry(0x16, &HandleException0x0B, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
    idt.SetEntry(0x16, &HandleException0x0C, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
    idt.SetEntry(0x16, &HandleException0x0D, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
    idt.SetEntry(0x16, &HandleException0x0E, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
    idt.SetEntry(0x16, &HandleException0x0F, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
    idt.SetEntry(0x16, &HandleException0x10, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
    idt.SetEntry(0x16, &HandleException0x11, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
    idt.SetEntry(0x16, &HandleException0x12, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
    idt.SetEntry(0x16, &HandleException0x13, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);

    idt.SetEntry(hardwareInterruptOffset, &HandleInterruptRequest0x00, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);
    idt.SetEntry(hardwareInterruptOffset + 0x01, &HandleInterruptRequest0x01, codeSegment, IDT_PRIVILEGE0 | IDT_INTERRUPT_GATE);

    // Restart PICs.
    picMasterCommandPort.Write(0x11);
    picSlaveCommandPort.Write(0x11);

    // Remap PICs to proper interrupt vector offsets.
    picMasterDataPort.Write(hardwareInterruptOffset);
    picSlaveDataPort.Write(hardwareInterruptOffset + 8);

    picMasterDataPort.Write(0x04);
    picSlaveDataPort.Write(0x02);

    picMasterDataPort.Write(0x01);
    picSlaveDataPort.Write(0x01);

    picMasterDataPort.Write(0x00);
    picSlaveDataPort.Write(0x00);

    idt.LoadIDT();
}

InterruptManager::~InterruptManager(){
    Deactivate();
}

void InterruptManager::Activate(){
    if(activeManager != NULL)
        activeManager->Deactivate();
    
    activeManager = this;
    asm("sti");
}

void InterruptManager::Deactivate(){
    if(activeManager == this){
        activeManager = NULL;
        asm("cli");
    }
}

void InterruptManager::SetInterruptHandler(uint8_t interruptNumber, InterruptHandler * handler){
    handlers[interruptNumber] = handler;
}

// Single Interrupt Handler
extern "C" uint64_t HandleInterrupt(uint64_t rsp, uint8_t interruptNumber){
    if(InterruptManager::activeManager != NULL)
        return InterruptManager::activeManager->HandleInterrupt(rsp, interruptNumber);
    return rsp;
}

uint64_t InterruptManager::HandleInterrupt(uint64_t rsp, uint8_t interruptNumber){

    // If this interrupt has a handler, handle it.
    if(handlers[interruptNumber] != NULL){
        rsp = handlers[interruptNumber]->HandleInterrupt(rsp);
    }

    // Acknowledge hardware interrupts.
    if(interruptNumber >= hardwareInterruptOffset && interruptNumber < hardwareInterruptOffset + 0x10){
        picMasterCommandPort.Write(0x20);
        
        if(hardwareInterruptOffset + 8 <= interruptNumber)
            picSlaveCommandPort.Write(0x20);
    }

    return rsp;

}