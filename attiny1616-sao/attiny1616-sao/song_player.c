/*
 * song_player.c
 *
 * Created: 2023-04-28 23:01:34
 *  Author: Branden
 */ 

#ifndef F_CPU
# define F_CPU 32000000UL
#endif

#include "song_player.h"
#include <avr/io.h>
#include <stdlib.h>
#include <util/delay.h>
#include "sound_data.h"

uint8_t hasCh0Intro = 0;
uint8_t hasCh0Main = 0;
uint8_t * ch0IntroTrack;
uint8_t * ch0IntroExt;
uint8_t * ch0MainTrack;
uint8_t * ch0MainExt;
uint16_t noteCh0Index = 0;
uint16_t noteCh0ExtIndex = 0;
uint8_t noteCh0ExtBitPos = 0;
uint8_t noteCh0BeatCount = 0;
uint16_t ch0IntroNoteCount = 0;
uint16_t ch0MainNoteCount = 0;
uint16_t ch0NoteCount = 0;
uint8_t * ch0Track;
uint8_t * ch0Ext;
uint8_t isCh0Intro = 0;


uint8_t hasCh1Intro = 0;
uint8_t hasCh1Main = 0;
uint8_t * ch1IntroTrack;
uint8_t * ch1IntroExt;
uint8_t * ch1MainTrack;
uint8_t * ch1MainExt;
uint16_t noteCh1Index = 0;
uint16_t noteCh1ExtIndex = 0;
uint8_t noteCh1ExtBitPos = 0;
uint8_t noteCh1BeatCount = 0;
uint16_t ch1IntroNoteCount = 0;
uint16_t ch1MainNoteCount = 0;
uint16_t ch1NoteCount = 0;
uint8_t * ch1Track;
uint8_t * ch1Ext;
uint8_t isCh1Intro = 0;

void (*update_display_callback_ptr)();


void beat_timer_C4_init(uint16_t per)
{
	//for 136 BMP and 1/16 note support, 64 prescl and 13787 clk
// 	TCC4.CTRLB = TC_BYTEM_NORMAL_gc | TC_CIRCEN_DISABLE_gc | TC_WGMODE_NORMAL_gc;
// 	TCC4.CTRLE = 0;
// 	TCC4.INTCTRLA = TC_OVFINTLVL_HI_gc;
// 	TCC4.CTRLA = 0;
// 	TCC4.PER = per;
//  	
// 	TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_NORMAL_gc;
// 	TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
// 	TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV256_gc | TCA_SINGLE_ENABLE_bm;
// 	TCA0.SINGLE.PER = per;
	
	TCD0.CMPBCLR = per;
	TCD0.CTRLB = TCD_WGMODE0_bm;
	TCD0.INTCTRL = TCD_OVF_bm;
	TCD0.CTRLA = TCD_CLKSEL_20MHZ_gc | TCD_CNTPRES_DIV32_gc | TCD_ENABLE_bm;
	
}

//Channel 0 Counter
void note_0_timer_C5_init()
{
// 
// 	TCC5.CTRLB = TC_BYTEM_NORMAL_gc | TC_CIRCEN_DISABLE_gc | TC_WGMODE_FRQ_gc;
// 	TCC5.CTRLE = TC_CCAMODE_COMP_gc; 
// 	TCC5.CTRLA = TC_CLKSEL_DIV8_gc;
}

//Channel 1 Counter
void note_1_timer_D5_init()
{
// 	
// 	TCD5.CTRLB = TC_BYTEM_NORMAL_gc | TC_CIRCEN_DISABLE_gc | TC_WGMODE_FRQ_gc;
// 	TCD5.CTRLE = TC_CCAMODE_COMP_gc;
// 	TCD5.CTRLA = TC_CLKSEL_DIV8_gc;
}

