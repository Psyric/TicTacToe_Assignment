BROKER="34.30.214.222"
MODE_TOPIC="tictactoe/mode"
MOVE_TOPIC="tictactoe/move"
BOARD_TOPIC="tictactoe/board"
RESULT_TOPIC="tictactoe/result"
LCDRESET_TOPIC="tictactoe/lcdreset"

BOARD_TIMEOUT=1

trap "mosquitto_pub -h \$BROKER -t \$LCDRESET_TOPIC -m 'reset'; echo 'Sent LCD reset command'; exit" SIGINT SIGTERM EXIT
echo "==== Tic Tac Toe Game Mode Selection ===="
echo "1) 1 Player Mode (Player 1 manual; Player 2 bot)"
echo "2) 2 Player Mode (Both manual)"
echo "3) Automated Mode (Arduino bot vs. external bot)"
read -p "Enter your choice (1-3): " choice
# Publish the chosen mode.
mosquitto_pub -h "$BROKER" -t "$MODE_TOPIC" -m "$choice"
echo "Published game mode $choice to MQTT."
# Launch external bot for Player 2 if required.
if [[ "$choice" -eq 1 || "$choice" -eq 3 ]]; then
    echo "Launching external automated bot for Player 2..."
    nohup ./player2.sh > /dev/null 2>&1 &
fi
if [ "$choice" -eq 1 ]; then
    echo "Entering Mode 1 interactive mode (Player 1 moves only)."
    while true; do
        BOARD=$(timeout ${BOARD_TIMEOUT} mosquitto_sub -h "$BROKER" -t "$BOARD_TOPIC" -C 1)
        clear
        echo "====== Current Board State ======"
        echo "$BOARD"
        echo "=================================="
        RESULT=$(timeout ${BOARD_TIMEOUT} mosquitto_sub -h "$BROKER" -t "$RESULT_TOPIC" -C 1 2>/dev/null)
        if [ -n "$RESULT" ]; then
            echo "Game Result: $RESULT"
            echo "Press Enter to start a new game."
            read
            break
        fi
        echo "It is Player 1's turn."
        read -p "Enter your move (format: P1:row,col) or 'exit' to quit: " move
        if [ "$move" = "exit" ]; then
            echo "Exiting..."
            exit 0
        fi
        if [[ $move != P1:* ]]; then
            echo "Invalid move format. It must start with 'P1:'"
            sleep 1
            continue
        fi
        mosquitto_pub -h "$BROKER" -t "$MOVE_TOPIC" -m "$move"
        echo "Published move: $move"
        sleep 2
        BOARD=$(timeout ${BOARD_TIMEOUT} mosquitto_sub -h "$BROKER" -t "$BOARD_TOPIC" -C 1)
        clear
        echo "====== Updated Board State ======"
        echo "$BOARD"
        echo "=================================="
        RESULT=$(timeout ${BOARD_TIMEOUT} mosquitto_sub -h "$BROKER" -t "$RESULT_TOPIC" -C 1 2>/dev/null)
        if [ -n "$RESULT" ]; then
            echo "Game Result: $RESULT"
            echo "Press Enter to start a new game."
            read
            break
        fi
        echo "Press Enter to make your next move."
        read
    done
elif [ "$choice" -eq 2 ]; then
    echo "Entering Mode 2 interactive mode (Players alternate)."
    currentTurn=1
    while true; do
        BOARD=$(timeout ${BOARD_TIMEOUT} mosquitto_sub -h "$BROKER" -t "$BOARD_TOPIC" -C 1)
        clear
        echo "====== Current Board State ======"
        echo "$BOARD"
        echo "=================================="
        RESULT=$(timeout ${BOARD_TIMEOUT} mosquitto_sub -h "$BROKER" -t "$RESULT_TOPIC" -C 1 2>/dev/null)
        if [ -n "$RESULT" ]; then
            echo "Game Result: $RESULT"
            echo "Press Enter to start a new game."
            read
            break
        fi
        echo "It is Player $currentTurn's turn."
        read -p "Enter your move (format: P${currentTurn}:row,col) or 'exit' to quit: " move
        if [ "$move" = "exit" ]; then
            echo "Exiting..."
            exit 0
        fi
        expectedPrefix="P${currentTurn}:"
        if [[ $move != $expectedPrefix* ]]; then
            echo "Invalid move. It must start with '$expectedPrefix'."
            sleep 1
            continue
        fi
        mosquitto_pub -h "$BROKER" -t "$MOVE_TOPIC" -m "$move"
        echo "Published move: $move"
        sleep 1
        BOARD=$(timeout ${BOARD_TIMEOUT} mosquitto_sub -h "$BROKER" -t "$BOARD_TOPIC" -C 1)
        clear
        echo "====== Updated Board State ======"
        echo "$BOARD"
        echo "=================================="
        sleep 0.5
        currentTurn=$(( currentTurn == 1 ? 2 : 1 ))
    done
elif [ "$choice" -eq 3 ]; then
    echo "Mode 3 selected: Displaying board updates continuously. Press CTRL+C to exit."
    mosquitto_sub -h "$BROKER" -t "$BOARD_TOPIC"
fi
killall mosquitto_sub 2>/dev/null



