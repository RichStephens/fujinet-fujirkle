' fujirkle.bas -- FujiNet Fujirkle (Farkle-style dice) for Intellivision,
' entry point + state machine. Ported from fujinet-fujitzee/intv/fujitzee.bas
' (name entry -> table select -> game loop -> in-game menu), adapted for a
' press-your-luck dice game: no scorecard, so the 20x12 screen shows all six
' players' banked totals permanently, with the shared dice pool, kept tray,
' turn score, and ROLL/BANK buttons below (board.bas/dice.bas).
'
' GOTO boot_start jumps over every INCLUDE below before any of it runs --
' see fujinet.bas for why: falling into a PROCEDURE or DATA block by
' straight-line execution corrupts the return stack.
    GOTO boot_start

    INCLUDE "constants.bas"

    ' ROWCELLS/STATUS_ROW/PROMPT_ROW must be declared before the includes
    ' below: the compiler resolves CONST symbols at parse time (unlike
    ' GOSUB labels, which resolve fine forward), so a reference from within
    ' an included file to a CONST declared later in this file silently
    ' falls back to an implicit, unassigned 8-bit variable defaulting to 0
    ' -- Fujitzee's show_standings hit exactly this, printing its footer
    ' over row 0 instead of row 11.
    CONST ROWCELLS = 20
    CONST STATUS_ROW = screenpos(0, 11)
    ' The server prompt is up to 40 chars; it gets the bottom two rows via
    ' one draw_field spanning the linear BACKTAB from row 10 into row 11
    ' (draw_field's blank-padding self-erases a longer previous prompt).
    CONST PROMPT_ROW = screenpos(0, 10)

    INCLUDE "fujinet.bas"
    INCLUDE "state.bas"
    INCLUDE "board.bas"
    INCLUDE "dice.bas"
    INCLUDE "sound.bas"

' ---------------------------------------------------------------------------
' URL literals (ASCII DATA). url_path selects the endpoint:
'   0 tables   1 state   2 ready   3 roll/<mask>   4 bank/<mask>   5 leave
' The keep mask sent with roll/bank is SC_KEEP's raw ascii bytes, exactly
' mask_len long -- the server rejects (as a silent no-op) any mask whose
' length differs from the pool still in play.
' ---------------------------------------------------------------------------
lit_n_colon: DATA 78,58
' "https://fujirkle.carr-designs.com/" (34 bytes). For local testing swap
' in the line below it ("http://127.0.0.1:8080/", 22 bytes) and set
' LEN_HTTPS to match.
lit_https: DATA 104,116,116,112,115,58,47,47,102,117,106,105,114,107,108,101,46,99,97,114,114,45,100,101,115,105,103,110,115,46,99,111,109,47
'lit_https: DATA 104,116,116,112,58,47,47,49,50,55,46,48,46,48,46,49,58,56,48,56,48,47
lit_tables: DATA 116,97,98,108,101,115
lit_qbin: DATA 63,98,105,110,61,49
lit_state: DATA 115,116,97,116,101
lit_ready: DATA 114,101,97,100,121
lit_roll: DATA 114,111,108,108,47
lit_bank: DATA 98,97,110,107,47
lit_leave: DATA 108,101,97,118,101
lit_qtable: DATA 63,116,97,98,108,101,61
lit_aplayer: DATA 38,112,108,97,121,101,114,61
lit_abin: DATA 38,98,105,110,61,49

    CONST LEN_HTTPS = 34
    'CONST LEN_HTTPS = 22

    ' Lobby appkey: creator 1 / app 1, key = this game's registered slot.
    ' See fujinet-fujirkle/src/misc.h AK_LOBBY_KEY_SERVER -- Fujirkle's is
    ' 9 (Fujitzee's was 3); it must match the server's LOBBY_CLIENT_APP_KEY
    ' or the Lobby hands clients another game's server.
    CONST AK_LOBBY_KEY_SERVER = 9

' ---------------------------------------------------------------------------
' Globals
' ---------------------------------------------------------------------------
    DIM gs_i, gs_j, #gs_char, #gs_c
    DIM ne_i, ne_j, ne_cur, ne_len
    DIM ne_buf(8)
    DIM ak_try
    DIM #tbl_count, tbl_sel
    DIM inp_lock
    DIM want_leave, im_sel
    DIM hs_i, hs_page
    DIM url_path
    DIM #tmp_addr

    ' split_room_url locals (see procedure below)
    DIM sp_i, sp_found, sp_ok, sp_j, sp_k, sp_c, sp_valid, sp_m

    DIM #cur_round, #cur_pc, prev_round, prev_active, prev_seen_pc, my_turn
    DIM poll_wait, has_action
    DIM turn_changed, roll_changed, shadow_seeded, pc_changed
    DIM fujirkle_fired, fj_now, bank_seen, lb_held
    DIM mask_len, pt_i, ti_any, ti_framecount, #ti_timeleft
    DIM #hdr_t, #lb_pc

' ---------------------------------------------------------------------------
' draw_field: render df_len ASCII bytes from #df_src onto the screen at
' BACKTAB offset df_pos, in color #df_color. MODE 1 only exposes GROM cards
' 0-63 (ASCII 32-95, no lowercase), so this uppercases -- server strings
' (names, prompts) arrive lowercased. Stops at a NUL and pads the rest of
' the field with spaces; clamps anything outside the printable range to a
' space rather than drawing garbage. Ported from fujinet-fujitzee/intv.
' ---------------------------------------------------------------------------
DIM df_i, #df_c, df_stop
draw_field: PROCEDURE
    df_stop = 0
    FOR df_i = 0 TO df_len - 1
        #df_c = (PEEK(#df_src + df_i) AND 255)
        IF #df_c = 0 THEN df_stop = 1
        IF df_stop THEN
            #df_c = 32
        ELSEIF #df_c >= 97 AND #df_c <= 122 THEN
            #df_c = #df_c - 32
        END IF
        IF #df_c < 32 OR #df_c > 95 THEN #df_c = 32
        #BACKTAB(df_pos + df_i) = (#df_c - 32) * 8 + #df_color
    NEXT df_i
END

' ---------------------------------------------------------------------------
' cls_blue: CLS, then fill the whole 240-cell BACKTAB with a blank blue
' card. CLS alone leaves every cell at card 0 / color 0 (black), so
' without this every screen would still show a black field behind
' whatever text gets drawn on top of it. Paced one row (20 cells) per WAIT
' -- the STIC has no page-flip, and this is the single largest unbroken
' BACKTAB write in the project; splitting it keeps it from tearing into
' whatever's already on screen.
' ---------------------------------------------------------------------------
DIM cb_i, cb_row
cls_blue: PROCEDURE
    CLS
    FOR cb_row = 0 TO 11
        FOR cb_i = 0 TO 19
            #BACKTAB(cb_row * 20 + cb_i) = COL_TEXT
        NEXT cb_i
        WAIT
    NEXT cb_row
END

