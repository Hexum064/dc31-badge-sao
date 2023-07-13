/*
 * attiny1616-sao.c
 *
 * Created: 2023-07-09 18:45:37
 * Author : Branden
 */ 

//GAME NOTE/COLOR ORDER FOR LEDS 0 to 3: Blue=Bb3, Yellow=C#4, Red=F4, Green=Bb4

#define F_CPU 20000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "sound_data.h"
#include <stdlib.h>
#include <avr/eeprom.h>

#define EEPROM_SET 0xA7 //just some random value
#define LED_COUNT 12

#define SONG_DISP_PER 25 //25 * 10ms for 250ms or 4 fps
#define STANDBY_DISP_PER 5
#define GAME_STANDBY_DISP_PER 50
#define GAME_WON_DISP_PER 15
#define GAME_LOST_DISP_PER 15
#define GAME_SHOW_PATTERN_PER 35

#define DEBOUNCE_PER 2 //20ms

#define BUTTON_0 PIN0_bm
#define BUTTON_1 PIN1_bm
#define BUTTON_2 PIN2_bm
#define BUTTON_3 PIN3_bm
#define BUTTON_SONG PIN5_bm

#define GAME_LENGTH_MIN 5
#define GAME_MAX_WAIT 500 //5 seconds
#define GAME_PLAY_NOTE_LEN 0x20 //1/4 notes	

typedef struct Song
{
uint8_t reset_index;
uint8_t note_count;
uint8_t note_index;
uint8_t ext_index;
uint8_t ext_pos;
uint8_t * data;
uint8_t * ext;
		
} Song;

typedef struct Pixel
{
	uint8_t g;
	uint8_t r;
	uint8_t b;	
} Pixel;

Pixel leds[LED_COUNT];

#define RED_c 0
#define ORANGE_c 1
#define YELLOW_c 2
#define GREEN_c 3
#define CYAN_c 4
#define	BLUE_c 5
#define PINK_c 6
#define VIOLET_c 7

typedef enum  
{
	InitStandbyMode,
	StandbyMode,
	InitSongMode,
	SongMode,
	InitGameMode,
	GameStandbyMode,
	StartGame,
	GameShowPatternMode,
	GamePlayerMode,
	GameWonMode,
	GameLostMode,
} SaoStates;

Pixel colors[] = 
{
	{.g = 0,	.r = 80,	.b = 0 },
	{.g = 32,	.r = 24,	.b = 0 },
	{.g = 48,	.r = 24,	.b = 0 },
	{.g = 80,	.r = 0,		.b = 0 },
	{.g = 32,	.r = 0,		.b = 32 },
	{.g = 0,	.r = 0,		.b = 80 },
	{.g = 0,	.r = 32,	.b = 32 },
	{.g = 0,	.r = 64,	.b = 48 }
};

Pixel off = {.r = 0, .g = 0, .b = 0 };
Pixel white = {.r = 32, .g = 32, .b = 32 };

uint8_t button_color_index[] = {BLUE_c, YELLOW_c, RED_c, GREEN_c};
			
extern void output_pixels(uint16_t port, uint8_t pin, Pixel * pixels, uint16_t size);

SaoStates sao_mode = StandbyMode;

Song song_data[2];
volatile uint8_t ch0_beats = 0;
volatile uint8_t ch0_note = 0;
volatile uint8_t ch1_beats = 0;
volatile uint8_t ch1_note = 0;



volatile uint8_t disp_count = 0;
volatile uint8_t disp_temp = 0;
volatile uint8_t disp_temp2 = 0;

volatile uint8_t insert_button_pressed = 0;
volatile uint8_t button_down = 0;
volatile uint8_t debounce_count = 0;
volatile uint8_t button_held = 0;
volatile uint8_t button_action = 0;

uint8_t * game_sequence = 0;
volatile uint8_t game_step = 0;
volatile uint8_t game_step_index = 0;
volatile uint8_t game_note_count = 0;
volatile uint8_t game_note_index = 0;
volatile uint8_t game_temp_flag = 0;
volatile uint16_t game_timer = 0;
volatile uint8_t game_length = GAME_LENGTH_MIN;

void clk_init()
{
	CPU_CCP = CCP_IOREG_gc;
	CLKCTRL_MCLKCTRLA = CLKCTRL_CLKSEL_OSC20M_gc;
	
	while(!(CLKCTRL_MCLKSTATUS & CLKCTRL_OSC20MS_bm)){};

	CPU_CCP = CCP_IOREG_gc;
	CLKCTRL_MCLKCTRLB = 0;	
}



