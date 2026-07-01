
//
// Included Files
//PIL
//
#include "driverlib.h"
#include "device.h"
#include "board.h"
#include "scicomm.h"
#include <math.h>

#pragma DATA_SECTION(vo,"CpuToCla1MsgRAM");
float vo;
#pragma DATA_SECTION(io,"CpuToCla1MsgRAM");
float io;
#pragma DATA_SECTION(iref,"CpuToCla1MsgRAM");
float iref;
#pragma DATA_SECTION(d,"Cla1ToCpuMsgRAM");
float d;
#pragma DATA_SECTION(teste,"Cla1ToCpuMsgRAM");
float teste;

float rxData[2];

float k=0;


#define FGRID       60.0f
#define TS_CONTROL  (1.0f/20000.0f)
#define AMP_IREF    12.8565f
#define AMP_IREF_STEP    AMP_IREF/2.0f
#define TWO_PI      6.28318530718f
#define STEP        0.0708334f


float theta = 0.0f;
float tempo = 0.0f;

void updateIref60Hz(void)
{
    
// Comentar estrutura if e descomentar "iref" para regime permanente
    if(theta >= TWO_PI)
    {
        theta -= TWO_PI;
    }
    if (tempo >= STEP) {
    
    iref =  AMP_IREF_STEP  * sinf(theta);

    }else {

    iref = AMP_IREF * sinf(theta);

    }
    //iref = AMP_IREF * sinf(theta);
    
    theta += TWO_PI * FGRID * TS_CONTROL;

    tempo += TS_CONTROL;





}

void main(void)
{
        Device_init();

        Interrupt_initModule();

        Interrupt_initVectorTable();

        Board_init();
        EINT;
        ERTM;

        while(1)
        {

        }

}

__interrupt void cla1Isr1 ()
{
    protocolSendData(SCI0_BASE, &d,sizeof(float));
    Interrupt_clearACKGroup(INT_myCLA01_INTERRUPT_ACK_GROUP);
    Interrupt_clearACKGroup(INT_SCI0_RX_INTERRUPT_ACK_GROUP);
    SCI_clearInterruptStatus(SCI0_BASE,SCI_INT_RXFF);
}

__interrupt void INT_SCI0_RX_ISR ()
{

    protocolReceiveData(SCI0_BASE,rxData,sizeof(rxData));
    

    vo = rxData[0];
    io = rxData[1];

    k++;
   
    updateIref60Hz();

    CLA_forceTasks(myCLA0_BASE,CLA_TASKFLAG_1);
    

}


__interrupt void INT_myCPUTIMER0_ISR(void)
{
    //updateIref60Hz();
  
    //
    // Clear interrupt flag
    //
    CPUTimer_clearOverflowFlag(myCPUTIMER0_BASE);

    //
    // Acknowledge interrupt group 1
    //
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}