' ---------------------------------------------------------------------------
' compose_url: build "N:<endpoint><path...><suffix>" directly into FN_TX.
' url_path=3/4 (roll/bank) append SC_KEEP's raw ascii mask bytes, exactly
' mask_len of them (captured by turn_input at submit time = the pool size
' the player acted on; it's already in wire format, '1' = keep/score).
' ---------------------------------------------------------------------------
compose_url: PROCEDURE
    #fn_txlen = 0
    #fn_src = VARPTR lit_n_colon(0) : fn_len = 2 : GOSUB fn_putstr
    #fn_src = SC_ENDPT : ls_max = 65 : GOSUB fn_strlen : GOSUB fn_putstr

    IF url_path = 0 THEN
        #fn_src = VARPTR lit_tables(0) : fn_len = 6 : GOSUB fn_putstr
        #fn_src = VARPTR lit_qbin(0) : fn_len = 6 : GOSUB fn_putstr
    ELSE
        IF url_path = 1 THEN
            #fn_src = VARPTR lit_state(0) : fn_len = 5 : GOSUB fn_putstr
        ELSEIF url_path = 2 THEN
            #fn_src = VARPTR lit_ready(0) : fn_len = 5 : GOSUB fn_putstr
        ELSEIF url_path = 3 THEN
            #fn_src = VARPTR lit_roll(0) : fn_len = 5 : GOSUB fn_putstr
            #fn_src = SC_KEEP : fn_len = mask_len : GOSUB fn_putstr
        ELSEIF url_path = 4 THEN
            #fn_src = VARPTR lit_bank(0) : fn_len = 5 : GOSUB fn_putstr
            #fn_src = SC_KEEP : fn_len = mask_len : GOSUB fn_putstr
        ELSE
            #fn_src = VARPTR lit_leave(0) : fn_len = 5 : GOSUB fn_putstr
        END IF

        #fn_src = VARPTR lit_qtable(0) : fn_len = 7 : GOSUB fn_putstr
        #fn_src = SC_TABLE : ls_max = 9 : GOSUB fn_strlen : GOSUB fn_putstr
        #fn_src = VARPTR lit_aplayer(0) : fn_len = 8 : GOSUB fn_putstr
        #fn_src = SC_NAME : ls_max = 9 : GOSUB fn_strlen : GOSUB fn_putstr
        #fn_src = VARPTR lit_abin(0) : fn_len = 6 : GOSUB fn_putstr
    END IF
END

' ===========================================================================
' Boot
' ===========================================================================
boot_start:
    MODE 1
    GOSUB cls_blue
    GOSUB dice_init
    PRINT AT 0 COLOR COL_TEXT, "FUJIRKLE"
    PRINT AT 40, "CONNECTING TO FUJINET"
    GOSUB fn_wait_mailbox
    IF fn_ok = 0 THEN
        PRINT AT 40, "NO CARTRIDGE MAILBOX"
        GOTO halt
    END IF

