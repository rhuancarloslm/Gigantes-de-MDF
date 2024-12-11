/*
               +--\/--+
          PC6  1|    |28  PC5 (A5/ADC5)
RXD  (D0) PD0  2|    |27  PC4 (A4/ADC4)
TXD  (D1) PD1  3|    |26  PC3 (A3/ADC3)
     (D2) PD2  4|    |25  PC2 (A2/ADC2)
PWM  (D3) PD3  5|    |24  PC1 (A1/ADC1)
XCK  (D4) PD4  6|    |23  PC0 (A0/ADC0)
          VCC  7|    |22  GND
          GND  8|    |21  AREF
          PB6  9|    |20  AVCC
          PB7 10|    |19  PB5 (D13)
OC0B (D5) PD5 11|    |18  PB4 (D12)
OC0A (D6) PD6 12|    |17  PB3 (D11) PWM
     (D7) PD7 13|    |16  PB2 (D10) PWM
     (D8) PB0 14|    |15  PB1 (D9)  PWM
                +----+
*/

#include <avr/io.h>
#include <avr/interrupt.h>

#define FREC_CPU 16000000UL
#define USART_BAUDRATE 115200
#define BAUD_PRESCALER ((FREC_CPU / 16 / USART_BAUDRATE) - 1)

// Define as portas
#define MOTOR_PORT		PORTB	// Indica se está ligado ou desligado
#define MOTOR_DDR		DDRB	// Define quais pinos são entrada ou saída
#define INPUT2			PB1		// INPUT PARA A PONTE H -- Definir como OUTPUT---- INPUT2A
#define INPUT1			PB2		// INPUT PARA A PONTE H -- Definir como OUTPUT --  INPUT1A
#define INPUT4			PB3		// INPUT PARA A PONTE H -- Definir como OUTPUT --- INPUT4A

#define LED_PORT		PORTC	// Indica se está ligado ou desligado
#define LED_DDR			DDRC	// Define quais pinos são entrada ou saída
#define RECEPTOR_PIN	PC0		// Pino do receptor de LASER_PIN 
#define LED_PIN_1		PC2		// LEDs para indicar vida
#define LED_PIN_2		PC3
#define LED_PIN_3		PC4
#define LASER_PIN		PC5		// Usaremos PC5 para o controle do LASER_PIN -- Definir como OUTPUT

#define INPUT3_PORT		PORTD	// Indica se está ligado ou desligado
#define INPUT3_DDR		DDRD	// Define quais pinos são entrada ou saída
#define INPUT3			PD3		// INPUT PARA A PONTE H -- Definir como OUTPUT --- INPUT3A

volatile uint8_t vidas = 3;		// Inicialmente 3 vidas
volatile uint8_t tot = 0;		// Estouro do timer
volatile uint8_t USART_Buffer;	// Variável que recebe comando do serial

// Função para configurar os pinos
void setup()
{
	MOTOR_DDR |= (1 << INPUT1) | (1 << INPUT2) | (1 << INPUT4);
	INPUT3_DDR |= (1 << INPUT3);
	DDRC |= (1 << PC1);

	// Configura LED's e laser como saída
	LED_DDR |= (1 << LED_PIN_1) | (1 << LED_PIN_2) | (1 << LED_PIN_3);
	LED_DDR |= (1 << LASER_PIN);
	LED_PORT |= (1 << LED_PIN_1) | (1 << LED_PIN_2) | (1 << LED_PIN_3);	// Inicialmente, todos LED's ligados

	// Seta o receptor como entrada
	LED_DDR &= ~(1 << RECEPTOR_PIN);
	LED_PORT |= (1 << RECEPTOR_PIN);	// Habilita pull-up

	// Habilita a interrupção no receptor (INT0)  -  Reset dos LED's
	EICRA |= (0b10 << ISC00);	// Interrupção na borda de descida
	EIMSK |= (1 << INT0);		// Habilita INT0

	// Habilitar interrupção no receptor (PCINT8) --  INTERRUPÇAO DO LDR
	PCICR  |= (1 << PCIE1);
	PCMSK1 |= (1 << PCINT8);

	// Habilitar interrupção do Timer0 para o LASER_PIN
	TIMSK0 |= (1 << OCIE0A);
	TCCR0A |= (1 << WGM01);	// Modo CTC
	TCCR0B |= (1 << CS02) | (1 << CS00);	// Prescaler 1024
	OCR0A = 156;	// Aproximadamente 1 segundo - Max value

	// Habilitar interrupção do Timer1 para conseguir rotacionar carrinho em 180º
	TCCR1B |= (1 << WGM12);	// Modo CTC
	TCCR1B |= (1 << CS12) | (1 << CS10);	// Prescaler 1024
	OCR1A = 45000;	// Aproximadamente 1 segundo -- 

	//ADC - Configuração do LDR - prescaler de 128
	ADCSRA |= (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
	ADMUX |= (1 << REFS0);

	// Seta o baud rate
	UBRR0H = BAUD_PRESCALER >> 8;
	UBRR0L = BAUD_PRESCALER;

	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);	// 8-bit data, 1 stop bit

	UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);

	sei();	// Habilitar interrupções globais
}

