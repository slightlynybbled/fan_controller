/*
 * File:   main.c
 * Author: Jason
 */
#include "config.h"
#include "libmathq15.h"
#include "task.h"
#include "dio.h"
#include "eeprom.h"

/*********** Useful defines and macros ****************************************/
typedef enum {eINIT, eFAN_START, eNORMAL, eFAN_ADJ} FanState;

#define FAN_ADJUST_TIMEOUT  5000
#define MIN_INPUT_DC        2500
#define MIN_FAN_DC          3277
#define NUM_OF_FANS         4
#define MILLISECONDS_AFTER_PWM_TO_FULL_SPEED 100

#define SWITCH_PORT DIO_PORT_B
#define SWITCH_PIN  3
#define SWITCH      PORTBbits.RB3

#define ENC_A_PORT  DIO_PORT_A
#define ENC_A_PIN   0
#define ENC_B_PORT  DIO_PORT_B
#define ENC_B_PIN   2

/*********** Variable Declarations ********************************************/
FanState fanState = eINIT;
uint32_t lastEncoderTime = 0;
uint8_t switchPressed = 0;
q15_t encoderTurned = 0;

q15_t dcFan[NUM_OF_FANS] = {0};
q15_t targetDcFan[NUM_OF_FANS] = {0};

/*********** Function Declarations ********************************************/
void initOsc(void);
void initInterrupts(void);
void initIO(void);
void initPwm(void);
void initAdc(void);

void serviceFanState(void);
void serviceSwitch(void);
void serviceEncoder(void);

void setDutyCycleFan(uint8_t fan, q15_t dutyCycle);
void setDutyCycleFan0(q15_t dutyCycle);
void setDutyCycleFan1(q15_t dutyCycle);
void setDutyCycleFan2(q15_t dutyCycle);
void setDutyCycleFan3(q15_t dutyCycle);
q15_t rampDc(q15_t dc, q15_t targetDc);

/*********** Function Implementations *****************************************/
int main(void) {
    /* setup the hardware */
    initOsc();
    initInterrupts();
    initIO();
    initPwm();
    initAdc();
    
    /* initialize the task manager */
    TASK_init();
    
    /* add tasks */
    TASK_add(&serviceFanState, 10);
    TASK_add(&serviceSwitch, 1);
    TASK_add(&serviceEncoder, 1);
    
    TASK_manage();
    
    return 0;
}