' ---------------------------------------------------------------------------
' Name: try the shared lobby appkey (creator 1 / app 1 / key 0) first, with
' one retry (a cold RP2040/ESP32 link right after the mailbox comes up can
' time out the very first transaction); fall back to the disc letter picker
' if the read fails, comes back empty, or holds anything outside A-Z0-9 --
' this slot is shared by every FujiNet client, and a stray character here
' goes straight into the query string unescaped.
' ---------------------------------------------------------------------------
    gs_i = 0
    FOR ak_try = 0 TO 1
        ak_creator_lo = 1 : ak_creator_hi = 0 : ak_app = 1 : ak_key = 0 : ak_mode = 0
        GOSUB appkey_open
        IF fn_ok THEN EXIT FOR
    NEXT ak_try
    IF fn_ok THEN
        #fn_src = SC_NAME : ls_max = 9 : GOSUB appkey_read
        GOSUB appkey_close
        IF fn_len > 0 THEN
            gs_i = 1
            FOR gs_j = 0 TO fn_len - 1
                #gs_char = (PEEK(SC_NAME + gs_j) AND 255)
                IF #gs_char >= 97 AND #gs_char <= 122 THEN #gs_char = #gs_char - 32
                IF (#gs_char < 65 OR #gs_char > 90) AND (#gs_char < 48 OR #gs_char > 57) THEN gs_i = 0
            NEXT gs_j
        END IF
    END IF
    IF gs_i = 0 THEN GOSUB name_entry_screen

' ---------------------------------------------------------------------------
' Room: check the lobby server appkey (creator 1 / app 1 / key
' AK_LOBBY_KEY_SERVER). If the lobby wrote a room there, split it into
' SC_ENDPT/SC_TABLE and skip straight past table select. Seed the
' compiled-in default endpoint first so SC_ENDPT is always valid even if
' the appkey is empty or the read fails -- compose_url relies on it from
' here on for every request, not just /tables.
' ---------------------------------------------------------------------------
    FOR gs_i = 0 TO LEN_HTTPS - 1
        POKE (SC_ENDPT + gs_i), PEEK(VARPTR lit_https(0) + gs_i) AND 255
    NEXT gs_i
    POKE (SC_ENDPT + LEN_HTTPS), 0
    POKE SC_TABLE, 0

    FOR ak_try = 0 TO 1
        ak_creator_lo = 1 : ak_creator_hi = 0 : ak_app = 1
        ak_key = AK_LOBBY_KEY_SERVER : ak_mode = 0
        GOSUB appkey_open
        IF fn_ok THEN EXIT FOR
    NEXT ak_try
    IF fn_ok THEN
        #fn_src = SC_ENDPT : ls_max = 65 : GOSUB appkey_read
        GOSUB appkey_close
        IF fn_len > 0 THEN
            GOSUB split_room_url
        ELSE
            ' appkey_read always NUL-terminates at fn_len, even when
            ' empty, which would otherwise wipe out the default just
            ' seeded above -- restore it.
            FOR gs_i = 0 TO LEN_HTTPS - 1
                POKE (SC_ENDPT + gs_i), PEEK(VARPTR lit_https(0) + gs_i) AND 255
            NEXT gs_i
            POKE (SC_ENDPT + LEN_HTTPS), 0
        END IF
    END IF

    IF (PEEK(SC_TABLE) AND 255) <> 0 THEN GOTO table_joined

' ===========================================================================
' Table select (re-entered after leaving a table)
' ===========================================================================
table_select:
    GOSUB cls_blue
    PRINT AT 0 COLOR COL_TEXT, "CHOOSE A TABLE"
    PRINT AT 40, "REFRESHING..."

    url_path = 0 : GOSUB compose_url
    #net_readlen = TABLES_MAXLEN
    GOSUB api_call

    IF fn_ok = 1 THEN
        #tmp_addr = 1 + (PEEK(FN_RX) AND 255) * TABLE_STRIDE
        IF #net_gotlen < #tmp_addr THEN fn_ok = 0
    END IF

    IF fn_ok = 0 THEN
        PRINT AT 40, "SERVER UNREACHABLE  "
        inp_lock = 90
        WHILE inp_lock > 0
            inp_lock = inp_lock - 1
            WAIT
        WEND
        GOTO table_select
    END IF

    #tbl_count = (PEEK(FN_RX) AND 255)
    IF #tbl_count > 8 THEN #tbl_count = 8
    IF #tbl_count = 0 THEN
        PRINT AT 40, "NO TABLES AVAILABLE "
        inp_lock = 90
        WHILE inp_lock > 0
            inp_lock = inp_lock - 1
            WAIT
        WEND
        GOTO table_select
    END IF

    GOSUB cls_blue
    PRINT AT 0 COLOR COL_TEXT, "CHOOSE A TABLE"
    FOR gs_i = 0 TO #tbl_count - 1
        #tmp_addr = table_addr(gs_i)
        #df_src = #tmp_addr + TBL_NAME : df_pos = 20 + gs_i * 20 + 1 : df_len = 14 : #df_color = COL_TEXT
        GOSUB draw_field
        #df_src = #tmp_addr + TBL_PLAYERS : df_pos = 20 + gs_i * 20 + 15 : df_len = 5 : #df_color = COL_TEXT
        GOSUB draw_field
    NEXT gs_i

    tbl_sel = 0
    inp_lock = 0
ts_input:
    WAIT
    FOR gs_i = 0 TO #tbl_count - 1
        #gs_c = COL_TEXT
        IF gs_i = tbl_sel THEN #gs_c = COL_HILITE
        #BACKTAB(20 + gs_i * 20) = (62 - 32) * 8 + #gs_c ' '>' cursor glyph
    NEXT gs_i

    IF inp_lock > 0 THEN inp_lock = inp_lock - 1 : GOTO ts_input

    IF CONT1.DOWN THEN
        tbl_sel = tbl_sel + 1
        IF tbl_sel >= #tbl_count THEN tbl_sel = 0
        inp_lock = 8
        GOSUB sound_cursor
        GOTO ts_input
    END IF
    IF CONT1.UP THEN
        IF tbl_sel = 0 THEN tbl_sel = #tbl_count
        tbl_sel = tbl_sel - 1
        inp_lock = 8
        GOSUB sound_cursor
        GOTO ts_input
    END IF
    IF CONT1.BUTTON = 0 THEN GOTO ts_input
    GOSUB sound_select

    #tmp_addr = table_addr(tbl_sel)
    FOR gs_i = 0 TO 8
        POKE (SC_TABLE + gs_i), PEEK(#tmp_addr + TBL_ID + gs_i) AND 255
    NEXT gs_i

    ' Update the lobby server appkey so a reboot without going back
    ' through the lobby rejoins this table.
    GOSUB write_room_appkey

table_joined:
' ===========================================================================
' In a table: poll /state (or submit a pending ready/roll/bank action in
' its place) and dispatch on round. A full CLS happens only on the
' transition into/out of play (lobby<->play<->gameover); every other poll
' updates cells in place -- the STIC has no page-flip, so a per-poll CLS
' would visibly blank the screen every cycle.
' ===========================================================================
    prev_round = 255  ' sentinel: forces the initial CLS/board_init
    prev_active = 255 ' sentinel: forces the initial turn-start reset+sound
    prev_seen_pc = 255 ' sentinel: skip the join/leave cue on the first poll
    poll_wait = 0
    want_leave = 0
    has_action = 0
    dice_cur = 0
    lb_held = 1 ' the button press that picked the table may still be down
    GOSUB sound_join

game_loop:
    WAIT

    IF poll_wait > 0 THEN
        poll_wait = poll_wait - 1
        GOTO gl_input
    END IF

    IF has_action = 1 THEN
        url_path = 2
    ELSEIF has_action = 2 THEN
        url_path = 3
    ELSEIF has_action = 3 THEN
        url_path = 4
    ELSE
        url_path = 1
    END IF
    has_action = 0
    GOSUB compose_url
    #net_readlen = GAME_MAXLEN
    GOSUB api_call
    GOSUB validate_state

    IF fn_ok = 0 THEN
        poll_wait = 30
        GOTO gl_input
    END IF

    #cur_round = state_round
    #cur_pc = state_playercount

    pc_changed = 0
    IF prev_seen_pc <> 255 THEN
        IF #cur_pc > prev_seen_pc THEN pc_changed = 1 : GOSUB sound_player_join
        IF #cur_pc < prev_seen_pc THEN pc_changed = 1 : GOSUB sound_player_left
    ELSE
        pc_changed = 1
    END IF
    prev_seen_pc = #cur_pc

    IF #cur_round = ROUND_LOBBY THEN
        IF prev_round <> ROUND_LOBBY THEN GOSUB cls_blue
        GOSUB render_lobby
        poll_wait = 45
    ELSEIF #cur_round = ROUND_GAMEOVER THEN
        IF prev_round <> ROUND_GAMEOVER THEN
            GOSUB sound_gamedone
            GOSUB cls_blue
            GOSUB render_gameover
        END IF
        poll_wait = 60
    ELSE
        ' ---- play (round 1..98; Fujirkle rounds are unbounded) ----
        IF prev_round = ROUND_LOBBY OR prev_round = 255 OR prev_round = ROUND_GAMEOVER THEN
            GOSUB cls_blue
            GOSUB board_init
            dice_cur = 0
            POKE SC_PREVDICE, 0 ' sentinel: don't animate the first paint
            shadow_seeded = 0   ' sentinel: don't bank-diff against garbage
            fujirkle_fired = 0
            FOR pt_i = 0 TO 5
                POKE (SC_KEEP + pt_i), 48
            NEXT pt_i
        END IF

        ' Pool size: the dice string shrinks as dice are set aside; count
        ' leading '1'..'6' bytes (NUL-terminated on the wire).
        cur_pool = 6
        FOR pt_i = 0 TO 5
            #gs_char = die_at(pt_i)
            IF #gs_char < 49 THEN cur_pool = pt_i : EXIT FOR
            IF #gs_char > 54 THEN cur_pool = pt_i : EXIT FOR
        NEXT pt_i

        turn_changed = 0
        IF active_player <> prev_active THEN turn_changed = 1

        ' Deliberately nested single-condition IFs, not one chained AND:
        ' IntyBASIC v1.4.2 miscompiles a chain of two "X = const"
        ' comparisons joined by AND when one side's value comes from an
        ' expression that itself contains "AND 255" (any state.bas DEF FN
        ' body) -- the fused codegen ANDs in a stray always-zero register
        ' instead of the second comparison, so the whole condition is
        ' always false. Confirmed in Fujitzee by reading the generated
        ' .lst: it never called turn_input. The validmoves gate is what
        ' keeps us out of turn_input during the server's 3-second
        ' FUJIRKLE hold (activePlayer stays 0 there but validMoves is 0).
        my_turn = 0
        IF active_player = 0 THEN
            IF state_viewing = 0 THEN
                IF state_validmoves <> 0 THEN my_turn = 1
            END IF
        END IF

        ' A roll landed for whoever's turn it is -- the wire dice string
        ' differs from the copy shadowed on the previous poll (or the turn
        ' itself changed, covering the server's automatic opening roll).
        ' The shadow MUST be compared before anything below overwrites it:
        ' FN_RX is rewritten by every mailbox transaction, so prev-state
        ' copies only survive within the poll that took them.
        roll_changed = 0
        #gs_char = (PEEK(SC_PREVDICE) AND 255)
        IF #gs_char <> 0 THEN
            FOR pt_i = 0 TO 6
                #gs_char = (PEEK(FN_RX + GAME_DICE + pt_i) AND 255)
                #gs_c = (PEEK(SC_PREVDICE + pt_i) AND 255)
                IF #gs_char <> #gs_c THEN roll_changed = 1
            NEXT pt_i
            IF turn_changed THEN roll_changed = 1
        END IF

        IF roll_changed THEN
            ' The pool the player was selecting from is gone -- reset the
            ' local mask and keep the cursor inside the new pool.
            FOR pt_i = 0 TO 5
                POKE (SC_KEEP + pt_i), 48
            NEXT pt_i
            IF dice_cur < 6 THEN
                IF cur_pool = 0 THEN
                    dice_cur = 6
                ELSEIF dice_cur >= cur_pool THEN
                    dice_cur = cur_pool - 1
                END IF
            END IF
            GOSUB animate_roll
            ' Hot dice: the previous selection scored out the whole pool,
            ' so the server dealt six fresh dice with the turn score
            ' carried and the kept tray cleared (gamelogic.c's check:
            ' turnScore > 0, full pool, empty keptDice).
            #gs_char = turn_score
            IF #gs_char > 0 THEN
                IF cur_pool = 6 THEN
                    IF kept_at(0) = 0 THEN GOSUB sound_hotdice
                END IF
            END IF
        END IF

        ' Fujirkle: your own roll scored nothing. The server zeroes
        ' validMoves and holds the busted roll on screen for 3 seconds
        ' (FUJIRKLE_SHOW_TIME) before advancing -- a 20-frame poll cadence
        ' always catches that window. Latched so the sting plays once per
        ' hold, and only for your own bust (matching the reference
        ' clients; other seats' busts are narrated by the prompt).
        fj_now = 0
        IF state_viewing = 0 THEN
            IF active_player = 0 THEN
                IF state_validmoves = 0 THEN fj_now = 1
            END IF
        END IF
        IF fj_now = 1 THEN
            IF fujirkle_fired = 0 THEN
                fujirkle_fired = 1
                GOSUB sound_noscore
            END IF
        ELSE
            fujirkle_fired = 0
        END IF

        ' Bank: any seated player's banked total grew since the previous
        ' poll. TurnScore reads 0 by the time we see it -- the server
        ' folds it into the total and starts the next turn in the same
        ' request -- so the score delta is the only trace of a bank.
        ' Skipped when the seat list changed (indices shifted).
        bank_seen = 0
        IF shadow_seeded = 1 THEN
            IF pc_changed = 0 THEN
                FOR pt_i = 0 TO 5
                    IF pt_i < #cur_pc THEN
                        #tmp_addr = player_addr(pt_i)
                        #gs_char = score_of(#tmp_addr)
                        #gs_c = (PEEK(SC_PREVSCORES + pt_i * 2 + 1) AND 255) * 256 + (PEEK(SC_PREVSCORES + pt_i * 2) AND 255)
                        IF #gs_char > #gs_c THEN bank_seen = 1
                    END IF
                NEXT pt_i
            END IF
        END IF
        IF bank_seen THEN GOSUB sound_bank

        ' Refresh the shadows for the next poll -- in the same poll that
        ' consumed them, before any further mailbox transaction touches
        ' FN_RX.
        FOR pt_i = 0 TO 6
            POKE (SC_PREVDICE + pt_i), PEEK(FN_RX + GAME_DICE + pt_i) AND 255
        NEXT pt_i
        FOR pt_i = 0 TO 5
            #gs_char = 0
            IF pt_i < #cur_pc THEN
                #tmp_addr = player_addr(pt_i)
                #gs_char = score_of(#tmp_addr)
            END IF
            POKE (SC_PREVSCORES + pt_i * 2), #gs_char AND 255
            POKE (SC_PREVSCORES + pt_i * 2 + 1), (#gs_char / 256) AND 255
        NEXT pt_i
        shadow_seeded = 1

        IF turn_changed THEN
            IF my_turn THEN
                GOSUB sound_myturn
                dice_cur = 0
                FOR pt_i = 0 TO 5
                    POKE (SC_KEEP + pt_i), 48
                NEXT pt_i
            END IF
        END IF

        GOSUB render_playscreen

        IF my_turn THEN
            GOSUB turn_input
            IF has_action THEN poll_wait = 0 ELSE poll_wait = 20
        ELSE
            poll_wait = 20
        END IF
    END IF
    prev_active = active_player
    prev_round = #cur_round

gl_input:
    ' Lobby ready-toggle lives here (sampled every frame) rather than in
    ' render_lobby (sampled only once per 45-frame poll, where a quick tap
    ' between polls would be missed): a fresh press queues has_action=1,
    ' and the next loop pass sends /ready in place of /state. lb_held
    ' requires a release between toggles so one press can't ready and
    ' un-ready across two polls.
    IF #cur_round = ROUND_LOBBY THEN
        IF CONT1.BUTTON THEN
            IF lb_held = 0 THEN
                lb_held = 1
                GOSUB sound_select
                has_action = 1
                poll_wait = 0
            END IF
        ELSE
            lb_held = 0
        END IF
    END IF

    IF CONT1.KEY = 10 THEN GOSUB ingame_menu
    IF want_leave THEN
        want_leave = 0
        GOTO table_select
    END IF
    GOTO game_loop

' ===========================================================================
' render_lobby: server name, ready list, prompt. The 13-byte player record
' carries a dedicated ready byte (PL_READY: 1=ready, 0=not, $FE=watching)
' -- unlike Fujitzee, where scores[0] doubled as the flag.
' ===========================================================================
render_lobby: PROCEDURE
    #df_src = FN_RX + GAME_SERVERNAME : df_pos = screenpos(0, 0) : df_len = 20 : #df_color = COL_TEXT
    GOSUB draw_field

    #lb_pc = state_playercount
    IF #lb_pc > 8 THEN #lb_pc = 8
    FOR gs_i = 0 TO 7
        IF gs_i < #lb_pc THEN
            #tmp_addr = player_addr(gs_i)
            pc_idx = gs_i
            GOSUB player_color
            #df_src = #tmp_addr + PL_NAME : df_pos = screenpos(0, gs_i + 2) : df_len = 9 : #df_color = #pc_color
            GOSUB draw_field
            #gs_char = ready_of(#tmp_addr)
            #gs_c = 46 ' '.'
            IF #gs_char = READY_YES THEN #gs_c = 42 ' '*'
            IF #gs_char = READY_VIEWING THEN #gs_c = 86 ' 'V'
            #BACKTAB(screenpos(10, gs_i + 2)) = (#gs_c - 32) * 8 + COL_TEXT
        ELSE
            ' Blank departed players' rows -- there's no per-poll CLS.
            PRINT AT screenpos(0, gs_i + 2) COLOR COL_TEXT, "           "
        END IF
    NEXT gs_i
    WAIT

    #df_src = FN_RX + GAME_PROMPT : df_pos = PROMPT_ROW : df_len = 40 : #df_color = COL_TEXT
    GOSUB draw_field
END

' ===========================================================================
' render_gameover: final standings (the same name+total list that's live
' during play) plus the prompt naming the winner. Held on screen
' (poll_wait=60) until the server resets the table back to the lobby.
' ===========================================================================
render_gameover: PROCEDURE
    prl_top = 1
    GOSUB render_player_list
    #df_src = FN_RX + GAME_PROMPT : df_pos = PROMPT_ROW : df_len = 40 : #df_color = COL_HILITE
    GOSUB draw_field
END

' ===========================================================================
' render_header: table name, round counter, move timer. The timer cell is
' also repainted once a second by turn_input's own countdown during your
' turn; between polls of other seats' turns it shows the server's value.
' ===========================================================================
render_header: PROCEDURE
    #df_src = FN_RX + GAME_SERVERNAME : df_pos = screenpos(0, 0) : df_len = 12 : #df_color = COL_LABEL
    GOSUB draw_field
    PRINT AT screenpos(13, 0) COLOR COL_ROUND, "R"
    PRINT AT screenpos(14, 0) COLOR COL_ROUND, <.2>#cur_round
    #hdr_t = state_movetime
    ' <.3> (space-padded), not <3> (zero-padded): .z never initializes the
    ' register that picks blank-vs-zero padding, so a shorter value can
    ' print garbage in the unused leading digit(s). moveTime runs to 250
    ' when solo against bots, so the field needs all 3 digits.
    PRINT AT screenpos(17, 0) COLOR COL_TEXT, <.3>#hdr_t
END

' ===========================================================================
' render_playscreen: full redraw of the play screen from whatever is
' currently sitting in FN_RX -- header, player list, kept tray, pool row,
' prompt. Split with WAITs between regions so one poll's redraw can't tear
' a frame (STIC has no page-flip; a big unbroken BACKTAB write can spill
' past vblank into active display).
' ===========================================================================
render_playscreen: PROCEDURE
    GOSUB render_header
    prl_top = 1
    GOSUB render_player_list
    WAIT
    GOSUB draw_kept_row
    dr_cursor = 255
    IF my_turn THEN dr_cursor = dice_cur
    GOSUB draw_pool_row
    WAIT
    #df_src = FN_RX + GAME_PROMPT : df_pos = PROMPT_ROW : df_len = 40 : #df_color = COL_TEXT
    GOSUB draw_field
END

' ===========================================================================
' restore_screen: full repaint of whatever screen #cur_round says should be
' up right now, from whatever's already cached in FN_RX -- not a fresh
' poll. Shared by any full-screen overlay (ingame_menu, help_screen) that
' can be opened from inside turn_input's own blocking ti_loop: control
' doesn't return to game_loop's CLS/redraw dispatch until the current turn
' ends, which could be a long time away, so without this the overlay would
' just leave its own drawing sitting there once closed.
' ===========================================================================
restore_screen: PROCEDURE
    prev_round = 255 ' safety net, in case some other path still relies on it
    GOSUB cls_blue
    IF #cur_round = ROUND_LOBBY THEN
        GOSUB render_lobby
    ELSEIF #cur_round = ROUND_GAMEOVER THEN
        GOSUB render_gameover
    ELSE
        GOSUB board_init
        GOSUB render_playscreen
    END IF
END

' ===========================================================================
' turn_input: your turn. Blocks in its own WAIT loop (like Fujitzee's)
' walking one cursor over the pool dice and the ROLL/BANK buttons
' (positions 0-5 = dice, 6 = ROLL, 7 = BANK, matching the reference
' clients' cursor stops). The action button toggles a die into the local
' keep mask (only where the wire says it can score); on ROLL/BANK it
' captures mask_len and sets has_action, returning to let the outer poll
' loop perform the actual network call -- the reply to roll/<mask> or
' bank/<mask> is the next state.
' ===========================================================================
turn_input: PROCEDURE
    inp_lock = 0
    has_action = 0
    #ti_timeleft = state_movetime
    ti_framecount = 0
    PRINT AT screenpos(17, 0) COLOR COL_TEXT, <.3>#ti_timeleft

