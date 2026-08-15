' sound.bas -- sound effects, adapted from fujinet-fujitzee/intv/sound.bas
' (itself from fujinet-battleship/intv). The transport primitive (play_tone)
' and the UI-generic effects are kept verbatim; the game-specific effects
' follow the reference tuning in fujinet-fujirkle/src/msdos/sound.c (the
' cross-platform Hz table -- "retune by one constant multiplier or not at
' all"), converted to PSG divisors.
'
' All effects play on PSG channel 0 -- turn-based game, non-overlapping UI
' events, no need for polyphony (and SOUND's channel argument must be a
' compile-time constant anyway, which rules out a generic multi-channel
' helper).
'
' SOUND's frequency argument is a 12-bit PSG divisor, computed as
' round(3579545/32/hz) for NTSC. These are precomputed rather than divided
' at runtime: 3579545/32 alone is ~111861, which overflows IntyBASIC's
' 16-bit variables (max 65535), so it only works as a compile-time-folded
' literal division -- not as CONST-divided-by-a-runtime-variable. Every
' effect here uses a fixed, known-in-advance frequency, so precomputing
' sidesteps the overflow entirely.
'   50Hz->2237  60Hz->1864  70Hz->1598   80Hz->1398  100Hz->1119
'  300Hz->373  341Hz->328   344Hz->325  350Hz->320   352Hz->318
'  355Hz->315  386Hz->290   403Hz->278  416Hz->269   443Hz->253
'  467Hz->240  493Hz->227   508Hz->220  521Hz->215   591Hz->189
'  600Hz->187  633Hz->177   691Hz->162  700Hz->160
    CONST SND_VOL = 12

    DIM #snd_val, snd_gate, snd_post, snd_i

' ---------------------------------------------------------------------------
' play_tone: one square-wave note on channel 0. #snd_val = precomputed PSG
' divisor (see table above), snd_gate = frames the tone sounds, snd_post =
' frames of silence after.
' ---------------------------------------------------------------------------
play_tone: PROCEDURE
    SOUND 0, #snd_val, SND_VOL
    FOR snd_i = 1 TO snd_gate
        WAIT
    NEXT snd_i
    SOUND 0, 0, 0
    FOR snd_i = 1 TO snd_post
        WAIT
    NEXT snd_i
END

' sound_join: sit down at a table (soundJoinGame: 403,344,403 Hz).
sound_join: PROCEDURE
    #snd_val = 278 : snd_gate = 4 : snd_post = 1 : GOSUB play_tone
    #snd_val = 325 : snd_gate = 4 : snd_post = 1 : GOSUB play_tone
    #snd_val = 278 : snd_gate = 4 : snd_post = 0 : GOSUB play_tone
END

' sound_myturn: it's your turn to roll (soundMyTurn: double 403 Hz beep).
sound_myturn: PROCEDURE
    #snd_val = 278 : snd_gate = 4 : snd_post = 3 : GOSUB play_tone
    #snd_val = 278 : snd_gate = 8 : snd_post = 0 : GOSUB play_tone
END

' sound_gamedone: game over fanfare (soundGameDone: 341,467,521,591 Hz).
sound_gamedone: PROCEDURE
    #snd_val = 328 : snd_gate = 8 : snd_post = 0 : GOSUB play_tone
    #snd_val = 240 : snd_gate = 12 : snd_post = 8 : GOSUB play_tone
    #snd_val = 215 : snd_gate = 12 : snd_post = 0 : GOSUB play_tone
    #snd_val = 189 : snd_gate = 20 : snd_post = 0 : GOSUB play_tone
END

' sound_hotdice: all six dice scored -- roll them all again (soundHotDice:
' rising 341,467,591,691 then the top two echoed).
sound_hotdice: PROCEDURE
    #snd_val = 328 : snd_gate = 4 : snd_post = 0 : GOSUB play_tone
    #snd_val = 240 : snd_gate = 4 : snd_post = 0 : GOSUB play_tone
    #snd_val = 189 : snd_gate = 4 : snd_post = 0 : GOSUB play_tone
    #snd_val = 162 : snd_gate = 8 : snd_post = 2 : GOSUB play_tone
    #snd_val = 189 : snd_gate = 4 : snd_post = 0 : GOSUB play_tone
    #snd_val = 162 : snd_gate = 12 : snd_post = 0 : GOSUB play_tone