void stopTimer0(void)
{
	// Desabilita a interrupção do Timer 1
	TIMSK0 &= ~(1 << OCIE0A);
	tot = 0;
}

void startTimer0(void) {
	TIMSK0 |= (1 << OCIE0A);
}

void stopTimer1(void)
{
	// Desabilita a interrupção do Timer 1
	TIMSK1 &= ~(1 << OCIE1A);
}

void startTimer1(void) {
	TIMSK1 |= (1 << OCIE1A);
}

uint16_t adc_read(uint8_t channel) {
	// Seleciona o canal ADC
	ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);

	// Começa a conversão
	ADCSRA |= (1 << ADSC);

    // Espera a conversão ser finalizada
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

// Função de Interrupção para tratar os comandos recebidos
ISR(USART_RX_vect)
{
	USART_Buffer = UDR0;	// Recebe valor do serial
	
	if(USART_Buffer == 'W')	// Frente
	{
    PORTB |= (1 << INPUT1);		// 10 - m1p1
    PORTB &= ~(1 << INPUT4);	// 11 - m1p2
    PORTD |= (1 << INPUT3);		// 3 - m2p1
    PORTB &= ~(1 << INPUT2);	// 9 - m2p2
	}
	
	// Abaixo são as funções de release, ou seja, quando o botão estiver solto
	else if(USART_Buffer == 'A')	// Esquerda
	{
    PORTB &= ~(1 << INPUT1);	// 10 - m1p1
    PORTB &= ~(1 << INPUT4);	// 11 - m1p2
    PORTD |= (1 << INPUT3);		// 3 - m2p1
    PORTB &= ~(1 << INPUT2);	// 9 - m2p2
	}

	else if(USART_Buffer == 'S')	// Ré
	{
    PORTB &= ~(1 << INPUT1);	// 10 - m1p1
    PORTB |= (1 << INPUT4);		// 11 - m1p2
    PORTD &= ~(1 << INPUT3);	// 3 - m2p1
    PORTB |= (1 << INPUT2);		// 9 - m2p2
	}

	else if(USART_Buffer == 'D')	// Direita
	{
    PORTB |= (1 << INPUT1);		// 10 - m1p1
    PORTB &= ~(1 << INPUT4);	// 11 - m1p2
    PORTD &= ~(1 << INPUT3);	// 3 - m2p1
    PORTB &= ~(1 << INPUT2);	// 9 - m2p2
	}

	else if(USART_Buffer == 'X')//Para
	{
    PORTB &= ~(1 << INPUT1); 	// 10 - m1p1
    PORTB &= ~(1 << INPUT4);	// 11 - m1p2
    PORTD &= ~(1 << INPUT3);	// 3 - m2p1
    PORTB &= ~(1 << INPUT2);	// 9 - m2p2
	}
}

// Função para controle do LASER_PIN
ISR(TIMER0_COMPA_vect)
{
	if (tot >= 100)
	{
		LED_PORT ^= (1 << LASER_PIN);  // Alterna o estado do laser a cada 1 segundo
        tot = 0;
	}
	else
	{
		tot++;
	}
}

ISR(TIMER1_COMPA_vect)
{
	if (TCNT1 >= OCR1A)
	{
		MOTOR_PORT &= ~(1 << INPUT1);  
        INPUT3_PORT &= ~(1 << INPUT3);
	}
}

// Função para detectar laser no receptor
ISR(PCINT1_vect)
{
    // Lê o valor do LDR
    uint16_t ldr_value = adc_read(0);

	if (PINC & (1 << PC0))
	{
    	if (ldr_value < 600)
		{
            PORTD &= ~(1 << LASER_PIN);

	        if (vidas == 3)
			{
	            LED_PORT &= ~(1 << LED_PIN_3);
	            vidas--;	// Decrementa vida
			}
	        else if (vidas == 2)
			{
	            LED_PORT &= ~(1 << LED_PIN_2);
	            vidas--;	// Decrementa vida
	        } 
	        else if (vidas == 1)
			{
	            LED_PORT &= ~(1 << LED_PIN_1);
	            vidas--;	Decrementa vida
	        }

            MOTOR_PORT |= (1 << INPUT1);
            INPUT3_PORT |= (1 << INPUT3);
            MOTOR_PORT &= ~(1 << INPUT2);
            MOTOR_PORT &= ~(1 << INPUT4);
            stopTimer0();
            startTimer1();
	    }
	}	
}

ISR(INT0_vect){
	cli();
	LED_PORT |= (1 << LED_PIN_1) | (1 << LED_PIN_2) | (1 << LED_PIN_3);
	vidas = 3;
	sei();
}

// Função principal
int main(void) {
	setup();

    while (1)
	{
	}
}