ti_loop:
    WAIT

    ti_framecount = ti_framecount + 1
    IF ti_framecount >= 60 THEN
        ti_framecount = 0
        IF #ti_timeleft > 0 THEN #ti_timeleft = #ti_timeleft - 1
        PRINT AT screenpos(17, 0) COLOR COL_TEXT, <.3>#ti_timeleft
        GOSUB sound_tick
        ' The server enforces its own move time limit and banks the best
        ' dice for a player who runs out (gameLogic.go's forceHumanMove)
        ' -- without this bail, a player who never acts would stay stuck
        ' in this loop forever and never poll again to discover the turn
        ' moved on.
        IF #ti_timeleft = 0 THEN RETURN
    END IF

    IF inp_lock > 0 THEN inp_lock = inp_lock - 1 : GOTO ti_loop

    IF CONT1.KEY = 10 THEN
        GOSUB ingame_menu
        IF want_leave THEN RETURN
        GOTO ti_loop
    END IF

    ' Disc before BUTTON deliberately: the three action-button signals
    ' share overlapping bits (constants.bas's BUTTON_MASK), so
    ' CONT1.BUTTON reads non-zero for any side button; the disc directions
    ' are distinct signals and must be given first refusal.
    IF CONT1.RIGHT THEN
        dice_cur = (dice_cur + 1) % 8
        ' Skip pool slots that no longer hold a die (the pool shrinks from
        ' the right as dice are set aside).
