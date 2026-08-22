' dice.bas -- die-face GRAM bitmaps and Fujirkle dice-row rendering, MODE 1
' (FG/BG). Die art and print_die are carried over from
' fujinet-fujitzee/intv/dice.bas: each face is one 8x8 GRAM card, loaded
' once at boot, drawn by poking a BACKTAB cell to (index + 256) * 8 + color.
'
' All die states are color-only (no extra GRAM variants), keeping the GRAM
' budget at 6 of 64 cards:
'   COL_DICE_NORMAL  black-on-white   pool die that can score this roll
'   COL_DICE_DEAD    black-on-grey    pool die that cannot score (wire
'                                     selectable[i] <> '1')
'   COL_DICE_KEPT    black-on-yellow  die marked in the local keep mask
'   COL_DICE_CURSOR  black-on-green   die/button under the turn cursor
'                                     (wins over the other three)

' ---------------------------------------------------------------------------
' dice_init: load the 6 die faces into GRAM slots 0-5 (screen codes 256-261).
' Must run once at startup. GRAM loads take effect on the next video frame,
' so DEFINE is followed by WAIT -- a second DEFINE in the same frame would
' silently overwrite the first.
' ---------------------------------------------------------------------------
dice_init: PROCEDURE
    DEFINE 0, 6, diceart : WAIT
END

' ---------------------------------------------------------------------------
' print_die: draw one die face at BACKTAB cell dp_pos. Inputs: dp_pos (cell
' offset), dp_val (1-6; anything outside that range draws a blank cell so a
' short kept-dice string doesn't render garbage), #dp_color. The blank is
' GROM card 0 (the space glyph) -- just the color bits, card number 0.
' ---------------------------------------------------------------------------
DIM dp_pos, dp_val, #dp_color
print_die: PROCEDURE
    IF dp_val >= 1 AND dp_val <= 6 THEN
        #BACKTAB(dp_pos) = #dp_color + (dp_val - 1 + 256) * 8
    ELSE
        #BACKTAB(dp_pos) = #dp_color
    END IF
END

    CONST COL_DICE_NORMAL = FG_BLACK + BG_WHITE
    CONST COL_DICE_DEAD   = FG_BLACK + BG_GREY
    CONST COL_DICE_KEPT   = FG_BLACK + BG_YELLOW
    CONST COL_DICE_CURSOR = FG_BLACK + BG_GREEN
    CONST COL_BTN         = FG_BLACK + BG_WHITE

    CONST ROLL_COL = 8
    CONST BANK_COL = 14

' ---------------------------------------------------------------------------
' draw_pool_row: the pool dice at row POOL_ROW cols 0-5, then the ROLL and
' BANK buttons. Reads the live wire dice/selectable strings (FN_RX) and the
' local keep mask (SC_KEEP) directly rather than taking them as parameters
' -- all are always current by the time this is called. cur_pool (set by
' game_loop each poll) bounds the live dice; cells past it are blanked back
' to the page background, since the pool shrinks as dice are set aside.
'
' SC_KEEP holds the wire's keep-mask convention for Fujirkle: '1' (49) =
' set this die aside to SCORE it -- the OPPOSITE polarity of Fujitzee's
' reroll mask, because a Fujirkle set-aside is permanent, so the mask marks
' what you keep, not what you throw.
'
' Input: dr_cursor (0-5 = a pool die, 6 = ROLL, 7 = BANK, 255 = no cursor,
' used when it isn't your turn).
' ---------------------------------------------------------------------------
DIM dr_cursor, dr_i, #dr_ch, #dr_col
draw_pool_row: PROCEDURE
    FOR dr_i = 0 TO 5
        dp_pos = screenpos(dr_i, POOL_ROW)
        IF dr_i < cur_pool THEN
            #dr_ch = die_at(dr_i)
            IF #dr_ch >= 49 AND #dr_ch <= 54 THEN dp_val = #dr_ch - 48 ELSE dp_val = 0
            #dp_color = COL_DICE_DEAD
            #dr_ch = selectable_at(dr_i)
            IF #dr_ch = 49 THEN #dp_color = COL_DICE_NORMAL
            #dr_ch = (PEEK(SC_KEEP + dr_i) AND 255)
            IF #dr_ch = 49 THEN #dp_color = COL_DICE_KEPT
            IF dr_cursor = dr_i THEN #dp_color = COL_DICE_CURSOR
            GOSUB print_die
        ELSE
            #BACKTAB(dp_pos) = COL_TEXT
        END IF
    NEXT dr_i

    #dr_col = COL_BTN
    IF dr_cursor = 6 THEN #dr_col = COL_DICE_CURSOR
    PRINT AT screenpos(ROLL_COL, POOL_ROW) COLOR #dr_col, "ROLL"
    #dr_col = COL_BTN
    IF dr_cursor = 7 THEN #dr_col = COL_DICE_CURSOR
    PRINT AT screenpos(BANK_COL, POOL_ROW) COLOR #dr_col, "BANK"
