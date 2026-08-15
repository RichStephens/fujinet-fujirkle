' board.bas -- play-screen board pieces shared with the lobby/gameover
' screens: page colors, per-seat player colors, the static board labels,
' and the name+score player list. Replaces fujinet-fujitzee/intv/card.bas
' -- Fujirkle has no per-category scorecard, so the whole 6-player list
' with live banked totals fits on screen permanently and there is no
' paging, standings overlay, or category grid.
'
' Play screen layout (20x12 BACKTAB, MODE 1):
'   row0    server/table name, round number, move timer
'   row1-6  player list: name (9) + banked score (5 digits, col 14)
'   row7    KEPT label, up to 6 set-aside dice, +turn score
'   row8    (blank)
'   row9    pool dice (up to 6, cols 0-5), ROLL and BANK buttons
'   row10   server prompt, first 20 chars
'   row11   server prompt, chars 21-40 (one draw_field call spanning both)

    ' Page background is blue (cls_blue in fujirkle.bas fills every CLS'd
    ' screen with this), so every color below pairs with BG_BLUE instead
    ' of BG_BLACK -- anything still paired with black would sit on an
    ' invisible matching square rather than actually showing a black box.
    CONST COL_TEXT     = FG_WHITE + BG_BLUE
    CONST COL_LABEL    = FG_TAN + BG_BLUE
    CONST COL_HILITE   = FG_YELLOW + BG_BLUE
    CONST COL_ACTIVE   = FG_BLACK + BG_YELLOW
    ' MODE 1's foreground palette is fixed at 8 colors (no orange) --
    ' orange only exists as one of the 16 background shades. So "orange
    ' text" for the round indicator is rendered as black-on-orange
    ' instead of a foreground color.
    CONST COL_ROUND    = FG_BLACK + BG_ORANGE

    CONST KEPT_ROW   = 7
    CONST POOL_ROW   = 9
    CONST SCORE_COL  = 14

' ---------------------------------------------------------------------------
' player_color_tbl / player_color: a distinct FG color per seat (0-5),
' used everywhere a player's name is drawn (player list, lobby, gameover)
' so a given player reads consistently across every screen. FG_BLUE is
' deliberately excluded -- it would be invisible on the page's own blue
' background. Only 6 colors are available without reusing COL_TEXT/
' COL_LABEL, matching the server's 6 seats; entries beyond that
' (spectators, in the lobby list which shows up to 8) fall back to plain
' COL_TEXT.
' ---------------------------------------------------------------------------
player_color_tbl: DATA FG_RED, FG_TAN, FG_DARKGREEN, FG_GREEN, FG_YELLOW, FG_WHITE

DIM pc_idx, #pc_color
player_color: PROCEDURE
    IF pc_idx < 6 THEN
        #pc_color = player_color_tbl(pc_idx) + BG_BLUE
    ELSE
        #pc_color = COL_TEXT
    END IF
END

' ---------------------------------------------------------------------------
' board_init: draw the static labels once, on the transition into play.
' Everything else on the board is repainted per poll.
' ---------------------------------------------------------------------------
board_init: PROCEDURE
    PRINT AT screenpos(0, KEPT_ROW) COLOR COL_LABEL, "KEPT"
END

' ---------------------------------------------------------------------------
' render_player_list: name + banked total for up to 6 seated players,
' starting at BACKTAB row prl_top, each row in that player's own color --
' except the active player, whose name is highlighted black-on-yellow so
' whose turn it is reads at a glance. Rows past playerCount are blanked
' (a departing player would otherwise leave a stale row -- there is no
' per-poll CLS). Shared by the play screen (prl_top=1) and render_gameover
' (active_player is -1 there, so no row highlights).
'
' Fujirkle's wire score (PL_SCORE) is the live banked total, so it is
' printed directly -- no running-total computation like Fujitzee's
' scorecard needed. #prl_addr must be 16-bit: player_addr() is
' FN_RX-relative and FN_RX sits well above byte range ($9D40+), so an
' 8-bit holder would truncate the pointer and PEEK garbage (the root
' cause of Fujitzee's long-standing blank-names bug).
' ---------------------------------------------------------------------------
DIM prl_top, prl_i, #prl_addr, prl_pc, #prl_score
render_player_list: PROCEDURE
    prl_pc = state_playercount
    IF prl_pc > 6 THEN prl_pc = 6
    FOR prl_i = 0 TO 5
        IF prl_i < prl_pc THEN
            #prl_addr = player_addr(prl_i)
            IF prl_i = active_player THEN
                #pc_color = COL_ACTIVE
            ELSE
                pc_idx = prl_i
                GOSUB player_color
            END IF
            #df_src = #prl_addr + PL_NAME : df_pos = screenpos(0, prl_top + prl_i) : df_len = 9 : #df_color = #pc_color
            GOSUB draw_field
            #prl_score = score_of(#prl_addr)
            PRINT AT screenpos(SCORE_COL, prl_top + prl_i) COLOR #pc_color, <.5>#prl_score
        ELSE
            PRINT AT screenpos(0, prl_top + prl_i) COLOR COL_TEXT, "         "
            PRINT AT screenpos(SCORE_COL, prl_top + prl_i) COLOR COL_TEXT, "     "
        END IF
    NEXT prl_i
END