ti_skip_fwd:
        IF dice_cur < 6 THEN
            IF dice_cur >= cur_pool THEN
                dice_cur = dice_cur + 1
                GOTO ti_skip_fwd
            END IF
        END IF
        inp_lock = 8 : GOSUB sound_cursor
        dr_cursor = dice_cur : GOSUB draw_pool_row
        GOTO ti_loop
    END IF
    IF CONT1.LEFT THEN
        dice_cur = (dice_cur + 7) % 8
ti_skip_back:
        IF dice_cur < 6 THEN
            IF dice_cur >= cur_pool THEN
                IF dice_cur = 0 THEN
                    dice_cur = 7
                ELSE
                    dice_cur = dice_cur - 1
                END IF
                GOTO ti_skip_back
            END IF
        END IF
        inp_lock = 8 : GOSUB sound_cursor
        dr_cursor = dice_cur : GOSUB draw_pool_row
        GOTO ti_loop
    END IF
    IF CONT1.UP THEN
        dice_cur = 6 ' jump straight to ROLL
        inp_lock = 8 : GOSUB sound_cursor
        dr_cursor = dice_cur : GOSUB draw_pool_row
        GOTO ti_loop
    END IF
    IF CONT1.DOWN THEN
        dice_cur = 7 ' jump straight to BANK
        inp_lock = 8 : GOSUB sound_cursor
        dr_cursor = dice_cur : GOSUB draw_pool_row
        GOTO ti_loop
    END IF

    IF CONT1.BUTTON = 0 THEN GOTO ti_loop

    IF dice_cur < 6 THEN
        ' On a die -- toggle it in/out of the keep mask, but only if the
        ' wire's selectable string says this die can be part of a scoring
        ' combination; the server would silently reject anything else.
        #gs_char = selectable_at(dice_cur)
        IF #gs_char = 49 THEN
            #gs_char = (PEEK(SC_KEEP + dice_cur) AND 255)
            IF #gs_char = 49 THEN
                POKE (SC_KEEP + dice_cur), 48
                GOSUB sound_release
            ELSE
                POKE (SC_KEEP + dice_cur), 49
                GOSUB sound_keep
            END IF
            dr_cursor = dice_cur : GOSUB draw_pool_row
        ELSE
            GOSUB sound_invalid
        END IF
        inp_lock = 8
        GOTO ti_loop
    END IF

    ' On ROLL or BANK -- both require at least one die set aside (you must
    ' score something every roll; the server rejects an empty mask).
    ti_any = 0
    pt_i = 0