void io_init()
{
	
	//simon input pins
	PORTC.DIRCLR = PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm;
	PORTC.PIN0CTRL = PORT_PULLUPEN_bm;
	PORTC.PIN1CTRL = PORT_PULLUPEN_bm;
	PORTC.PIN2CTRL = PORT_PULLUPEN_bm;
	PORTC.PIN3CTRL = PORT_PULLUPEN_bm;
	
	//nyan and mode pins
	PORTB.DIRCLR = PIN4_bm | PIN5_bm;
	PORTB.PIN4CTRL = PORT_PULLUPEN_bm;
	PORTB.PIN5CTRL = PORT_PULLUPEN_bm;
	
	//LEDs out
	PORTA.DIRSET = PIN1_bm;
	
	//Sound out
	PORTA.DIRSET = PIN5_bm; //Low notes
	PORTB.DIRSET = PIN0_bm; //High notes
	

}

void timers_init()
{

	//High note timer : Ch0
	//TCA0.SINGLE.CMP0 = 299 * 2;
	TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_FRQ_gc;// | TCA_SINGLE_CMP0EN_bm;
	TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV64_gc | TCA_SINGLE_ENABLE_bm;
	
	//Low note timer : Ch1
	//TCD0.CMPBCLR = 299 * 2; 
	//TCD0.CMPBSET = 299;
	TCD0.CTRLB = 0;
	CPU_CCP = CCP_IOREG_gc;
	TCD0.FAULTCTRL = TCD_CMPBEN_bm;
	TCD0.CTRLA = TCD_CLKSEL_20MHZ_gc | TCD_CNTPRES_DIV32_gc | TCD_SYNCPRES_DIV4_gc;// | TCD_ENABLE_bm;
		

		
	//TCB Timer clock based off of TCA clock. Assumed to be 20mHz / 64
	//This means TCA must always be enabled.
		
	//BPM timer
// 	TCB0.CCMP = BPM_TIMER_PER;
	TCB0.CTRLB = 0;
	TCB0.INTCTRL = TCB_CAPT_bm;
	TCB0.CTRLA = TCB_CLKSEL_CLKTCA_gc;// | TCB_ENABLE_bm;
	
	//System event timer. 10ms;
	TCB1.CCMP = 3100;
	TCB1.CTRLB = 0;
	TCB1.INTCTRL = TCB_CAPT_bm;
	TCB1.CTRLA = TCB_CLKSEL_CLKTCA_gc | TCB_ENABLE_bm;	
		
}

void load_game_length()
{
	uint8_t eeprom_set = eeprom_read_byte((uint8_t *)1);
	game_length = GAME_LENGTH_MIN;
	if (eeprom_set == EEPROM_SET)
	{
		game_length = eeprom_read_byte((uint8_t *)2);
	}	
}

void update_game_length()
{
	game_length++;
	
	if (game_length < GAME_LENGTH_MIN)
	{
		game_length = GAME_LENGTH_MIN;
	}
	
	eeprom_write_byte((uint8_t *)2, game_length);
	eeprom_write_byte((uint8_t *)1, EEPROM_SET);
}

void bmp_timer_enable(uint16_t bpm_per)
{
	TCB0.CCMP = bpm_per;
	TCB0.CTRLA |= TCB_ENABLE_bm;
}

void bpm_timer_disable()
{
	TCB0.CTRLA &= ~TCB_ENABLE_bm;
}

void play_note(uint8_t note, uint8_t beats_16th, uint8_t channel)
{	
	//automatically update the lenght of the beats
	beats_16th <<= 2;
	if (channel)
	{
		ch1_beats = beats_16th;
		ch1_note = note;
	}
	else
	{
		ch0_beats = beats_16th;
		ch0_note = note;
	}
}


