/*
 * song_player.h
 *
 * Created: 2023-04-28 23:01:49
 *  Author: Branden
 */ 


#ifndef SONG_PLAYER_H_
#define SONG_PLAYER_H_
#include <stdint-gcc.h>


typedef struct songTrack
{
	uint8_t * track_data;
	uint16_t track_size;
	uint8_t * extension_data;	
	uint16_t extension_size;
} Track;

typedef struct songChannel
{
	Track intro;
	Track main;	
} Channel;

typedef struct songInitParams
{	
	Channel ch0;
	Channel ch1;	
	uint16_t bmp_period;
} SongInitParams;

void song_player_init(SongInitParams initParams, void (*update_display_cb)());
void song_start();
void song_interrupt_handler();

#endif /* SONG_PLAYER_H_ */