//Returns the clock count of the note, sets the beat counter for the note, and updates the indexes
uint16_t set_note_and_beat(uint8_t * noteBeatCountPtr, uint8_t * notesPtr, uint8_t * noteExtPtr, uint16_t * noteIndexPtr, uint16_t * noteExtIndexPtr, uint8_t * extBitPosPtr)
{
	*noteBeatCountPtr = 0x01;
	uint8_t noteVal = notesPtr[*noteIndexPtr];
	uint8_t beats = *noteBeatCountPtr;
	beats <<= (noteVal >> 6);
	beats -= 1; //This will turn 0b0010 into 0b0001 or 0b0100 into 0b0011. A cheap way of doing a power of 2
	
	//0b00000001 << 3 = 0b00001000. 0b00001000 - 1 = 0b00000111
	//when we count down the number of 1/16 beats the note will be played for it will always play 1 1/16,
	//then decrement the beat counter until it reaches 0. So, a beat of 7 will play 8 beats, or a half note
	uint8_t extByte = noteExtPtr[*noteExtIndexPtr];
	if ((extByte >> (*extBitPosPtr)) & 0x01) //if the extension bit is set then this is a whole note
	{
		beats = 0x0F;
	}
	*noteBeatCountPtr = beats;
	//Get the counts from the note as an index
	uint16_t index = noteVal & 0x3F;
	uint16_t count = 0;
	if (index > 0)
	{
		count = noteClocks[index] / 2;
	}
	
	
	//Increment the note index and the bit pos of the ext
	(*noteIndexPtr)++;
	(*extBitPosPtr)++;
	
	//If we have maxed out the bit pos, move to the next ext byte
	if (*extBitPosPtr == 8)
	{
		*extBitPosPtr = 0;
		(*noteExtIndexPtr)++;
	}
	
	return count;
	
}


//Uses the current note indexes for treble
void set_ch0_note_and_beat()
{
	//The note number is the first 6 bits of the byte for that note and represents the index into the note clocks
// 	TCC5.CCA = set_note_and_beat(&noteCh0BeatCount, ch0Track, ch0Ext, &noteCh0Index, &noteCh0ExtIndex, &noteCh0ExtBitPos);
}

//Uses the current note indexes for Bass
void set_ch1_note_and_beat()
{
	//The note number is the first 6 bits of the byte for that note and represents the index into the note clocks
// 	TCD5.CCA = set_note_and_beat(&noteCh1BeatCount, ch1Track, ch1Ext, &noteCh1Index, &noteCh1ExtIndex, &noteCh1ExtBitPos);
}

void song_init()
{
	if (hasCh0Intro)
	{
		isCh0Intro = 1;
		ch0Track = ch0IntroTrack;
		ch0Ext = ch0IntroExt;
	}

	if (!(hasCh0Intro) && hasCh0Main)
	{
		isCh0Intro = 0;
		ch0Track = ch0MainTrack;
		ch0Ext = ch0MainExt;
	}

	noteCh0BeatCount = 0;
	noteCh0Index = 0;
	noteCh0ExtIndex = 0;
	noteCh0ExtBitPos = 0;
	
	if (hasCh1Intro)
	{
		isCh1Intro = 1;
		ch1Track = ch1IntroTrack;
		ch1Ext = ch1IntroExt;
	}

	if (!(hasCh1Intro) && hasCh1Main)
	{
		isCh1Intro = 0;
		ch1Track = ch1MainTrack;
		ch1Ext = ch1MainExt;
	}

	noteCh1BeatCount = 0;
	noteCh1Index = 0;
	noteCh1ExtIndex = 0;
	noteCh1ExtBitPos = 0;
	
	if (hasCh0Intro || hasCh0Main)
	{
		set_ch0_note_and_beat();
	}
	
	if (hasCh1Intro || hasCh1Main)
	{
		set_ch1_note_and_beat();
	}
	
}

void load_track_into_mem(uint8_t * source, uint8_t ** dest, uint16_t size)
{
	*dest = (uint8_t *)malloc(size);
	volatile uint8_t temp;
	uint16_t i = 0;
	for(;i<size;i++)
	{
		temp =pgm_read_byte(source + i);
		(*dest)[i] = temp;
	}
}