END

' ---------------------------------------------------------------------------
' draw_kept_row: the dice set aside so far this turn (wire keptDice, up to
' 6) at row KEPT_ROW cols 5-10, plus the accumulated turn score at cols
' 13-18 ("+ NNNNN", blanked when zero). The kept string is NUL-terminated
' and shorter than 6 most of the time; blanks past its end self-erase the
' previous turn's longer tray.
' ---------------------------------------------------------------------------
DIM dk_i, #dk_ch, #dk_ts
draw_kept_row: PROCEDURE
    FOR dk_i = 0 TO 5
        #dk_ch = kept_at(dk_i)
        IF #dk_ch >= 49 AND #dk_ch <= 54 THEN dp_val = #dk_ch - 48 ELSE dp_val = 0
        dp_pos = screenpos(5 + dk_i, KEPT_ROW)
        IF dp_val = 0 THEN
            #BACKTAB(dp_pos) = COL_TEXT
        ELSE
            #dp_color = COL_DICE_NORMAL
            GOSUB print_die
        END IF
    NEXT dk_i

    #dk_ts = turn_score
    IF #dk_ts > 0 THEN
        PRINT AT screenpos(13, KEPT_ROW) COLOR COL_HILITE, "+"
        PRINT AT screenpos(14, KEPT_ROW) COLOR COL_HILITE, <.5>#dk_ts
    ELSE
        PRINT AT screenpos(13, KEPT_ROW) COLOR COL_TEXT, "      "
    END IF
END

' ---------------------------------------------------------------------------
' animate_roll: dice-rolling flourish shown right after any player's roll
' lands (yours or another seat's -- see game_loop's roll_changed check).
' FN_RX already holds the settled result at this point (the server computes
' the whole roll in one shot), so this doesn't determine the outcome. After
' any Fujirkle roll the ENTIRE remaining pool is freshly rolled (the server
' moves kept dice out of the dice string before rolling the rest), so every
' pool cell flickers -- no per-die keepRoll inspection like Fujitzee needed
' for its in-place rerolls. The kept row is untouched throughout.
' ---------------------------------------------------------------------------
    CONST ROLL_ANIM_FRAMES = 24
    CONST ROLL_ANIM_STEP = 3

DIM ra_frame, ra_i
animate_roll: PROCEDURE
    FOR ra_frame = 1 TO ROLL_ANIM_FRAMES
        IF ra_frame % ROLL_ANIM_STEP = 1 THEN
            FOR ra_i = 0 TO 5
                IF ra_i < cur_pool THEN
                    dp_val = RAND(6) + 1
                    dp_pos = screenpos(ra_i, POOL_ROW) : #dp_color = COL_DICE_NORMAL
                    GOSUB print_die
                END IF
            NEXT ra_i
            GOSUB sound_tick
        END IF
        WAIT
    NEXT ra_frame
END

' Pip grid: border at rows/cols 0 and 7; pips at rows/cols {2,4,6} of an
' 8x8 cell. Row4 col4 is the single center pip (face 1); rows/cols 2 and 6
' are the four corners (faces 4-6); row4 cols 2/6 are the middle side pips
' (face 6 only).
diceart:
    ' Face 1 (index 0): center pip.
    ' Face 1 (index 0): center pip.
    BITMAP "........"
    BITMAP "........"
    BITMAP "........"
    BITMAP "...o...."
    BITMAP "........"
    BITMAP "........"
    BITMAP "........"
    BITMAP "........"

    ' Face 2 (index 1): top-left, bottom-right.
    BITMAP "........"
    BITMAP ".o......"
    BITMAP "........"
    BITMAP "........"
    BITMAP "........"
    BITMAP ".....o.."
    BITMAP "........"
    BITMAP "........"

    ' Face 3 (index 2): top-left, center, bottom-right.
    BITMAP "........"
    BITMAP ".o......"
    BITMAP "........"
    BITMAP "...o...."
    BITMAP "........"
    BITMAP ".....o.."
    BITMAP "........"
    BITMAP "........"

    ' Face 4 (index 3): four corners.
    BITMAP "........"
    BITMAP ".o...o.."
    BITMAP "........"
    BITMAP "........"
    BITMAP "........"
    BITMAP ".o...o.."
    BITMAP "........"
    BITMAP "........"

    ' Face 5 (index 4): four corners + center.
    BITMAP "........"
    BITMAP ".o...o.."
    BITMAP "........"
    BITMAP "...o...."
    BITMAP "........"
    BITMAP ".o...o.."
    BITMAP "........"
    BITMAP "........"

    ' Face 6 (index 5): four corners + two middle side pips.
    BITMAP "........"
    BITMAP ".o...o.."
    BITMAP "........"
    BITMAP ".o...o.."
    BITMAP "........"
    BITMAP ".o...o.."
    BITMAP "........"
    BITMAP "........"