/******************************************************************************/
/* Tasks below this line */
void serviceFanState(void){
    static uint8_t lastFanAdjusted = 0;
    
    /* ADC conversion to determine the input duty cycle */
    AD1CON1bits.SAMP = 0;
    while(!AD1CON1bits.DONE);   // ...wait for the ADC to finish...
    q15_t inputPwmDutyCycle = (q15_t)(ADC1BUF0 >> 1);
    
    switch(fanState){
        case eINIT:
        {
            uint8_t i;
            for(i = 0; i < NUM_OF_FANS; i++){
                dcFan[i] = 0;
                setDutyCycleFan(i, 0);
                
                targetDcFan[i] = EEPROM_read(i);
                
                if(targetDcFan[i] == 0){
                    targetDcFan[i] = 32767;
                    EEPROM_write(i, 32767);
                }
            }
            
            fanState = eFAN_START;
            switchPressed = 0;
            lastFanAdjusted = 0;
            
            break;
        }
        
        case eFAN_START:
        {
            /* start the fans */
            uint8_t i;
            for(i = 0; i < NUM_OF_FANS; i++){
                if(dcFan[i] != targetDcFan[i]){
                    dcFan[i] = rampDc(dcFan[i], targetDcFan[i]);
                    setDutyCycleFan(i, dcFan[i]);
                    break;
                }
            }
            
            /* determine when to exit the eFAN_START state */
            uint8_t exitCondition = 1;
            for(i = 0; i < NUM_OF_FANS; i++){
                if(dcFan[i] != targetDcFan[i])
                    exitCondition = 0;
            }
            if(exitCondition)
                fanState = eNORMAL;
            
            /* deal with the adjust button being pressed */
            if(switchPressed){
                fanState = eFAN_ADJ;
                lastFanAdjusted = 0;
                encoderTurned = 0;
            }
            switchPressed = 0;
            
            break;
        }
        
        case eNORMAL:
        {
            /* deal with the adjust button being pressed */
            if(switchPressed){
                fanState = eFAN_ADJ;
                lastFanAdjusted = 0;
                encoderTurned = 0;
            }
            switchPressed = 0;
            

            /* scale the target duty cycle to the input duty cycle */
            uint8_t i;
            for(i = 0; i < NUM_OF_FANS; i++){
                q15_t dc;
                
                dc = q15_mul(inputPwmDutyCycle, targetDcFan[i]);

                if(dc < MIN_FAN_DC)
                    dc = MIN_FAN_DC;
                
                if(inputPwmDutyCycle < MIN_INPUT_DC)
                    dc = 0;
                
                setDutyCycleFan(i, dc);
            }
            
            break;
        }
        
        case eFAN_ADJ:
        {
            /* when the encoder is being turned quickly, then move the
             * duty cycle quickly, else when the encoder is being turned
             * slowly, then move the duty cycle slowly */
            q15_t increment, dc;
            if((encoderTurned < 4) && (encoderTurned > -4))
                increment = 10 * encoderTurned;
            else
                increment = 100 * encoderTurned;
            
            /* dc = inc + lastDc */
            dc = q15_add(increment, dcFan[lastFanAdjusted]);
            
            /* place limits on the duty cycle */
            if(dc < 0)
                dc = 0;
            else if((dcFan[lastFanAdjusted] > 0) && (dc < MIN_FAN_DC))
                dc = 0;
            else if((dcFan[lastFanAdjusted] == 0) && (dc > 0))
                dc = MIN_FAN_DC;
            
            dcFan[lastFanAdjusted] = dc;
            
            encoderTurned = 0;
            
            /* deal with a timeout */
            if(TASK_getTime() > (lastEncoderTime + FAN_ADJUST_TIMEOUT)){
                fanState = eINIT;
                
                targetDcFan[lastFanAdjusted] = dcFan[lastFanAdjusted];
                EEPROM_write(lastFanAdjusted, dcFan[lastFanAdjusted]);
            }
            
            /* deal with the adjust button being pressed */
            if(switchPressed){
                targetDcFan[lastFanAdjusted] = dcFan[lastFanAdjusted];
                EEPROM_write(lastFanAdjusted, dcFan[lastFanAdjusted]);
                
                lastFanAdjusted++;
                if(lastFanAdjusted > NUM_OF_FANS)
                    lastFanAdjusted = 0;
            }
            switchPressed = 0;
            
            /* set the appropriate duty cycles - only the fan currently
             * being adjusted should be greater than 0 */
            uint8_t i;
            for(i = 0; i < NUM_OF_FANS; i++){
                if(i == lastFanAdjusted){
                    setDutyCycleFan(i, dcFan[i]);
                }else{
                    setDutyCycleFan(i, 0);
                }
            }
            
            break;
        }
        
        default:
        {
            while(1);   // programmer's trap
        }
        
    }
}

void serviceSwitch(void){
    static uint8_t switchArray = 0;
    static uint8_t state = 0;
    
    /* debounce */
    switchArray <<= 1;
    if(SWITCH){
        switchArray |= 0x01;
    }else{
        switchArray &= 0xfe;
    }
    
    /* edge detector */
    if((switchArray == 0xff) && (state == 0)){
        /* switch just released */
        state = 1;
        lastEncoderTime = TASK_getTime();
        
        switchPressed = 1;
    }else if((switchArray == 0x00) && (state == 1)){
        /* switch just pressed */
        state = 0;
        lastEncoderTime = TASK_getTime();
    }
}

