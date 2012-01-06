#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define ADC1	1
#define LED1	PB0
#define LED2	PB1
#define RLED	PB2
#define BTN1	PB3
#define BTN2	PB4
#define PWM1	OCR0A
#define PWM2	OCR0B

#define BIT_GET(x, y)	(((x) >> (y)) & 1)
#define BIT_SET(x, y)	((x) |= _BV(y))
#define BIT_CLR(x, y)	((x) &= ~_BV(y))
#define BIT_INV(x, y)	((x) ^= _BV(y))

#define BTN_PRESSED(x)	(! BIT_GET(PINB, (x)))

uint8_t flare_vals[] EEMEM = {
0,1,2,3,4,5,6,6,6,7,7,8,8,9,9,10,11,12,12,13,14,15,16,17,19,20,
21,23,24,26,28,30,32,34,36,39,42,44,48,51,54,58,62,66,71,76,81,
87,93,100,106,114,122,130,139,149,159,170,182,195,208,223,238,255 };

uint8_t random_vals[] PROGMEM = {
234,191,103,250,144,74,39,34,215,128,9,122,144,74,137,105,123,218,158,175,205,
118,149,13,98,7,173,179,194,97,115,110,213,80,220,142,102,102,36,152,90,135,
105,176,173,49,6,197,48,140,176,122,4,53,83,216,212,202,170,180,214,53,161,
225,129,185,106,22,12,190,97,158,170,92,160,194,134,169,98,246,128,195,24,
198,165,156,77,126,113,136,58,156,196,136,41,246,164,84,138,171,184,42,214,
203,128,89,39,198,85,140,148,149,36,215,78,170,234,131,124,152,239,154,214,
130,194,49,3,69,248,120,179,101,163,131,124,184,148,213,118,213,81,177,149,
58,213,33,201,63,10,195,215,190,7,86,245,128,9,8,40,102,51,125,94,92,5,159,
75,253,158,40,4,6,178,241,92,124,73,248,1,157,61,50,86,136,113,22,16,171,209,
230,144,240,14,188,2,167,22,88,57,50,86,171,73,114,175,34,226,245,57,180,111,
220,186,170,242,141,229,49,158,30,82,161,49,124,65,139,24,95,14,133,65,238,
116,180,190,49,130,30,30,59,93,173,139,19,187,2,163,102,26,255,23,239,196,19,
6,162 };

enum modes { SLEEP, ON_ALWAYS, M_END };
enum styles { ALL, FLARE1, FLARE2, RANDOM1, RANDOM2, S_END };

volatile static enum modes Mode = SLEEP;
volatile static enum styles Style = ALL;

inline void PWM_Enable()
{
	// Fast PWM mode, Top = 0xFF, Prescaler 64
	PWM1 = 255;
	PWM2 = 255;
	TCCR0A = _BV(COM0A1) | _BV(COM0A0) | _BV(COM0B1) | _BV(COM0B0) | _BV(WGM01) | _BV(WGM00);
	TCCR0B = _BV(CS01) | _BV(CS00);;
}

inline void PWM_Disable()
{
	TCCR0A = 0;
}

inline void Init()
{
	DDRB  = 0b11100111; // PB0-PB2 - Output, PB3-PB4 - Input
	PORTB = 0b11111000;

	PWM_Enable();

	// Enable external interrupts for PCINT3-PCINT4
	GIMSK = _BV(PCIE);
	PCMSK = _BV(PCINT4) | _BV(PCINT3);

	// Enable watchdog timer, interrupt mode, time-out 8 sec
	WDTCR = _BV(WDTIE) | _BV(WDP3) | _BV(WDP0);

	set_sleep_mode(SLEEP_MODE_PWR_DOWN);

	sei(); // Enable interrupts
}

/*inline uint16_t Get_Light(void)
{
	uint8_t i;
	uint16_t val = 0;

	// Measure PB2 using internal ref
	ADMUX  = _BV(REFS0) | ADC1;

	// Enable ADC, prescaler 128
	ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);

	// Let red LED generate some voltage
	PWM_Disable();
	BIT_CLR(DDRB, RLED);
	BIT_CLR(PORTB, RLED);
	_delay_ms(10);

	// Warm up the ADC, discard the first conversion
	BIT_SET(ADCSRA, ADSC);
	while (ADCSRA & _BV(ADSC));

	// Calculate average value for 16 samples
	for (i = 0; i < 16; i++)
	{
		_delay_ms(10);
		BIT_SET(ADCSRA, ADSC);
		while (ADCSRA & _BV(ADSC));
		val += ADC;
	}
	val >>= 4;

	ADCSRA = 0;          // Disable ADC
	BIT_SET(DDRB, RLED); // Re-enable red LED

	return val;
}*/

ISR(PCINT0_vect)
{
	_delay_ms(100); // Eliminate contact bounce

	PWM_Enable();
	BIT_SET(PORTB, RLED);

	if (BTN_PRESSED(BTN1))
	{
		Mode++;
		if (Mode >= M_END) Mode = SLEEP;
	}

	if (BTN_PRESSED(BTN2))
	{
		Style++;
		if (Style >= S_END) Style = ALL;
	}
}

ISR(WDT_vect)
{
	//Light = Get_Light();
}

int main()
{
	int8_t dx1 = -1;
	int8_t dx2 = 1;
	uint8_t k1 = 255;
	uint8_t k2 = 63;
	uint8_t k3 = 255;

	Init();

	while (1)
	{
		BIT_SET(PORTB, RLED);

		if (Mode == SLEEP)
		{
			PWM_Disable();
			BIT_CLR(PORTB, LED1);
			BIT_CLR(PORTB, LED2);
			BIT_CLR(PORTB, RLED);

			sleep_mode();
		}
		else
		{
			if (Style == ALL)
			{
				PWM_Disable();
				BIT_SET(PORTB, LED1);
				BIT_SET(PORTB, LED2);

				sleep_mode();
			}
			else if (Style == FLARE1)
			{
				if (dx1 == -1)
				{
					if (k1 > 0)
					{
						PWM1 = k1;
						PWM2 = k1--;
					}
					else
						dx1 = 1;
				}
				else
				{
					if (k1 < 255)
					{
						PWM1 = k1;
						PWM2 = k1++;
					}
					else
						dx1 = -1;
				}
			}
			else if (Style == FLARE2)
			{
				PWM1 = 255 - eeprom_read_byte(flare_vals + k2);
				PWM2 = 255 - eeprom_read_byte(flare_vals + 255 - k2);

				if (dx2 == 1)
				{
					if (k2 > 0) k2--;
					else dx2 = -1;
				}
				else
				{
					if (k2 < 62) k2++;
					else dx2 = 1;
				}
			}
			else if ((Style == RANDOM1) || (Style == RANDOM2))
			{
				PWM1 = pgm_read_byte_near(random_vals + 255 - k3);
				PWM2 = pgm_read_byte_near(random_vals + k3++);

				if (Style == RANDOM1)
					_delay_ms(500);
				else
					_delay_ms(100);
			}
		}

		_delay_ms(20);
	}
}