//only pointing at nyan for now
void load_next_note(uint8_t channel)
{
	uint8_t index = song_data[channel].note_index;
	uint8_t note =  pgm_read_byte(song_data[channel].data + index);
	uint8_t ext = pgm_read_byte(song_data[channel].ext + song_data[channel].ext_index);
	uint8_t beats = 1 << (note >> 6);
	
	if ((ext >> song_data[channel].ext_pos))
	{
		beats = 16;
	}
	


	play_note(note & 0x3F, beats, channel);
	
	song_data[channel].note_index++;
	
	if (song_data[channel].note_index >= song_data[channel].note_count)
	{
		song_data[channel].note_index = song_data[channel].reset_index;
		song_data[channel].ext_index = song_data[channel].reset_index >> 3; 
		song_data[channel].ext_pos = song_data[channel].reset_index % 8 ;
	}
	
	song_data[channel].ext_pos++;
	
	if (song_data[channel].ext_pos > 7)
	{
		song_data[channel].ext_pos = 0;
		song_data[channel].ext_index++;
	}
	
	
}

void stop_song()
{
	bpm_timer_disable();
	TCA0.SINGLE.CTRLB &= ~TCA_SINGLE_CMP0EN_bm;
	TCD0.CTRLA &= ~TCD_ENABLE_bm;
	ch0_note = 0;
	ch0_beats = 0;
	ch1_note = 0;
	ch1_beats = 0;
}

void update_display()
{
	output_pixels((uint16_t)(&PORTA), PIN1_bm, leds, sizeof(leds));
}

void update_standby_display()
{
	uint8_t led = disp_temp % LED_COUNT;
	uint8_t col = 0;
	

	
	col = disp_temp2 % 8;
	
	for (uint8_t i = 0; i < LED_COUNT; i++)
	{
		leds[i]	= off;
		if (i == led)
		{
			leds[i] = colors[col];
		}
	}
	disp_temp++;
	if (disp_temp >= LED_COUNT)
	{
		disp_temp = 0;
		disp_temp2++;
	}
	update_display();
}


void load_standby()
{
	disp_count = 0;
	disp_temp = 0;
	disp_temp2 = 0;
	sao_mode = StandbyMode;
	update_standby_display();
}



void clear_pixels()
{
	for (uint8_t i = 0; i < LED_COUNT; i++)
	{
		leds[i] = off;
	}

}

void update_song_display()
{
	if (disp_temp % 2)
	{
		leds[0] = colors[PINK_c];
		leds[1] = colors[PINK_c];
		leds[2] = off;
		leds[3] = off;

	}
	else
	{
		leds[3] = colors[PINK_c];
		leds[2] = colors[PINK_c];
		leds[1] = off;
		leds[0] = off;
		
	}
	
	for (uint8_t i = 0; i < 8; i++)
	{
		leds[i + 4] = colors[(i + disp_temp) % 8];
	}
	
	disp_temp++;
	update_display();
}


void load_nyan()
{
	disp_count = 0;
		
	song_data[0] =
	(Song) {
		.data = nyanTreble,
		.ext = nyanExtTreble,
		.note_count = sizeof(nyanTreble),
		.note_index = 0,
		.ext_index = 0,
		.ext_pos = 0,
		.reset_index = NYAN_TREBLE_RESET,
	};
	
	song_data[1] =
	(Song) {
		.data = nyanBass,
		.ext = nyanExtBass,
		.note_count = sizeof(nyanBass),
		.note_index = 0,
		.ext_index = 0,
		.ext_pos = 0,
		.reset_index = NYAN_BASS_RESET,
	};
	
	load_next_note(0);
	load_next_note(1);
	bmp_timer_enable(NYAN_BPM_TIMER_PER);
	update_song_display();
	sao_mode = SongMode;
}

void update_game_standby_display()
{
	uint8_t i = 0;
	uint8_t mod = disp_temp % 4;
	
	for (i = 0; i < LED_COUNT; i++)
	{
		leds[i]	 = off;
		if (i == mod)
		{
			leds[i] = colors[button_color_index[i]];

		}
	}
	

	
	update_display();
	disp_temp++;
}

unsigned int get_seed()
{
	PORTA.DIRCLR = PIN7_bm;
	ADC0.CTRLB = 0; //medium current consumption, maximum sampling speed 150ksps, resolution 12-bit right adjusted, signed mode
	ADC0.CTRLC = ADC_REFSEL_VDDREF_gc;
	ADC0.CTRLD = 0;
	ADC0.CTRLE = 0;
	ADC0.MUXPOS = ADC_MUXPOS_AIN7_gc;	
	ADC0.EVCTRL=0x00;               //no event channel input is selected
	
	ADC0.CTRLA= ADC_RESSEL_8BIT_gc | ADC_FREERUN_bm | ADC_ENABLE_bm; //enable ADC
	ADC0.COMMAND = ADC_STCONV_bm; //start the conversion
	while(!(ADC0.INTFLAGS & ADC_RESRDY_bm));

	
	unsigned int result = ADC0.RES;
	
	ADC0.CTRLA = 0;
	return result;
}