void serviceEncoder(void){
    /* I found this routine at 
     * https://www.circuitsathome.com/mcu/reading-rotary-encoder-on-arduino 
     * appears to work well enough and I would like to give credit where
     * it is due. */
    
    static int8_t enc_states[] = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
    static uint8_t old_AB = 0;
    
    uint8_t new_AB = PORTAbits.RA0 | (PORTBbits.RB2 << 1);
    
    old_AB <<= 2;
    old_AB |= new_AB;
    
    int8_t state = enc_states[(old_AB & 0x0f)];
    
    if(state == 1){
        /* counter-clockwise */
        lastEncoderTime = TASK_getTime();
        encoderTurned--;    // reverse the direction of CW and CCW
    }else if(state == -1){
        /* clockwise */
        lastEncoderTime = TASK_getTime();
        encoderTurned++;    // reverse the direction of CW and CCW
    }
}

/******************************************************************************/
/* Helper functions below this line */
void setDutyCycleFan(uint8_t fan, q15_t dutyCycle){
    switch(fan){
        case 0:     setDutyCycleFan0(dutyCycle);    break;
        case 1:     setDutyCycleFan1(dutyCycle);    break;
        case 2:     setDutyCycleFan2(dutyCycle);    break;
        case 3:     setDutyCycleFan3(dutyCycle);    break;
        default:                                    break;
    }
}

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
    CCP4RB = q15_mul(dutyCycle, CCP4PRL);
    
    /* when CCPxRB == 0, PWM doesn't update properly */
    if(CCP4RB < 2)
        CCP4RB = 2;
    else if(CCP4RB >= CCP4PRL)
        CCP4RB = CCP4PRL - 1;
}

void setDutyCycleFan3(q15_t dutyCycle){
    CCP1RB = q15_mul(dutyCycle, CCP1PRL);
    
    /* when CCPxRB < 2, PWM doesn't update properly */
    if(CCP1RB < 2)
        CCP1RB = 2;
    else if(CCP1RB >= CCP1PRL)
        CCP1RB = CCP1PRL - 1;
    
    return;
}

q15_t rampDc(q15_t dc, q15_t targetDc){
    const q15_t rampIncrement = 50;
    q15_t newDc = 0;
    
    if(targetDc < MIN_FAN_DC){
        newDc = 0;
    }else if((targetDc >= MIN_FAN_DC) && (dc == 0)){
        newDc = MIN_FAN_DC;
    }else if(targetDc > dc){
        /* ramp the fan to its target */
        q15_t dcDiff = q15_add(targetDc, -dc);
        if(dcDiff > rampIncrement)
            dcDiff = rampIncrement;
        
        newDc = q15_add(dc, dcDiff);
    }
    
    return newDc;
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
    
    /* allow change notification interrupts */
    IFS1bits.CNIF = 0;
    IEC1bits.CNIE = 1;
    
    /* enable CN interrupt for PWM and tach measurements */
    CNEN1bits.CN14IE = 1;
    CNEN1bits.CN13IE = 1;
    
    return;
}

void initIO(void){
    /* debugging outputs */
    DIO_makeOutput(DIO_PORT_A, 2);
    DIO_makeOutput(DIO_PORT_A, 3);
        
    /* encoder inputs */
    DIO_makeInput(ENC_A_PORT, ENC_A_PIN);
    DIO_makeInput(ENC_B_PORT, ENC_B_PIN);
    DIO_makeInput(SWITCH_PORT, SWITCH_PIN);
    
    DIO_makeDigital(ENC_A_PORT, ENC_A_PIN);
    DIO_makeDigital(ENC_B_PORT, ENC_B_PIN);
    DIO_makeDigital(SWITCH_PORT, SWITCH_PIN);
    
    /* enable switch and encoder pull-down resistors */
    CNPD1bits.CN2PDE = 1;
    CNPD1bits.CN6PDE = 1;
    CNPD1bits.CN7PDE = 1;
    
    /* fan PWM outputs */
    DIO_makeOutput(DIO_PORT_B, 12); /* PWM fan0 */
    DIO_makeOutput(DIO_PORT_A, 7);  /* PWM fan1 */
    DIO_makeOutput(DIO_PORT_B, 9);  /* PWM fan2 */
    DIO_makeOutput(DIO_PORT_B, 5);  /* PWM fan3 */
    
    /* fan tach inputs */
    DIO_makeInput(DIO_PORT_B, 13);  /* tach fan0 */
    DIO_makeInput(DIO_PORT_B, 10);  /* tach fan1 */
    DIO_makeInput(DIO_PORT_B, 8);   /* tach fan2 */
    DIO_makeInput(DIO_PORT_B, 6);   /* tach fan3 */
    
    DIO_makeDigital(DIO_PORT_B, 13);
    DIO_makeDigital(DIO_PORT_B, 8);
    DIO_makeDigital(DIO_PORT_B, 6);
    
    /* PWM input, should be configured as an input */
    DIO_makeInput(DIO_PORT_A, 1);
    DIO_makeAnalog(DIO_PORT_A, 1);
    
    /* tach output */
    DIO_makeOutput(DIO_PORT_B, 14);
}

