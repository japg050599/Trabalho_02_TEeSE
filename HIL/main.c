
//
// Included Files
// HIL
//
#include "driverlib.h"
#include "device.h"
#include "board.h"
#include <math.h>
#include "shared_vars.h"


//#pragma DATA_SECTION(iref,"CpuToCla1MsgRAM");
//float iref;


//=================================================
// Planta do conversor (igual ao cScript do trabalho 1)
//=================================================
#define DT         (1.0f/200000.0f)
#define TWO_PI      6.28318530718f
#define VDC        400.0f
#define LF         0.003f
#define RF         0.0377f
#define VG_AMP     (220.0f*1.41421356f)
#define A_PLANTA   ((1.0f - (RF*DT)/(2.0f*LF)) / (1.0f + (RF*DT)/(2.0f*LF)))
#define B_PLANTA   ((DT/(4.0f*LF)) / (1.0f + (2.0f*RF*DT)/(4.0f*LF)))
#define K_PLANTA   ((B_PLANTA*VDC)/2.0f)


#pragma DATA_SECTION(theta_grid, "CpuToCla1MsgRAM")
float theta_grid;

float iL = 0.0f;
float iL_old = 0.0f;
float vg = 0.0f;
float vg_old = 0.0f;
float theta_grid = 0.0f;
float S = 0;
float S_old = 0.0f;



volatile uint16_t io = 0;
volatile uint16_t vo = 0;

volatile uint16_t decimador = 0;

bool habilitaConta = 0;


bool S1 = 0;
bool S2 = 0;
bool S3 = 0;
bool S4 = 0;

float teste_k;
// ==============
//Buffers para visualização
// ==============
#define TAM_BUFFER 200
volatile uint16_t buffer_io[TAM_BUFFER];
volatile uint16_t buffer_vo[TAM_BUFFER];
volatile uint16_t buffer_S[TAM_BUFFER];
volatile uint16_t buffer_S1[TAM_BUFFER];
volatile uint16_t buffer_S3[TAM_BUFFER];
volatile uint16_t buffer_iref[TAM_BUFFER];
volatile uint16_t idx_buffer = 0;
volatile uint16_t buffer_cheio = 0;
static uint16_t dec_buffer = 0;


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

          if(habilitaConta)
            {
                
                habilitaConta = 0;

                S1 = GPIO_readPin(GPIO_S1);
                S2 = GPIO_readPin(GPIO_S2);
                S3 = GPIO_readPin(GPIO_S3);
                S4 = GPIO_readPin(GPIO_S4);

                // Rede 60 Hz

                vg = VG_AMP * sinf(theta_grid);
                

                theta_grid += TWO_PI*60.0f*DT;

                if(theta_grid >= TWO_PI)
                {
                    theta_grid -= TWO_PI;
                }

                // Planta

                S = (S1-S2-S3+S4)*1.0f;
                
                teste_k++;

                iL_old = iL;
                S_old = S;
                vg_old = vg;

                iL = A_PLANTA*iL_old + K_PLANTA*(S + S_old) - B_PLANTA*(vg + vg_old);



                // Saídas da planta

                io = (uint16_t)(HALF_DAC_VALUE + ((iL * DIV_SCALE_I) * HALF_DAC_VALUE));
                vo = (uint16_t)(HALF_DAC_VALUE + ((vg * DIV_SCALE_V) * HALF_DAC_VALUE));



                if(io > 4095U)
                    {
                        io = 4095U;
                    }
                    else
                    {
                        io = io;
                    }

                    if(vo > 4095U)
                    {
                        vo = 4095U;
                    }
                    else
                    {
                        vo = vo;
                    }

//======================= Buffers ======================
                    dec_buffer++;

                    if(dec_buffer >= 100)
                    {
                        dec_buffer = 0;

                    buffer_io[idx_buffer] = (uint16_t)(2048 + iL*10.0f);
                    buffer_vo[idx_buffer] = (uint16_t)(2048 + vg);
                    buffer_S[idx_buffer] = (uint16_t)(2048 + S);
                    buffer_S1[idx_buffer] = (uint16_t)(2048 + S1);
                    buffer_S3[idx_buffer] = (uint16_t)(2048 + S3);



                        idx_buffer++;

                        if(idx_buffer >= TAM_BUFFER)
                        {
                            idx_buffer = 0;
                            buffer_cheio = 1;
                        }
                    }
//======================= Buffers ======================

                DAC_setShadowValue(DAC_IO_BASE, io);
                DAC_setShadowValue(DAC_VO_BASE, vo);


            }
        }

}



__interrupt void INT_ADC0_1_ISR(void)
{
    habilitaConta = 1;   // planta roda a 200 kHz

    decimador++;

    if(decimador >= 10)
    {
        decimador = 0;

        CLA_forceTasks(myCLA0_BASE, CLA_TASKFLAG_1);
    }

    ADC_clearInterruptStatus(ADC0_BASE, ADC_INT_NUMBER1);
    Interrupt_clearACKGroup(INT_ADC0_1_INTERRUPT_ACK_GROUP);
}

