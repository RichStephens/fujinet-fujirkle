' state.bas -- binary GameState/Tables wire format, read in place out of FN_RX.
'
' Per the pattern established by fujinet-fujitzee/intv/state.bas (itself from
' fujinet-5cardstud/intv and fujinet-battleship/intv): never copy the reply
' into IntyBASIC variables -- these are just fixed offsets (matching
' server/util.go's serializeResults binary layout, requested with &bin=1)
' plus small DEF FN accessors that PEEK straight out of FN_RX.
'
' GameState layout, always this shape regardless of round (util.go:75-101),
' verified byte-for-byte against a live server dump:
'   0   playerCount (1)
'   1   serverName[21]         lowercased, NUL-padded
'  22   prompt[41]             lowercased, NUL-padded (40 chars max)
'  63   round (1)              0=lobby, 99=gameover, else round number --
'                              UNBOUNDED (no 13-round cap like Fujitzee;
'                              play ends on 10000 points, not a round count)
'  64   activePlayer (signed, $FF = -1; 0 = your turn -- the server rotates
'                              the player array so the requester is index 0)
'  65   moveTime (1, seconds, <=255; can be 250 when solo vs bots)
'  66   viewing (1)            1 = you are a spectator
'  67   validMoves (1)         bitfield MOVE_ROLL=1 | MOVE_BANK=2; in
'                              practice 0 (not your move) or 3
'  68   turnScore (u16 LE)     points held this turn, not yet banked
'  70   dice[7]                pool still in play, ascii '1'-'6' + NUL;
'                              SHRINKS as dice are set aside (6 -> 1)
'  77   keptDice[7]            set aside so far this turn (cleared on hot dice)
'  84   selectable[7]          same length as dice; '1' = this die can be
'                              part of some scoring combination
'  91   keepRoll[7]            echo of the last submitted keep mask
'  98   players[N] -- N = playerCount (can exceed 6; spectators are
'       appended beyond the 6 seated players), 13 bytes each:
'         name[9] alias(1) ready(1: 1=ready, 0=not, $FE=-2 watching)
'         score (u16 LE, live banked total)
'
' /tables -> Tables struct (identical to Fujitzee):
'   0   count (1)
'   then N x 36 bytes: table[9] name[21] players[6] (literal "cur / max")

    CONST GAME_PLAYERCOUNT  = 0
    CONST GAME_SERVERNAME   = 1     ' 21 bytes
    CONST GAME_PROMPT       = 22    ' 41 bytes
    CONST GAME_ROUND        = 63
    CONST GAME_ACTIVEPLAYER = 64
    CONST GAME_MOVETIME     = 65
    CONST GAME_VIEWING      = 66
    CONST GAME_VALIDMOVES   = 67
    CONST GAME_TURNSCORE    = 68    ' u16 LE
    CONST GAME_DICE         = 70    ' 7 bytes
    CONST GAME_KEPT         = 77    ' 7 bytes
    CONST GAME_SELECTABLE   = 84    ' 7 bytes
    CONST GAME_KEEPROLL     = 91    ' 7 bytes
    CONST GAME_PLAYERS      = 98

