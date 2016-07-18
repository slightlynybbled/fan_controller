/*
 * File:   main.c
 * Author: Jason
 */
#include "config.h"
#include "libmathq15.h"
#include "task.h"
#include "dio.h"

/*********** Useful defines and macros ****************************************/

/*********** Variable Declarations ********************************************/

/*********** Function Declarations ********************************************/
void initOsc(void);
void initInterrupts(void);
void initIO(void);
void initPwm(void);

void incPwm(void);

void setDutyCycleFan0(q15_t dutyCycle);
void setDutyCycleFan1(q15_t dutyCycle);
void setDutyCycleFan2(q15_t dutyCycle);
void setDutyCycleFan3(q15_t dutyCycle);

/*********** Function Implementations *****************************************/
int main(void) {
    /* setup the hardware */
    initOsc();
    initInterrupts();
    initIO();
    initPwm();
    
    /* initialize the task manager */
    TASK_init();
    
    /* add tasks */
    TASK_add(&incPwm, 1);
    
    /* set the initial duty cycles */
    setDutyCycleFan0(0);
    setDutyCycleFan1(0);
    setDutyCycleFan2(0);
    setDutyCycleFan3(0);
    
    TASK_manage();
    
    return 0;
}
/******************************************************************************/
/* Tasks below this line */
void incPwm(void){
    static q15_t dc = 16384;
    
    dc += 1;
    if(dc < 0)
        dc = 0;
    
    setDutyCycleFan0(dc);
    setDutyCycleFan1(dc);
    setDutyCycleFan2(dc);
    setDutyCycleFan3(dc);
}

/******************************************************************************/
/* Helper functions below this line */
void setDutyCycleFan0(q15_t dutyCycle){
    CCP2RB = q15_mul(dutyCycle, CCP2PRL);
    
    /* when CCPxRB == 0, PWM doesn't update properly */
    if(CCP2RB < 2)
        CCP2RB = 2;
    else if(CCP2RB >= CCP2PRL)
        CCP2RB = CCP2PRL - 1;
    
    return;
}

void setDutyCycleFan1(q15_t dutyCycle){
    CCP5RB = q15_mul(dutyCycle, CCP5PRL);
    
    /* when CCPxRB == 0, PWM doesn't update properly */
    if(CCP5RB < 2)
        CCP5RB = 2;
    else if(CCP5RB >= CCP5PRL)
        CCP5RB = CCP5PRL - 1;
}

void setDutyCycleFan2(q15_t dutyCycle){
    CCP1RB = q15_mul(dutyCycle, CCP1PRL);
    
    /* when CCPxRB < 2, PWM doesn't update properly */
    if(CCP1RB < 2)
        CCP1RB = 2;
    else if(CCP1RB >= CCP1PRL)
        CCP1RB = CCP1PRL - 1;
    
    return;
}

void setDutyCycleFan3(q15_t dutyCycle){
    CCP4RB = q15_mul(dutyCycle, CCP4PRL);
    
    /* when CCPxRB == 0, PWM doesn't update properly */
    if(CCP4RB < 2)
        CCP4RB = 2;
    else if(CCP4RB >= CCP4PRL)
        CCP4RB = CCP4PRL - 1;
}

/******************************************************************************/
/* Initialization functions below this line */
void initOsc(void){
    CLKDIV = 0;

    return;
}

void initInterrupts(void){
    /* configure the global interrupt conditions */
    /* interrupt nesting disabled, DISI instruction active */
    INTCON1 = 0x8000;
    INTCON2 = 0x4000;
    
    return;
}

void initIO(void){
    /* fan PWM outputs */
    DIO_makeOutput(DIO_PORT_B, 11); /* PWM fan0 */
    DIO_makeOutput(DIO_PORT_A, 7);  /* PWM fan1 */
    DIO_makeOutput(DIO_PORT_B, 7);  /* PWM fan2 */
    
    DIO_makeInput(DIO_PORT_B, 5);   /* PWM fan3 */
    DIO_makeOutput(DIO_PORT_B, 9);  /* PWM fan3 */
    
    /* fan tach inputs */
    DIO_makeInput(DIO_PORT_B, 13);  /* tach fan0 */
    DIO_makeInput(DIO_PORT_A, 6);   /* tach fan1 */
    DIO_makeInput(DIO_PORT_B, 8);   /* tach fan2 */
    DIO_makeInput(DIO_PORT_B, 6);   /* tach fan3 */
    
    /* PWM input, should be configured as an input capture */
    DIO_makeInput(DIO_PORT_B, 12);
}

void initPwm(void){
    /* Initialize MCCP/SCCP modules */
    
    /* period registers */
    CCP1PRH = CCP2PRH = CCP4PRH = CCP5PRH = 0;
    CCP1PRL = CCP2PRL = CCP4PRL = CCP5PRL = 640;
    
    CCP1CON1L = CCP2CON1L = CCP4CON1L = CCP5CON1L = 0x0005;
    CCP1CON1H = CCP2CON1H = CCP4CON1H = CCP5CON1H = 0x0000;
    CCP1CON2L = CCP2CON2L = CCP4CON2L = CCP5CON2L = 0x0000;
    
    CCP2CON2H = 0x8100; // enable output 0C2A (fan0)
    CCP5CON2H = 0x8100; // enable output OC5 (fan1)
    CCP1CON2H = 0x8100; // enable output OC1A (fan2)
    CCP4CON2H = 0x8100; // enable output OC4 (fan3)
    
    CCP1CON3L = CCP2CON3L = 0;  // dead time disabled
    CCP1CON3H = CCP2CON3H = CCP4CON3H = CCP5CON3H = 0x0020;
    
    CCP1CON1Lbits.CCPON = 
            CCP2CON1Lbits.CCPON = 
            CCP4CON1Lbits.CCPON = 
            CCP5CON1Lbits.CCPON = 1;
    
    /* duty cycle registers */
    CCP1RA = CCP2RA = CCP4RA = CCP5RA = 0;
    CCP1RB = CCP2RB = CCP4RB = CCP5RB = 0;
    
    return;
}