void initPwm(void){
    /* Initialize MCCP/SCCP modules */
    
    /* period registers */
    CCP1PRH = CCP2PRH = CCP4PRH = CCP5PRH = 0;
    CCP1PRL = CCP2PRL = CCP4PRL = CCP5PRL = 640;
    
    CCP1CON1L = CCP2CON1L = CCP4CON1L = CCP5CON1L = 0x0005;
    CCP1CON1H = CCP2CON1H = CCP4CON1H = CCP5CON1H = 0x0000;
    CCP1CON2L = CCP2CON2L = CCP4CON2L = CCP5CON2L = 0x0000;
    
    CCP2CON2H = 0x8200; // enable output 0C2B (fan0)
    CCP5CON2H = 0x8100; // enable output OC5 (fan1)
    CCP4CON2H = 0x8100; // enable output OC4 (fan2)
    CCP1CON2H = 0x9000; // enable output OC1E (fan3)
    
    CCP1CON3L = CCP2CON3L = 0;  // dead time disabled
    
    /**/
    CCP1CON3H = CCP2CON3H = CCP4CON3H = CCP5CON3H = 0;
    CCP1CON3Hbits.POLACE = CCP4CON3Hbits.POLACE = CCP5CON3Hbits.POLACE = 1;
    CCP2CON3Hbits.POLBDF = 1;
    
    CCP1CON1Lbits.CCPON = 
            CCP2CON1Lbits.CCPON = 
            CCP4CON1Lbits.CCPON = 
            CCP5CON1Lbits.CCPON = 1;
    
    /* duty cycle registers */
    CCP1RA = CCP2RA = CCP4RA = CCP5RA = 0;
    CCP1RB = CCP2RB = CCP4RB = CCP5RB = 0;
    
    /* set the initial duty cycles */
    setDutyCycleFan0(0);
    setDutyCycleFan1(0);
    setDutyCycleFan2(0);
    setDutyCycleFan3(0);
    
    return;
}

void initAdc(void){
    /* set up the analog pin as analog inputs */
    DIO_makeInput(DIO_PORT_A, 1);
    DIO_makeAnalog(DIO_PORT_A, 1);
    
    AD1CON1 = 0x0200;   /* Clear sample bit to trigger conversion
                         * FORM = left justified  */
    AD1CON2 = 0x0000;   /* Set AD1IF after every 1 samples */
    AD1CON3 = 0x0007;   /* Sample time = 1Tad, Tad = 8 * Tcy */
    
    AD1CHS = 0x0101;    /* AN1 */
    AD1CSSL = 0;
    
    AD1CON1bits.ADON = 1; // turn ADC ON
    AD1CON1bits.ASAM = 1; // auto-sample
    
    return;
}

void _ISR _CNInterrupt(void){
    IFS1bits.CNIF = 0;
    
    /* reflect FAN0 tach to the motherboard tach */
    static uint8_t lastTachFan0 = 0;
    uint8_t tachFan0 = PORTBbits.RB13;
    if(tachFan0 != lastTachFan0){
        /* set the output tach based on fan 0 */
        if(tachFan0 == 0){
            LATBbits.LATB14 = 0;
        }else{
            LATBbits.LATB14 = 1;
        }
    }
    lastTachFan0 = tachFan0;
    
    LATAbits.LATA2 = 0;
}