void load_game()
{
	disp_count = 0;
	game_step = 0;
	disp_temp = 0;
	game_step_index = 0;
	free(game_sequence);
	game_sequence = (uint8_t *)malloc(game_length);
// 	uint16_t seed = get_seed();
	srand(TCA0.SINGLE.CNT); //TCA is always running
	for (uint8_t i = 0; i < game_length; i++)
	{
		game_sequence[i] = 0x01 << (rand() % 4);
	}
	update_game_standby_display();
	sao_mode = GameStandbyMode;
}

void update_game_start_display()
{

	uint8_t i = 0;
	uint8_t len = game_length;
// 	for (; i < 4; i++)
// 	{
// 		leds[i] = colors[button_color_index[i]];
// 	}
	
	for (; i < LED_COUNT; i++)
	{
		leds[i] = off;
		if (len & 0x01)
		{
			leds[i] = white;
		}
		len >>= 1;
		
	}
	update_display();
}

void play_game_notes(uint8_t * notes, uint8_t size, SaoStates nextMode)
{
	if (!game_temp_flag)
	{
		game_temp_flag = 1;
		ch0_note = 0;
		ch0_beats = 0;
		bmp_timer_enable(GAME_BPM_TIMER_PER);
		game_note_count = size;
		game_note_index = 0;
		
	}
	
	if (ch0_beats == 0)
	{		
		
		play_note(notes[game_note_index] & 0x3F, 1 << ((notes[game_note_index] & 0xC0) >> 6), 0);
		game_note_index++;
	}
	
	if (game_note_index == game_note_count)
	{
		game_temp_flag = 0;
		stop_song();
		sao_mode = nextMode;
		disp_count = 0;
	}
}

void activate_button_led(uint8_t led)
{
	PORTA.OUTTGL = PIN2_bm;
	for(uint8_t i = 0; i < LED_COUNT; i++)
	{
		leds[i] = off;
	}
	leds[led] = colors[button_color_index[led]];
	
	update_display();
}

uint8_t get_button_number(uint8_t button)
{
	uint8_t i = 0;
	
	for(; i < 8; i++)
	{
		if ((1 << i) == button)
		{
			break;
		}
	}
	
	return i;
}

void show_game_pattern() 
{
	uint8_t button_num;
	if (game_temp_flag)
	{
		//Already played the pattern item, so clear and pause
		game_temp_flag = 0;
		clear_pixels();
		update_display();
		stop_song();
		
		if (game_step_index > game_step)
		{
			sao_mode = GamePlayerMode;
			game_temp_flag = 0;
			game_step_index = 0;
			game_timer = 0;
			return; //Important so game_step_index does not get incremented again
		}
	}
	else
	{
		game_temp_flag = 1;
		button_num = get_button_number(game_sequence[game_step_index]);
		play_note(gameButtonNotes[button_num] & 0x3f, 8, 0);
		activate_button_led(button_num);
		bmp_timer_enable(GAME_BPM_TIMER_PER);
		game_step_index++;
	}
	

	
}

void handle_game_input()
{
	//First check if the button is correct
	uint8_t button_num;
	if (button_held & (BUTTON_0 | BUTTON_1 | BUTTON_2 | BUTTON_3))
	{
		game_timer = 0;	
		
		if (button_held != game_sequence[game_step_index])
		{
			sao_mode = GameLostMode;
			return; //Important to return immediately 
		}
		button_num = get_button_number(button_held);
		play_note(gameButtonNotes[button_num], 1, 0);
		if (button_held != game_temp_flag)
		{
			activate_button_led(button_num);	
		}
		game_temp_flag = button_held;
		bmp_timer_enable(GAME_BPM_TIMER_PER);
	}
	else
	{
		

		
		if (game_temp_flag)
		{
			
			
			//we will only get here if the button that was being pressed was correct
			stop_song();	
			clear_pixels();
			update_display();		
			game_timer = 0;	
			game_step_index++;	
			
			if (game_step_index > game_step)
			{
				game_step++;
				
				if (game_step == game_length)
				{
					update_game_length();
					sao_mode = GameWonMode;
				}
				else
				{
					game_step_index = 0;
					sao_mode = GameShowPatternMode;	
				}
				
			}	
			
			
				
		}			

		game_temp_flag = 0;
		
		if (game_timer >= GAME_MAX_WAIT)
		{
			sao_mode = GameLostMode;
			return;
		}		
	}
	
	disp_count = 0;
}