ti_scan:
    IF pt_i < cur_pool THEN
        IF (PEEK(SC_KEEP + pt_i) AND 255) = 49 THEN ti_any = 1
        pt_i = pt_i + 1
        GOTO ti_scan
    END IF
    IF ti_any = 0 THEN
        GOSUB sound_invalid
        inp_lock = 10
        GOTO ti_loop
    END IF

    ' Capture the mask length NOW, from the pool the player acted on --
    ' compose_url must send exactly len(dice) mask bytes or the server
    ' no-ops the move.
    mask_len = cur_pool
    GOSUB sound_rollbutton
    IF dice_cur = 6 THEN has_action = 2 ELSE has_action = 3
END

' The compiled program overflows IntyBASIC's default $5000-$6FFF
' (8192-word) window -- the compiler happily keeps allocating past $6FFF,
' but $7000 is not in the manual's list of ranges "usable without
' additional programming" on modern flash cart/homebrew PCBs (that list
' jumps from $6FFF to $A000). Everything from here to halt: -- the "cold"
' tail (appkey room handling, in-game menu, help, name entry), none of it
' on the hot per-frame path -- lives in $D000+ instead. This matters
' beyond spec-purity: Fujitzee's FujiNet GO boot loader hung on mount when
' code spilled into $7000, even though jzIntv's emulation is permissive
' enough not to visibly choke on it. Cross-segment GOSUB/GOTO compiles to
' ordinary absolute CP-1610 jumps/calls, so this is a bookkeeping split,
' not a correctness one.
    ASM ORG $D000

' ---------------------------------------------------------------------------
' split_room_url: given a lobby-supplied "https://host/?table=xyz" value
' already sitting in SC_ENDPT with its length in fn_len (as left by
' appkey_read), split it into the bare endpoint (SC_ENDPT, truncated at
' the '?') and the table id (SC_TABLE). Mirrors the C clients'
' welcome-screen '?' scan. Leaves SC_TABLE empty (a leading NUL) if
' there's no '?', the query isn't "table=...", or the id contains
' anything other than A-Z/a-z/0-9 -- it goes straight into a rebuilt
' query string unescaped, same reasoning as the username check at boot.
' ---------------------------------------------------------------------------
split_room_url: PROCEDURE
    POKE SC_TABLE, 0

    sp_found = 255 ' sentinel: not found
    sp_i = 0
    WHILE sp_i < fn_len
        IF (PEEK(SC_ENDPT + sp_i) AND 255) = 63 THEN ' '?'
            sp_found = sp_i
            EXIT WHILE
        END IF
        sp_i = sp_i + 1
    WEND
    IF sp_found = 255 THEN RETURN

    ' Truncate the endpoint at the '?' regardless of what follows.
    POKE (SC_ENDPT + sp_found), 0

    ' Require "table=" (6 bytes) right after the '?'.
    sp_ok = 0
    IF sp_found + 7 <= fn_len THEN
        sp_ok = 1
        IF (PEEK(SC_ENDPT + sp_found + 1) AND 255) <> 116 THEN sp_ok = 0 ' t
        IF (PEEK(SC_ENDPT + sp_found + 2) AND 255) <> 97  THEN sp_ok = 0 ' a
        IF (PEEK(SC_ENDPT + sp_found + 3) AND 255) <> 98  THEN sp_ok = 0 ' b
        IF (PEEK(SC_ENDPT + sp_found + 4) AND 255) <> 108 THEN sp_ok = 0 ' l
        IF (PEEK(SC_ENDPT + sp_found + 5) AND 255) <> 101 THEN sp_ok = 0 ' e
        IF (PEEK(SC_ENDPT + sp_found + 6) AND 255) <> 61  THEN sp_ok = 0 ' =
    END IF
    IF sp_ok = 0 THEN RETURN

    ' Copy the id: up to 8 bytes, stopping at NUL or '&'.
    sp_j = sp_found + 7
    sp_k = 0
    WHILE sp_k < 8 AND sp_j < fn_len
        sp_c = PEEK(SC_ENDPT + sp_j) AND 255
        IF sp_c = 0 OR sp_c = 38 THEN EXIT WHILE ' NUL or '&'
        POKE (SC_TABLE + sp_k), sp_c
        sp_k = sp_k + 1
        sp_j = sp_j + 1
    WEND
    POKE (SC_TABLE + sp_k), 0

    ' Validate: A-Z/a-z/0-9 only.
    sp_valid = 1
    FOR sp_m = 0 TO sp_k - 1
        sp_c = PEEK(SC_TABLE + sp_m) AND 255
        IF (sp_c < 65 OR sp_c > 90) AND (sp_c < 97 OR sp_c > 122) AND (sp_c < 48 OR sp_c > 57) THEN sp_valid = 0
    NEXT sp_m
    IF sp_valid = 0 THEN POKE SC_TABLE, 0
END

' write_room_appkey: persist SC_ENDPT + "?table=" + SC_TABLE back to the
' lobby server appkey slot, so a reboot without going back through the
' lobby rejoins the same room (mirrors the C clients' "Update server app
' key in case of reboot"). Builds the payload directly in FN_TX and writes
' it from there -- appkey_write's byte-by-byte copy from #fn_src into
' FN_TX is a safe no-op when #fn_src is FN_TX itself.
write_room_appkey: PROCEDURE
    #fn_txlen = 0
    #fn_src = SC_ENDPT : ls_max = 65 : GOSUB fn_strlen : GOSUB fn_putstr
    #fn_src = VARPTR lit_qtable(0) : fn_len = 7 : GOSUB fn_putstr
    #fn_src = SC_TABLE : ls_max = 9 : GOSUB fn_strlen : GOSUB fn_putstr

    ak_creator_lo = 1 : ak_creator_hi = 0 : ak_app = 1
    ak_key = AK_LOBBY_KEY_SERVER : ak_mode = 1
    GOSUB appkey_open
    IF fn_ok THEN
        fn_len = #fn_txlen : #fn_src = FN_TX
        GOSUB appkey_write
        GOSUB appkey_close
    END IF