' Scratch RAM, ours, below fujinet.bas's own SC_* buffers (which stop at
' SC_QUERY $9150 + 48 = $9180) and well below the $9C00 mailbox. Declared
' here (rather than in fujirkle.bas) so dice.bas/board.bas -- included right
' after this file, before fujirkle.bas's own CONST section runs -- can
' reference these without a forward reference.
    CONST SC_TABLE      = $9180 ' selected table id, 9 bytes (8 + NUL)
    CONST SC_KEEP       = $9190 ' 6-char keep mask, ascii '0'/'1', no NUL.
                                ' POLARITY IS THE OPPOSITE OF FUJITZEE:
                                ' '1' = set this die aside to SCORE it (a
                                ' Fujirkle set-aside is permanent, so the
                                ' mask marks what you keep, not what you
                                ' re-roll). Default all '0'.
    CONST SC_PREVDICE   = $91A0 ' 7-byte shadow of the previous poll's dice
                                ' string (roll detection); [0]=0 = unseeded
    CONST SC_PREVSCORES = $91B0 ' 6 x u16 LE shadow of seated players'
                                ' banked totals (bank detection)

' Cross-file client display globals. IntyBASIC auto-registers a bare
' identifier the first time it's mentioned at all (read or write), not
' just on an explicit DIM -- board.bas (included right after this file)
' reads/writes draw_field's df_* parameters before fujirkle.bas's own DIM
' section would otherwise declare them, which turns that later DIM into a
' "variable already defined" error. Declaring them here, ahead of every
' include that touches them, avoids that.
    DIM df_pos, df_len, #df_color, #df_src
    DIM dice_cur, cur_pool

    CONST PLAYER_STRIDE = 13
    CONST PL_NAME   = 0             ' 9 bytes
    CONST PL_ALIAS  = 9             ' 1 byte
    CONST PL_READY  = 10            ' 1 byte: 1=ready, 0=not, $FE=watching
    CONST PL_SCORE  = 11            ' u16 LE

    CONST TABLE_STRIDE = 36
    CONST TBL_ID      = 0           ' 9 bytes
    CONST TBL_NAME    = 9           ' 21 bytes
    CONST TBL_PLAYERS = 30          ' 6 bytes, literal "cur / max"

    ' PLAYER_MAX matches the reference C client's misc.h (spectators can
    ' push playerCount past the 6 seated-player cap); GAME_MAXLEN sizes the
    ' one poll buffer big enough for the worst case, 98 + 12*13 = 254 bytes,
    ' well inside FN_RX's 704-byte window ($9D40-$9FFF).
    CONST PLAYER_MAX     = 12
    CONST GAME_MAXLEN    = GAME_PLAYERS + PLAYER_MAX * PLAYER_STRIDE
    CONST GAME_MINLEN    = GAME_PLAYERS
    CONST TABLES_MAXLEN  = 361

    ' Round values. Fujirkle rounds are unbounded (no ROUND_FINAL): any
    ' value 1..98 is a live round; only 99 is special.
    CONST ROUND_LOBBY    = 0
    CONST ROUND_GAMEOVER = 99

    ' Ready-flag byte values (PL_READY).
    CONST READY_YES     = 1
    CONST READY_NO      = 0
    CONST READY_VIEWING = 254       ' wire $FE = -2 signed

    ' validMoves bits.
    CONST MOVE_ROLL = 1
    CONST MOVE_BANK = 2

' player_addr(i): address of player i's 13-byte record in FN_RX.
    DEF FN player_addr(i) = FN_RX + GAME_PLAYERS + i * PLAYER_STRIDE

' table_addr(i): address of table i's 36-byte record in FN_RX (i is 0-based,
' the record right after the leading count byte).
    DEF FN table_addr(i) = FN_RX + 1 + i * TABLE_STRIDE

' score_of(addr): player record at `addr`'s banked total as an unsigned
' 16-bit word (little-endian byte pair recombined by hand -- we do not
' request &be=1).
    DEF FN score_of(addr) = (PEEK(addr + PL_SCORE + 1) AND 255) * 256 + (PEEK(addr + PL_SCORE) AND 255)

' ready_of(addr): player record at `addr`'s ready byte (READY_YES /
' READY_NO / READY_VIEWING).
    DEF FN ready_of(addr) = (PEEK(addr + PL_READY) AND 255)

' turn_score: points held this turn (u16 LE at GAME_TURNSCORE).
    DEF FN turn_score = (PEEK(FN_RX + GAME_TURNSCORE + 1) AND 255) * 256 + (PEEK(FN_RX + GAME_TURNSCORE) AND 255)

' active_player: activePlayer as a signed value (-1..11), converting the
' wire's $FF sentinel. Called as plain `active_player` (no parens -- DEF FN
' with no arguments is defined and invoked without them).
    DEF FN active_player = ((PEEK(FN_RX + GAME_ACTIVEPLAYER) AND 255) = 255) * -256 + (PEEK(FN_RX + GAME_ACTIVEPLAYER) AND 255)

' die_at/kept_at/selectable_at(i): raw wire bytes (ascii '1'-'6', '0'/'1',
' or NUL past the end of the string).
    DEF FN die_at(i)        = (PEEK(FN_RX + GAME_DICE + i) AND 255)
    DEF FN kept_at(i)       = (PEEK(FN_RX + GAME_KEPT + i) AND 255)
    DEF FN selectable_at(i) = (PEEK(FN_RX + GAME_SELECTABLE + i) AND 255)

' state_* byte reads, broken out as DEF FNs purely so call sites read as
' intent, not offsets.
'
' Parenthesized "(... AND 255)" deliberately, not left bare. IntyBASIC's
' "=" binds *tighter* than "AND" (same trap as C's "a & b == c"), so an
' unparenthesized "PEEK(x) AND 255" substituted into "state_viewing = 0"
' compiles as "PEEK(x) AND (255 = 0)" -- the 255 gets grouped with the
' comparison instead of the AND, folding to "PEEK(x) AND 0", which is
' always zero regardless of what was actually peeked. Confirmed in the
' Fujitzee port by reading the generated .lst: turn_input was never
' reached at all until every DEF FN body here was parenthesized.
    DEF FN state_round       = (PEEK(FN_RX + GAME_ROUND) AND 255)
    DEF FN state_playercount = (PEEK(FN_RX + GAME_PLAYERCOUNT) AND 255)
    DEF FN state_viewing     = (PEEK(FN_RX + GAME_VIEWING) AND 255)
    DEF FN state_validmoves  = (PEEK(FN_RX + GAME_VALIDMOVES) AND 255)
    DEF FN state_movetime    = (PEEK(FN_RX + GAME_MOVETIME) AND 255)

' ---------------------------------------------------------------------------
' validate_state: layered sanity check on a /state, /ready, /roll, or /bank
' reply already sitting in FN_RX (#net_gotlen bytes). FN_RX is never cleared
' between calls, so a short read leaves stale bytes from a previous, larger
' response -- fixed-offset reads would happily render that as garbage. Sets
' fn_ok = 0 (in addition to whatever net/transact left it) if any gate
' fails; callers must check fn_ok after calling this, not just after
' api_call.
' ---------------------------------------------------------------------------
DIM vs_round, vs_pc, #vs_expect
validate_state: PROCEDURE
    IF fn_ok = 0 OR #net_gotlen < GAME_MINLEN THEN
        fn_ok = 0
        RETURN
    END IF

    vs_pc = state_playercount
    IF vs_pc > PLAYER_MAX THEN
        fn_ok = 0
        RETURN
    END IF

    ' Rounds run 0..N (unbounded) plus 99; the byte can't legitimately
    ' exceed 99, so 100-255 means we're looking at stale/garbage bytes.
    vs_round = state_round
    IF vs_round > ROUND_GAMEOVER THEN
        fn_ok = 0
        RETURN
    END IF

    #vs_expect = GAME_PLAYERS + vs_pc * PLAYER_STRIDE
    IF #net_gotlen < #vs_expect THEN fn_ok = 0
END