void update_game_won_display()
{
	uint8_t i = 0;
	
	if (disp_temp % 2)
	{
		clear_pixels();
	}
	else
	{
		for (;i < 4; i++)	
		{
			leds[i] = colors[button_color_index[i]];
		}
		
		for (;i < LED_COUNT; i++)
		{
			leds[i] = white;
		}
	}
	
	update_display();
	disp_temp++;
}


void update_game_lost_display()
{
	uint8_t i = 0;
	
	if (disp_temp % 2)
	{
		clear_pixels();
	}
	else
	{
		for (;i < LED_COUNT; i++)
		{
			leds[i] = colors[RED_c];
		}		
	}
	
	update_display();
	disp_temp++;
}

uint8_t get_button_pressed()
{
	uint8_t in = (PORTC.IN & (PIN0_bm | PIN1_bm | PIN2_bm | PIN3_bm)) | (PORTB.IN & PIN5_bm );

	//This should stop other button presses from interfering 
	//Have to negate "in" because buttons are active low
	if (button_held & ~in)
	{
		return button_held;
	}

	if (!(in & PIN0_bm))
	{
		return BUTTON_0;
	}

	if (!(in & PIN1_bm))
	{
		return BUTTON_1;
	}

	if (!(in & PIN2_bm))
	{
		return BUTTON_2;
	}

	if (!(in & PIN3_bm))
	{
		return BUTTON_3;
	}

	if (!(in & PIN5_bm))
	{
		return BUTTON_SONG;
	}	
	
	return 0;
}

void check_buttons()
{
	if (!(button_down))
	{
		button_down = get_button_pressed();
				
		if (button_held && !button_down) //button was released
		{
			button_action = button_held;
			button_held = 0;
		}							
	}
	else
	{		
		if (debounce_count >= DEBOUNCE_PER)
		{				
			if (button_down == get_button_pressed())
			{					
				button_held = button_down;			
			}

			button_down = 0;
		}
	}
}



void run_state()
{
	switch (sao_mode)
	{
		case InitStandbyMode:
			load_standby();
			break;
		case StandbyMode:
			if (!insert_button_pressed)
			{
				sao_mode = InitGameMode;
				break;
			}
			
			if (button_action == BUTTON_SONG)
			{
				sao_mode = InitSongMode;				
				break;
			}
			
			if (disp_count == STANDBY_DISP_PER)
			{
				disp_count = 0;
				update_standby_display();
			}
			break;
		case InitSongMode:
			load_nyan();
			break;
		case SongMode:
		
			if (button_action == BUTTON_SONG)
			{
				if (insert_button_pressed)
				{
					sao_mode = InitStandbyMode;
				}
				else
				{
					sao_mode = InitGameMode;
				}				
				break;
			}
		
			if (disp_count == SONG_DISP_PER)
			{
				disp_count = 0;
				update_song_display();
			}
			break;
		case InitGameMode:
			load_game();
			break;
		case GameStandbyMode:
		
			if (insert_button_pressed)
			{
				sao_mode = InitStandbyMode;
				break;
			}
		
			if (button_action == BUTTON_SONG)
			{
				sao_mode = InitSongMode;				
				break;
			}
			
			if (button_action & (BUTTON_0 | BUTTON_1 | BUTTON_2 | BUTTON_3))
			{
				sao_mode = StartGame;				
				break;
			}
		
			if (disp_count == GAME_STANDBY_DISP_PER)
			{
				disp_count = 0;
				update_game_standby_display();
			}
			break;
		case StartGame:
			
			if (insert_button_pressed)
			{
				sao_mode = InitStandbyMode;
				break;
			}

			if (!game_temp_flag)
			{
				
				update_game_start_display();
			}
									
			play_game_notes(gameStartupNotes, GAME_START_SIZE, GameShowPatternMode);
			break;
		case GameShowPatternMode:
		
			if (insert_button_pressed)
			{
				sao_mode = InitStandbyMode;
				break;
			}
					
			if (button_action == BUTTON_SONG)
			{
				sao_mode = InitSongMode;				
				break;
			}
			
			if (disp_count == GAME_SHOW_PATTERN_PER)
			{
				disp_count = 0;
				show_game_pattern();
			}
						
			break;
		case GamePlayerMode:
		
			if (insert_button_pressed)
			{
				sao_mode = InitStandbyMode;
				break;
			}
					
			if (button_action == BUTTON_SONG)
			{
				sao_mode = InitSongMode;				
				break;
			}
			
			handle_game_input();
			
			break;
		case GameWonMode:
			play_game_notes(gameWonNotes, GAME_WON_SIZE, InitGameMode);
			if (disp_count == GAME_WON_DISP_PER)
			{
				disp_count = 0;
				update_game_won_display();
			}			
			break;
		case GameLostMode:
			play_game_notes(gameLostNotes, GAME_LOST_SIZE, InitGameMode);
			if (disp_count == GAME_LOST_DISP_PER)
			{
				disp_count = 0;
				update_game_lost_display();
			}					
			break;
		
	}
	
	if (insert_button_pressed || sao_mode == SongMode)
	{
		//This flag cannot be set if we are not in a game mode
		game_temp_flag = 0;
	}
	
	button_action = 0;
}