END

' clear_room_appkey: blank the lobby server appkey slot on a deliberate
' leave, so a later reboot lands on table select instead of silently
' rejoining a table the player just quit.
clear_room_appkey: PROCEDURE
    ak_creator_lo = 1 : ak_creator_hi = 0 : ak_app = 1
    ak_key = AK_LOBBY_KEY_SERVER : ak_mode = 1
    GOSUB appkey_open
    IF fn_ok THEN
        fn_len = 0 : #fn_src = FN_TX
        GOSUB appkey_write
        GOSUB appkey_close
    END IF
    POKE SC_TABLE, 0
END

' ===========================================================================
' ingame_menu: keypad CLEAR overlay, drawn over the prompt row and the three
' rows above it. Disc up/down to choose, action button to confirm, Clear
' again cancels straight back to RESUME.
' ===========================================================================
ingame_menu: PROCEDURE
    im_sel = 0
    inp_lock = 0
    ' Wait for the CLEAR press that opened the menu to release before
    ' accepting any input. Without this, the same still-held key
    ' immediately satisfies "IF CONT1.KEY = 10 THEN GOTO im_done" below on
    ' the very first frame, closing the menu the instant it opens -- the
    ' manual's own advice ("it's suggested to wait for CONT1.KEY to
    ' contain 12 before waiting for a key") for exactly this reason.
im_wait_release:
    WAIT
    IF CONT1.KEY <> 12 THEN GOTO im_wait_release

im_loop:
    ' Blank pass and text pass share one frame -- this loop repaints every
    ' frame the menu is up, so a WAIT between the two passes would flash
    ' the blanked rows for a frame on every iteration. 160 cells is well
    ' under the whole-screen (240 cell) burst that motivates splitting
    ' cls_blue across WAITs.
    PRINT AT STATUS_ROW - 60 COLOR COL_TEXT, "                    "
    PRINT AT STATUS_ROW - 40 COLOR COL_TEXT, "                    "
    PRINT AT STATUS_ROW - 20 COLOR COL_TEXT, "                    "
    PRINT AT STATUS_ROW COLOR COL_TEXT, "                    "
    PRINT AT STATUS_ROW - 60 COLOR COL_TEXT, "TABLE MENU"
    #gs_c = COL_TEXT
    IF im_sel = 0 THEN #gs_c = COL_HILITE
    PRINT AT STATUS_ROW - 40 COLOR #gs_c, "RESUME"
    #gs_c = COL_TEXT
    IF im_sel = 1 THEN #gs_c = COL_HILITE
    PRINT AT STATUS_ROW - 20 COLOR #gs_c, "HELP"
    #gs_c = COL_TEXT
    IF im_sel = 2 THEN #gs_c = COL_HILITE
    PRINT AT STATUS_ROW COLOR #gs_c, "QUIT TABLE"

    WAIT
    IF inp_lock > 0 THEN inp_lock = inp_lock - 1 : GOTO im_loop

    ' Wraps rather than clamps, matching every other cursor in the game
    ' (ne_cur, dice_cur, tbl_sel). The "IF im_sel = 0 THEN im_sel = 3"
    ' idiom before decrementing avoids unsigned underflow.
    IF CONT1.DOWN THEN
        im_sel = im_sel + 1
        IF im_sel > 2 THEN im_sel = 0
        inp_lock = 10
        GOSUB sound_cursor
        GOTO im_loop
    END IF
    IF CONT1.UP THEN
        IF im_sel = 0 THEN im_sel = 3
        im_sel = im_sel - 1
        inp_lock = 10
        GOSUB sound_cursor
        GOTO im_loop
    END IF
    IF CONT1.KEY = 10 THEN GOTO im_done
    IF CONT1.BUTTON = 0 THEN GOTO im_loop
    GOSUB sound_select

    IF im_sel = 1 THEN
        ' HELP takes over the whole screen and returns straight to the
        ' game rather than back to this menu -- im_done's restore_screen
        ' below is then the single full repaint the takeover needs, at no
        ' extra cost.
        GOSUB help_screen
        GOTO im_done
    END IF
    IF im_sel = 2 THEN
        url_path = 5 : GOSUB compose_url
        #net_readlen = 8
        GOSUB api_call
        GOSUB clear_room_appkey
        want_leave = 1
    END IF
im_done:
    ' Redraw the whole screen immediately -- not just next poll. See
    ' restore_screen's own comment for why this is needed here.
    IF want_leave = 0 THEN
        GOSUB restore_screen
        ' Don't hand a still-held button/key back to the caller: ti_loop
        ' reads CONT1.BUTTON as keep/roll/bank and gl_input reads BUTTON
        ' as the lobby ready toggle, so confirming RESUME (or dismissing
        ' HELP) with the button/key still down past restore_screen's ~14
        ' frames would otherwise fire an unintended action immediately.
im_exit_release:
        WAIT
        IF CONT1.BUTTON THEN GOTO im_exit_release
        IF CONT1.KEY <> 12 THEN GOTO im_exit_release
    END IF
END

' ===========================================================================
' help_screen: full-screen reference, opened from ingame_menu's HELP item.
' Two pages: controls, then the Fujirkle scoring table (which matches the
' server's scoring.go and won't fit alongside the controls on a 20x12
' screen). Disc left/right flips pages; any side button or any keypad key
' returns to the game (the caller's im_done then does the full repaint).
'
' Blanked one row per WAIT, like cls_blue -- the STIC has no page-flip and
' an unbroken 240-cell BACKTAB write tears.
' ===========================================================================
help_screen: PROCEDURE
    hs_page = 0

