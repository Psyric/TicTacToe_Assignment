BROKER="34.30.214.222;"
MOVE_TOPIC="tictactoe/move"

row=$(( RANDOM % 3 ))
col=$(( RANDOM % 3 ))
moveMsg="P2:${row},${col}"

mosquitto_pub -h "$BROKER" -t "$MOVE_TOPIC" -m "$moveMsg"
echo "Published automated move for Player 2: $moveMsg"