int main(void)
{
	cli();
	clk_init();
	io_init();
	

	//No Interrupt setup needed
	timers_init();
	
	//Debug LED pin
	PORTA.DIRSET = PIN2_bm;	
	sei();

	load_game_length();

	if (!(PORTB.IN & PIN4_bm)) //Force it for now
	{
		insert_button_pressed = 1;
		load_standby();	
	}
	else
	{
		insert_button_pressed = 0;
		load_game();
	}
	
	
    while (1) 
    {		
		check_buttons();
		run_state();
	}
}




//BPM Timer
ISR(TCB0_INT_vect)
{

	if (ch0_beats > 0)
	{
 		if (!(TCA0.SINGLE.CTRLB & TCA_SINGLE_CMP0EN_bm) && ch0_note)
 		{
// 			
// 			//PER for TCA0 needs to be multiplied by 2
// 			//Also need to reset the count to have the next not play correctly
 			TCA0.SINGLE.CNT = 0;
			TCA0.SINGLE.CMP0 = noteClocks[ch0_note - 1];
 			TCA0.SINGLE.CTRLB |= TCA_SINGLE_CMP0EN_bm;
 		}
		ch0_beats--;
		if (ch0_beats == 0)
		{
		
			TCA0.SINGLE.CTRLB &= ~TCA_SINGLE_CMP0EN_bm;
			//Adding a small delay punctuates identical notes in sequence
			_delay_ms(10);
			if (sao_mode == SongMode)
			{
				
				//Loading the next note here holds the timing better between channels				
				
				load_next_note(0);			
			}
		}
		
	}
	else
	{
		//"Turn off" the timer by disabling the output
		TCA0.SINGLE.CTRLB &= ~TCA_SINGLE_CMP0EN_bm;			
	}
	
	if (ch1_beats > 0)
	{
 		if (!(TCD0.CTRLA & TCD_ENABLE_bm) && ch1_note)
 		{

// 			//PER for TCD0 starts at half way mark and ends at double the frequency			
 			TCD0.CMPBCLR = noteClocks[ch1_note - 1];
 			TCD0.CMPBSET = noteClocks[ch1_note - 1] >> 1;			
 			TCD0.CTRLA |= TCD_ENABLE_bm;
 		}
		ch1_beats--;
		if (ch1_beats == 0)
		{
			TCD0.CTRLA &= ~TCD_ENABLE_bm;
			//Adding a small delay punctuates identical notes in sequence
			_delay_ms(10);			
			if (sao_mode == SongMode)
			{				
				//Loading the next note here holds the timing better between channels
				load_next_note(1);		
			}
		}
	}
	else
	{		
 		TCD0.CTRLA &= ~TCD_ENABLE_bm;
	}	
	
	TCB0.INTFLAGS = TCB_CAPT_bm;
}

//Sys Timer
ISR(TCB1_INT_vect)
{
	disp_count++;
	game_timer++;
	
	if (button_down)
	{
		debounce_count++;
	}
	else
	{
		debounce_count = 0;
	}
	
	//Setting the button here should cause less bounce issues without actually debouncing
	insert_button_pressed = !(PORTB.IN & PIN4_bm);
 	
	TCB1.INTFLAGS = TCB_CAPT_bm;
}