hs_draw:
    FOR hs_i = 0 TO 11
        PRINT AT screenpos(0, hs_i) COLOR COL_TEXT, "                    "
        WAIT
    NEXT hs_i

    PRINT AT screenpos(0, 0) COLOR COL_HILITE, "HELP"
    IF hs_page = 0 THEN
        PRINT AT screenpos(17, 0) COLOR COL_LABEL, "1/2"
        WAIT
        PRINT AT screenpos(0, 2) COLOR COL_LABEL, "YOUR TURN"
        PRINT AT screenpos(1, 3) COLOR COL_HILITE, "L / R"
        PRINT AT screenpos(8, 3) COLOR COL_TEXT, "PICK A DIE"
        PRINT AT screenpos(1, 4) COLOR COL_HILITE, "FIRE"
        PRINT AT screenpos(8, 4) COLOR COL_TEXT, "SET DIE ASIDE"
        WAIT
        PRINT AT screenpos(1, 5) COLOR COL_HILITE, "UP"
        PRINT AT screenpos(8, 5) COLOR COL_TEXT, "GO TO ROLL"
        PRINT AT screenpos(1, 6) COLOR COL_HILITE, "DOWN"
        PRINT AT screenpos(8, 6) COLOR COL_TEXT, "GO TO BANK"
        WAIT
        PRINT AT screenpos(0, 8) COLOR COL_LABEL, "ANYTIME"
        PRINT AT screenpos(1, 9) COLOR COL_HILITE, "CLEAR"
        PRINT AT screenpos(8, 9) COLOR COL_TEXT, "TABLE MENU"
        PRINT AT screenpos(0, 10) COLOR COL_TEXT, "LOBBY: FIRE = READY"
        PRINT AT STATUS_ROW COLOR COL_TEXT, "DISC R=MORE BTN=GAME"
    ELSE
        PRINT AT screenpos(17, 0) COLOR COL_LABEL, "2/2"
        WAIT
        PRINT AT screenpos(0, 1) COLOR COL_LABEL, "SCORING"
        PRINT AT screenpos(0, 2) COLOR COL_TEXT, "1=100  5=50"
        PRINT AT screenpos(0, 3) COLOR COL_TEXT, "TRIPLE = FACE X 100"
        PRINT AT screenpos(0, 4) COLOR COL_TEXT, "THREE 1S = 1000"
        WAIT
        PRINT AT screenpos(0, 5) COLOR COL_TEXT, "4K X2  5K X4  6K X8"
        PRINT AT screenpos(0, 6) COLOR COL_TEXT, "STRAIGHT 1-6 = 1500"
        PRINT AT screenpos(0, 7) COLOR COL_TEXT, "THREE PAIRS = 1500"
        WAIT
        PRINT AT screenpos(0, 8) COLOR COL_HILITE, "NO SCORE = FUJIRKLE"
        PRINT AT screenpos(0, 9) COLOR COL_TEXT, "ALL 6 KEPT=HOT DICE"
        PRINT AT screenpos(0, 10) COLOR COL_TEXT, "FIRST TO 10000 WINS"
        PRINT AT STATUS_ROW COLOR COL_TEXT, "DISC L=BACK BTN=GAME"
    END IF

    ' The button press that picked HELP out of the menu is still held here
    ' -- without this the first frame of hs_input would read it as "any
    ' button" and dismiss the screen instantly. Runs again after each page
    ' flip, where it's a harmless no-op (the disc isn't checked here).
hs_release:
    WAIT
    IF CONT1.BUTTON THEN GOTO hs_release
    IF CONT1.KEY <> 12 THEN GOTO hs_release
    inp_lock = 0

hs_input:
    WAIT
    IF inp_lock > 0 THEN inp_lock = inp_lock - 1 : GOTO hs_input
    ' Any of the three side buttons dismisses (CONT1.BUTTON is true for
    ' all three), as does any keypad key.
    IF CONT1.BUTTON THEN GOTO hs_done
    IF CONT1.KEY <> 12 THEN GOTO hs_done
    IF CONT1.RIGHT AND hs_page = 0 THEN
        hs_page = 1 : inp_lock = 10 : GOSUB sound_cursor : GOTO hs_draw
    END IF
    IF CONT1.LEFT AND hs_page = 1 THEN
        hs_page = 0 : inp_lock = 10 : GOSUB sound_cursor : GOTO hs_draw
    END IF
    GOTO hs_input

hs_done:
    GOSUB sound_select
END

' ===========================================================================
' name_entry_screen: disc letter picker, max 8 chars. Disc up/down cycles
' the character under the cursor through A-Z, 0-9, space; left/right moves
' the cursor; the action button accepts (at least 1 non-space char).
' Carried over from fujinet-fujitzee/intv (itself from Battleship).
' ===========================================================================
name_entry_screen: PROCEDURE
    GOSUB cls_blue
    PRINT AT 0 COLOR COL_TEXT, "ENTER YOUR NAME"
    FOR ne_i = 0 TO 7
        ne_buf(ne_i) = 36 ' space
    NEXT ne_i
    ne_cur = 0
    inp_lock = 0

ne_loop:
    FOR ne_i = 0 TO 7
        #gs_c = COL_TEXT
        IF ne_i = ne_cur THEN #gs_c = COL_HILITE
        ne_j = ne_buf(ne_i)
        IF ne_j < 26 THEN
            gs_j = 65 + ne_j
        ELSEIF ne_j < 36 THEN
            gs_j = 48 + ne_j - 26
        ELSE
            gs_j = 95 ' underscore stands in for a visible blank
        END IF
        #BACKTAB(60 + 6 + ne_i) = (gs_j - 32) * 8 + #gs_c
    NEXT ne_i

    WAIT
    IF inp_lock > 0 THEN inp_lock = inp_lock - 1 : GOTO ne_loop

    IF CONT1.RIGHT THEN
        ne_cur = ne_cur + 1
        IF ne_cur > 7 THEN ne_cur = 0
        inp_lock = 8
        GOSUB sound_cursor
        GOTO ne_loop
    END IF
    IF CONT1.LEFT THEN
        IF ne_cur = 0 THEN ne_cur = 8
        ne_cur = ne_cur - 1
        inp_lock = 8
        GOSUB sound_cursor
        GOTO ne_loop
    END IF
    IF CONT1.UP THEN
        ne_buf(ne_cur) = ne_buf(ne_cur) + 1
        IF ne_buf(ne_cur) > 36 THEN ne_buf(ne_cur) = 0
        inp_lock = 6
        GOSUB sound_cursor
        GOTO ne_loop
    END IF
    IF CONT1.DOWN THEN
        IF ne_buf(ne_cur) = 0 THEN ne_buf(ne_cur) = 37
        ne_buf(ne_cur) = ne_buf(ne_cur) - 1
        inp_lock = 6
        GOSUB sound_cursor
        GOTO ne_loop
    END IF
    IF CONT1.BUTTON = 0 THEN GOTO ne_loop
    GOSUB sound_select

    ne_len = 8
    WHILE ne_len > 0 AND ne_buf(ne_len - 1) = 36
        ne_len = ne_len - 1
    WEND
    IF ne_len = 0 THEN GOTO ne_loop

    FOR ne_i = 0 TO ne_len - 1
        ne_j = ne_buf(ne_i)
        IF ne_j < 26 THEN
            gs_j = 65 + ne_j
        ELSE
            gs_j = 48 + ne_j - 26
        END IF
        POKE (SC_NAME + ne_i), gs_j
    NEXT ne_i
    POKE (SC_NAME + ne_len), 0

    ak_creator_lo = 1 : ak_creator_hi = 0 : ak_app = 1 : ak_key = 0 : ak_mode = 1
    GOSUB appkey_open
    IF fn_ok THEN
        #fn_src = SC_NAME : fn_len = ne_len
        GOSUB appkey_write
        IF fn_ok = 0 THEN
            PRINT AT 100 COLOR COL_TEXT, "NAME NOT SAVED FOR "
            PRINT AT 120 COLOR COL_TEXT, "NEXT TIME           "
            inp_lock = 90
            WHILE inp_lock > 0
                inp_lock = inp_lock - 1
                WAIT
            WEND
        END IF
        GOSUB appkey_close
    END IF
END

halt:
    WAIT
    GOTO halt