void song_player_init(SongInitParams initParams, void (*update_display_cb)())
{
	update_display_callback_ptr = update_display_cb;
	
	beat_timer_C4_init(initParams.bmp_period);
	note_0_timer_C5_init();
	note_1_timer_D5_init();	
	
	if (initParams.ch0.main.track_data)
	{
		hasCh0Main = 1;
// 		load_track_into_mem(initParams.ch0.main.track_data, &ch0MainTrack, initParams.ch0.main.track_size);
// 		load_track_into_mem(initParams.ch0.main.extension_data, &ch0MainExt, initParams.ch0.main.extension_size);
		ch0MainTrack = initParams.ch0.main.track_data;
		ch0MainExt = initParams.ch0.main.extension_data;
		ch0MainNoteCount = initParams.ch0.main.track_size;
		ch0NoteCount = ch0MainNoteCount;
	}	
	
	//Note: Checking the main track stuff first so that we can set values here that can be overwritten if intro track
	//information is present without the need for more conditionals	
	
	//First copy over data from progmem
	if (initParams.ch0.intro.track_data)
	{
		hasCh0Intro = 1;
// 		load_track_into_mem(initParams.ch0.intro.track_data, &ch0IntroTrack, initParams.ch0.intro.track_size);
// 		load_track_into_mem(initParams.ch0.intro.extension_data, &ch0IntroExt, initParams.ch0.intro.extension_size);
		ch0IntroTrack = initParams.ch0.intro.track_data;
		ch0IntroExt = initParams.ch0.intro.extension_data;
		ch0IntroNoteCount = initParams.ch0.intro.track_size;		
		ch0NoteCount = ch0IntroNoteCount;
	}
	
	if (initParams.ch1.main.track_data)
	{
		hasCh1Main = 1;
// 		load_track_into_mem(initParams.ch1.main.track_data, &ch1MainTrack, initParams.ch1.main.track_size);
// 		load_track_into_mem(initParams.ch1.main.extension_data, &ch1MainExt, initParams.ch1.main.extension_size);
		ch1MainTrack = initParams.ch1.main.track_data;
		ch1MainExt = initParams.ch1.main.extension_data;
		ch1MainNoteCount = initParams.ch1.main.track_size;
		ch1NoteCount = ch1MainNoteCount;
	}
	
	if (initParams.ch1.intro.track_data)
	{
		hasCh1Intro = 1;
// 		load_track_into_mem(initParams.ch1.intro.track_data, &ch1IntroTrack, initParams.ch1.intro.track_size);
// 		load_track_into_mem(initParams.ch1.intro.extension_data, &ch1IntroExt, initParams.ch1.intro.extension_size);
		ch1IntroTrack = initParams.ch1.intro.track_data;
		ch1IntroExt = initParams.ch1.intro.extension_data;
		ch1IntroNoteCount = initParams.ch1.intro.track_size;		
		ch1NoteCount = ch1IntroNoteCount;
	}
	
		
	
	song_init();
}

void song_start()
{
// 	TCC4.CTRLA = TC_CLKSEL_DIV256_gc;
}

// void song_play_once(void (*callback)())
// {
// 	play_once_callback_ptr = callback;
// 	song_start();	
// }

void song_interrupt_handler(){
// 	PORTA.OUTSET = PIN2_bm;
// 	TCC4.INTFLAGS = TC4_OVFIF_bm;

	if (!(noteCh0BeatCount))
	{

		
		//First turn off treble counter
// 		TCC5.CTRLA = 0;
// 		TCC5.CCA = 0;

		if (noteCh0Index >= ch0NoteCount)
		{
			if (isCh0Intro)// && hasCh0Main)
			{
				isCh0Intro = 0;
				ch0NoteCount = ch0MainNoteCount;
				ch0Track = ch0MainTrack;
				ch0Ext = ch0MainExt;
			}
// 			else if (play_once_callback_ptr)
// 			{
// 				TCC4.CTRLA = 0;
// 				play_once_callback_ptr();
// 				
// 			}
			
			noteCh0BeatCount = 0;
			noteCh0Index = 0;
			noteCh0ExtIndex = 0;
			noteCh0ExtBitPos = 0;
		}
		

		set_ch0_note_and_beat();
		
	}
	else
	{
		noteCh0BeatCount--;
	}
	
	if (!(noteCh1BeatCount))
	{
		//First turn off base counter
		
// 		TCD5.CTRLA = 0;
// 		TCD5.CCA = 0;

		if (noteCh1Index >= ch1NoteCount)
		{
			if (isCh1Intro && hasCh1Main)
			{
				isCh1Intro = 0;
				ch1NoteCount = ch1MainNoteCount;
				ch1Track = ch1MainTrack;
				ch1Ext = ch1MainExt;
			}
			
			noteCh1BeatCount = 0;
			noteCh1Index = 0;
			noteCh1ExtIndex = 0;
			noteCh1ExtBitPos = 0;
		}

		set_ch1_note_and_beat();
	}
	else
	{
		noteCh1BeatCount--;
	}
	
	update_display_callback_ptr();
	_delay_ms(20);
// 	TCC5.CTRLA = TC_CLKSEL_DIV8_gc;
// 	TCD5.CTRLA = TC_CLKSEL_DIV8_gc;
}


