# ece4760finalproject
Our ECE 4760 final project is a two player version of a popular game called Simon Says but in this version we are calling it Pico Says. The rationale behind this project was taking a familiar game that does not have a technological piece to it and applying what we learned this semester to it to stretch and grow our understanding. The high level explanation of the game is that there will be two people playing with two separate Raspberry Pi Pico W boards that are connected by Wi-Fi. On each breadboard will be 4 buttons and 4 LEDs corresponding to each button. Sequentially each button/LED makes a distinct sound. The game logic is that Player 1 pushes a sequences of buttons corresponding to LEDs lighting up. That information is sent to Player 2 who has to remember what the sequence is. Then Player 2 gives a sequence to Player 1 and them go back and forth. If a player makes a mistake recalling the correct sequence, all 4 LEDS light up and the game ends. Also there is a 15 second limit to recall the sequence or else all the LEDs light up and the game ends. To change the diffcultly, there will be a user interface to select how many entries can be in each sequence. Either, 4, 6, or 8 entires can selected as the more entries to recall the harder the game becomes. We are planning to use UPD as the com,incation protocol between the two boards even though there is very little error checking.