END

' sound_noscore: a fujirkle -- the roll scored nothing and the turn's
' points are gone (soundNoScore: descending 633,521,443,386 Hz).
sound_noscore: PROCEDURE
    #snd_val = 177 : snd_gate = 8 : snd_post = 0 : GOSUB play_tone
    #snd_val = 215 : snd_gate = 8 : snd_post = 0 : GOSUB play_tone
    #snd_val = 253 : snd_gate = 12 : snd_post = 0 : GOSUB play_tone
    #snd_val = 290 : snd_gate = 20 : snd_post = 0 : GOSUB play_tone
END

' sound_player_join: another player sits down mid-lobby (rising sweep:
' 50,60,70,80 Hz).
sound_player_join: PROCEDURE
    #snd_val = 2237 : snd_gate = 2 : snd_post = 15 : GOSUB play_tone
    #snd_val = 1864 : snd_gate = 2 : snd_post = 15 : GOSUB play_tone
    #snd_val = 1598 : snd_gate = 2 : snd_post = 15 : GOSUB play_tone
    #snd_val = 1398 : snd_gate = 2 : snd_post = 15 : GOSUB play_tone
END

' sound_player_left: a player leaves mid-game (falling sweep, mirror of join).
sound_player_left: PROCEDURE
    #snd_val = 1398 : snd_gate = 2 : snd_post = 15 : GOSUB play_tone
    #snd_val = 1598 : snd_gate = 2 : snd_post = 15 : GOSUB play_tone
    #snd_val = 1864 : snd_gate = 2 : snd_post = 15 : GOSUB play_tone
    #snd_val = 2237 : snd_gate = 2 : snd_post = 15 : GOSUB play_tone
END

' sound_select: a menu/table/ready choice was confirmed (2-tone rising
' chirp: 300,350 Hz).
sound_select: PROCEDURE
    #snd_val = 373 : snd_gate = 3 : snd_post = 1 : GOSUB play_tone
    #snd_val = 320 : snd_gate = 3 : snd_post = 0 : GOSUB play_tone
END

' sound_cursor: cursor moved one position (300 Hz).
sound_cursor: PROCEDURE
    #snd_val = 373 : snd_gate = 2 : snd_post = 0 : GOSUB play_tone
END

' sound_tick: countdown clock tick / roll-animation flicker (short 341 Hz
' click, soundTick).
sound_tick: PROCEDURE
    #snd_val = 328 : snd_gate = 1 : snd_post = 0 : GOSUB play_tone
END

' sound_keep: a die was set aside to score (soundKeep: short rising run,
' 352,416,508 Hz).
sound_keep: PROCEDURE
    #snd_val = 318 : snd_gate = 1 : snd_post = 0 : GOSUB play_tone
    #snd_val = 269 : snd_gate = 1 : snd_post = 0 : GOSUB play_tone
    #snd_val = 220 : snd_gate = 2 : snd_post = 0 : GOSUB play_tone
END

' sound_release: a set-aside die was released back (soundRelease: 355 Hz
' pulses).
sound_release: PROCEDURE
    #snd_val = 315 : snd_gate = 1 : snd_post = 1 : GOSUB play_tone
    #snd_val = 315 : snd_gate = 1 : snd_post = 1 : GOSUB play_tone
    #snd_val = 315 : snd_gate = 1 : snd_post = 0 : GOSUB play_tone
END

' sound_rollbutton: ROLL or BANK submitted (soundRollButton: 344,403 Hz).
sound_rollbutton: PROCEDURE
    #snd_val = 325 : snd_gate = 4 : snd_post = 0 : GOSUB play_tone
    #snd_val = 278 : snd_gate = 4 : snd_post = 0 : GOSUB play_tone
END

' sound_bank: somebody's turn score landed in their total (soundScore:
' 493,521 Hz).
sound_bank: PROCEDURE
    #snd_val = 227 : snd_gate = 4 : snd_post = 0 : GOSUB play_tone
    #snd_val = 215 : snd_gate = 4 : snd_post = 2 : GOSUB play_tone
END

' sound_invalid: rejected input -- empty selection, unselectable die,
' out-of-turn action (single low buzz, 100 Hz, held).
sound_invalid: PROCEDURE
    #snd_val = 1119 : snd_gate = 10 : snd_post = 5 : GOSUB play_tone
END
