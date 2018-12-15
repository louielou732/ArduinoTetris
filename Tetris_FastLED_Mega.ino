#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#ifndef PSTR
 #define PSTR // Make Arduino Due happy
#endif

#include <FastLED.h>
#define NUM_LEDS 540
#define DATA_PIN 4

#include  <SPI.h>
#include "RF24.h"
#include "nRF24L01.h"

// array position for msg
const int JOY_UP = 8;
const int JOY_RIGHT = 2;
const int JOY_DOWN = 5;
const int JOY_LEFT = 3;
const int A_BTN = 4;
const int B_BTN = 1;
const int START_BTN = 6;
const int SELECT_BTN = 7;

//block type
const int SQUARE = 0;
const int LONG = 1;
const int JSHAPED = 2;
const int ZSHAPED = 3;
const int LSHAPED = 4;
const int SSHAPED = 5;
const int TSHAPED = 6;

// vars to hold current block type/color and next block type/color
int current_block, next_block;
int current_color, next_color;

int state = 0;

// move direction
const int MOVE_LEFT = 0;
const int MOVE_RIGHT = 1;

int block_type;

int joystick_input = 0;
int button_input = 0;

boolean game_over = false;

int level = 0;
int score = 0;
int lines_soft_dropped = 0;
int lines_to_next_level = 10;
int lines_cleared = 0;

// set up controller
int msg[2];
RF24 radio(7, 8);
const uint64_t pipe = 0xE8E8F0F0E1LL;

// set up LED matrix
CRGB leds[NUM_LEDS];

// three colors (red, blue, green)
const CRGB colors[] = { CRGB::Black,
  CRGB::Red, CRGB::Blue, CRGB::Green };

int color = 0;

unsigned long current_time = millis();
unsigned long last_time = millis();
unsigned long last_button_time = millis();
unsigned long last_flip_time = millis();
unsigned long last_move_time = millis();
int block_speed = 300;
int check_button = 50;
int flip_speed = 150;
int move_speed = 10;

int max_x = 10;
int max_y = 20;

int pos[4][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};

// a 10 x 20 array to hold the current status of each pixel on the gameboard
int board[10][20];

// array that will convert each pixel position from the 2-D gameboard to the 1-D FastLED array
int led_pos[10][20];

// this will hold the starting pixel number in the FastLED array for the scoreboard
int scoreboard_start;

// a 3 x 3 array to hold the FastLED led pixel position for the next peice preview
int preview_leds[3][4];

// boolean arrays for digits 0 through 9
// they indicate which LEDs will be lit to display a particular number
boolean digits[10][15] = { {true, true, true, true, true, true, false, false, false, true, true, true, true, true, true}, 
                       {true, false, false, true, false, true, true, true, true, true, true, false, false, false, false},
                       {true, true, false, false, true, true, false, true, false, true, true, false, false, true, true},
                       {true, false, false, false, true, true, false, true, false, true, true, true, true, true, true},
                       {false, false, true, true, true, false, false, true, false, false, true, true, true, true, true},
                       {true, false, false, true, true, true, false, true, false, true, true, true, false, false, true},
                       {true, true, true, true, true, true, false, true, false, true, true, true, true, false, true},
                       {false, false, false, false, true, true, false, true, true, true, false, false, false, true, true},
                       {true, true, true, true, true, true, false, true, false, true, true, true, true, true, true}, 
                       {false, false, true, true, true, true, false, true, false, false, true, true, true, true, true} };

boolean paused = false;

void setup() 
{

  int led_num = 0;

  // set up the FastLED lib
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);

  // set up the 2.4 GHz radio
  Serial.begin(9600);
  radio.begin();
  radio.openReadingPipe(1,pipe);
  radio.startListening();

  // set up the led_pos 2-D array which will translate the pixels grid position
  // to the FastLED strip position
  // led_pos will be given the (x, y) coordinates and that value there will be the pixels actual position in the strip
  // since each "pixel box" will contain two acutal WS2812B LEDs we will just put the pixel number of the first one
  for(int i = 0; i < max_x; i++)
  {
    // since the LEDs are set up in zig zag pattern (the lowest numbered pixel in each even column [0,2,4..] is at the bottom, in odd columns it is at the top)
    // we need to check if we are in an even or odd column to make sure we label the pixels correctly
    
    if (i % 2 != 0)
    {
      // if we are in an odd column we start at the bottom and move up the column while increasing the led_num
      for(int j = 0; j < max_y; j++)
      {
        led_pos[i][j] = led_num;
        led_num += 2;
      }
    }
    else
    {
      // if we are in an odd column we start at the top and move downn the column while increasing the led_num
      for(int j =  max_y-1; j >= 0; j--)
      {
        led_pos[i][j] = led_num;
        led_num += 2;
      }
    }
  }

  // set the scoreboard's first LED
  scoreboard_start = led_num;
  
  // set up the 3 x 3 array that holds the FastLED position of the next peice preview
  int preview_led = scoreboard_start + 6 * 15;    // 6 digit, 15 pixels per digit for scoreboard
  for(int i = 0; i < 3; i++)
  {
    if (i % 2 != 0)
    {  for(int j = 0; j < 4; j++)
        preview_leds[i][j] = preview_led;
    }
    else
    {
      for(int j = 4; j >= 0; j--)
        preview_leds[i][j] = preview_led;
    }

    preview_led++;  
  }

  for(int i = 0; i < max_x; i++)
  {
    for(int j = 0; j < max_y; j++)
    {
      leds[led_pos[i][j]] = colors[1];
      leds[led_pos[i][j]+1] = colors[1];
    }

    FastLED.show();
  }

  // turn off the LEDs and zero-out the board array
  for(int i = 0; i < max_x; i++)
   for(int j = 0; j < max_y; j++)
    {
      leds[led_pos[i][j]] = colors[0];
      leds[led_pos[i][j]+1] = colors[0];
      FastLED.show();

      board[i][j] = 0;
    }
    
  delay(200);

  // seed the random number generator
  randomSeed(analogRead(14));

  // get the first block/color and set as the next_block
  // then call the next_block function to make it the current_block and start the game
  next_block = random(7);
  next_color = random(3)+1;
  
  new_block();
}

void loop() 
{

  boolean drop_faster = false;
  current_time = millis();
  
    //Serial.println("inside if\n");    
   if (radio.available())
   {
     //Serial.println("inside while\n");

     // get the input from the controller by reading the data sent to the radio
     radio.read(msg, 4);
     joystick_input = msg[0];
     button_input = msg[1];
           
     //Serial.print("Player: ");
     //Serial.println(msg[0]);
  
    Serial.print("Button: ");
    Serial.println(msg[1]);

    // check to see if the game is paused
    // and skip the part that checks for the game play buttons
    if (!paused)
    {
         if (button_input == JOY_LEFT && ((current_time - last_move_time) >= flip_speed))
         {
          move_block(MOVE_RIGHT);
          radio.flush_rx();
    
          last_move_time = current_time;
         }
         else if (button_input == JOY_RIGHT && ((current_time - last_move_time) >= flip_speed))
          {
            move_block(MOVE_LEFT);
            radio.flush_rx();
    
           last_move_time = millis();
          }
    
         if ((button_input) == A_BTN && ((current_time - last_flip_time) >= flip_speed))
         {
          rotate();
          last_flip_time = millis();
          
          radio.flush_rx();
         }
    
         if (button_input == JOY_DOWN)
         {
          drop_blocks();
          radio.flush_rx();
         }

         if (button_input == START_BTN)
            paused = true;

          // drop the blocks down based on the current block_speed setting
          if ((current_time - last_time) >= block_speed)
            {
              drop_blocks();
              last_time = millis();
            }
    }
    else
    {
      if (button_input == START_BTN)
       {
        paused = false;

        radio.flush_rx();
       }
    }
    
   }
  
  update_board();

  if (game_over)
    restart_game();
}

// function to check for full rows, add to the score, and clear them
void check_and_clear_rows()
{
  Serial.println("enter clear rows");
  boolean clear_row[max_y];
  boolean all_full = false;
  boolean rows_need_clearing = false;
  lines_cleared = 0;

  // check for full rows
  // starting at the bottom, checking each row
  for(int y=0; y < max_y; y++)
  {
    // assume the current row (row y) is full
    all_full = true;

    // perform an AND on all_full and the current pixel at (x, y)
    // if an x position in the current row is NOT EQUAL to 0 than
    // the AND operation will set all_full = false
    for(int x=0; x < max_x; x++)
      all_full = all_full && (board[x][y] != 0);

    // the clear row array will tell us which row to clear
    // the current rows all_full boolean value is set here
    clear_row[y] = all_full;

    // if the row is all_full add to the number lines_cleared
    if (all_full)
      lines_cleared++;
  }

  // lines_cleared is GREATER THAN 0 than we need do three things:
  // 1) add to score (and advance to the next level if enough lines were cleared for the current level)
  // 2) flash the lines
  // 3) move the blocked above the cleared lines down
  if (lines_cleared > 0)
  {
    // the points added to score are based on the number of lines cleared with the current block
    // and the current level the player is on
    switch(lines_cleared)
    {
      case 1:
              score += (level+1)*40;
              break;
      case 2:
              score += (level+1)*100;
              break;
      case 3:
              score += (level+1)*300;
              break;
      case 4:
              score += (level+1)*1200;
              break;
      default:
              break;
    }

    // subtract the lines cleared from the number of lines that need to be cleared to reach the next level,
    // check if this value is less than or eaqual to zero and if so advance to the next level and reset the lines_to_next_level count
    lines_to_next_level -= lines_cleared;
    if (lines_to_next_level <= 0)
    {
      level++;
      lines_to_next_level = 0;
    }

    // flash the lines that are going to be cleared
    for(int i=0 ; i<2;i++)
    {
      // loop through each row
      for(int y=0; y<max_y; y++)
      {
        // check if its clear_row value is true
        if (clear_row[y])
        {
          // if so we are going to flash it white
          for(int i = 0; i < max_x; i++)
          {
            leds[led_pos[i][y]] = CRGB::White;
            leds[led_pos[i][y]+1] = colors[0];   
          }
        }
      }
      FastLED.show();
  
      delay(10);

      // loop through each row
      for(int y=0; y<max_y; y++)
      {
        // check if its clear_row value is true
        if (clear_row[y])
        {
          // turn off the LEDs for that row
          for(int i = 0; i < max_x; i++)
          {
            leds[led_pos[i][y]] = CRGB::Black;
            leds[led_pos[i][y]+1] = CRGB::Black;
          }
        }
      
        FastLED.show();
        delay(10);    
      }
      
    } // end loop that flashes the line of LEDs

    // clear the completed lines off the board and move the blocks above down
    for(int y = max_y-1; y >= 0; y--)
    {
      while(clear_row[y])
      {
        if(y == 0)
        {  
          for (int x = 0; x < max_x; x++)
            board[x][0] = 0;
        }
        else
        {
          for(int i = y; i >= 1; i--)
          {
            for (int x = 0; x < max_x; x++)
              board[x][i] = board[x][i-1];
            
            clear_row[i] = clear_row[i-1];
          }
        }
      }
    }
    
  } // end statements that deals with completed lines

  // update the scoreboard
  update_score(score);
 
  Serial.println("exit clear rows");
}

// function to update the LEDs on the board
// it reads the 2-D board array to decide how the LED at that (x, y) position lit up.
// the board array holds the 0,1,2, and 3 for the off, Red, Green, and Blue respectively.
// the led array is a 1-D array used by the FastLED library to control the LEDs.
// the led_pos array is a 2-D array that translates the 2-D position of an LED on the board into the 1-D array needed for FastLED.
void update_board()
{
  // loop through the 2-D array
  for(int i = 0; i < max_x; i++)
  {
    for(int j = 0; j < max_y; j++)
     {
      // set the LED at (i, j) to its value (off, red, green, or blue)
      leds[led_pos[i][j]] = colors[board[i][j]];
      leds[led_pos[i][j]+1] = colors[board[i][j]];
     }

  }
  
  // show the updated board      
  FastLED.show();
}

// function to update the scoreboard
// it will take the current score as an argumement and update the LEDs
void update_score(int score)
{
  int temp_score = score, i,j, current_digit;   
  int score_digits[6];      // this array will hold the digits of the score
  
  // take the score and place each digit into the score_digits array
  for(i = 0; i < 6; i++)
  {
    // use mod 10 to get the most left hand digit
    // then divide by 10 to move to the next digit
    current_digit = temp_score % 10;
    temp_score /= 10;

    // each scoreboard digit uses 15 LEDs
    // based on the current_digit we index in a 2-D boolean array to see which LEDs are lit up
    // NOTE: the left most digit position in the score starts at scoreboard_start but we start at the right most digit,
    // so we need use (5-i) to make sure the right digit is shown in the right position
    for (j = 0; j < 15; j++)
      leds[scoreboard_start + 15 * (5-i) + j] = digits[current_digit] ? colors[2] : colors[0];
  }

  FastLED.show();
}

// function to create a new block
// it will make the next_block the current_block and then randomly choose a next_block
void new_block()
{
  Serial.println("enter new block");

  // set the current_block to the next_block
  current_block = next_block;
  current_color = next_color;

  // get the next_block
  next_block = random(7);
  next_color = random(3)+1;
  
  int start_pos = 0;
  state = 0;

  //Serial.println(type);

  // based on which block is chosen for current_block
  // we will check to make the sure block can be successfully placed on the board
  // the 4 x 2 pos array will hold the (x, y) coordinates for each of the four sub-blocks that make the larger game block
  // the board grid in the proper positions to hold the block color
  switch(current_block)
  {
    case SQUARE:                       //square
      start_pos = random(8);
      game_over = (board[start_pos][0] != 0) || (board[start_pos][1] != 0) || (board[start_pos+1][0] != 0) || (board[start_pos+1][1] != 0);
      
      pos[0][0] = start_pos; pos[0][1] = 0; // top left
      board[start_pos][0] = current_color;
      
      pos[1][0] = start_pos; pos[1][1] = 1; // bottom left
      board[start_pos][1] = current_color;
      
      pos[2][0] = start_pos+1; pos[2][1] = 0; // top right
      board[start_pos+1][0] = current_color;
      
      pos[3][0] = start_pos+1; pos[3][1] = 1; // bottom right
      board[start_pos+1][1] = current_color;
      block_type = SQUARE;
      break;
      
    case LONG:                     // long
      start_pos = random(5);
      game_over = (board[start_pos][0] != 0) || (board[start_pos+1][0] != 0) || (board[start_pos+2][0] != 0) || (board[start_pos+3][0] != 0);
      
      pos[0][0] = start_pos; pos[0][1] = 0;
      board[start_pos][0] = current_color;
      
      pos[1][0] = start_pos+1; pos[1][1] = 0;
      board[start_pos+1][0] = current_color;
      
      pos[2][0] = start_pos+2; pos[2][1] = 0;
      board[start_pos+2][0] = current_color;
      
      pos[3][0] = start_pos+3; pos[3][1] = 0;
      board[start_pos+3][0] = current_color;
      block_type = LONG;
      break;

    case JSHAPED:                     // j shape
      start_pos = random(6);
      game_over = (board[start_pos][0] != 0) || (board[start_pos+1][0] != 0) || (board[start_pos+2][0] != 0) || (board[start_pos+2][1] != 0); 
      
      pos[0][0] = start_pos; pos[0][1] = 0;
      board[start_pos][0] = current_color;
      
      pos[1][0] = start_pos+1; pos[1][1] = 0;
      board[start_pos+1][0] = current_color;
      
      pos[2][0] = start_pos+2; pos[2][1] = 0;
      board[start_pos+2][0] = current_color;
      
      pos[3][0] = start_pos+2; pos[3][1] = 1;
      board[start_pos+2][1] = current_color;
      block_type = JSHAPED;
      break;
    
    case ZSHAPED:                     // z shape
      start_pos = random(5);
      game_over = (board[start_pos][0] != 0) || (board[start_pos+1][0] != 0) || (board[start_pos+1][1] != 0) || (board[start_pos+2][1] != 0); 
      
      pos[0][0] = start_pos; pos[0][1] = 0;
      board[start_pos][0] = current_color;
      
      pos[1][0] = start_pos+1; pos[1][1] = 0;
      board[start_pos+1][0] = current_color;
      
      pos[2][0] = start_pos+1; pos[2][1] = 1;
      board[start_pos+1][1] = current_color;
      
      pos[3][0] = start_pos+2; pos[3][1] = 1;
      board[start_pos+2][1] = current_color;
      block_type = ZSHAPED;
      break;

    case LSHAPED:                 // L shaped
      start_pos = random(6);
      game_over = (board[start_pos][0] != 0) || (board[start_pos+1][0] != 0) || (board[start_pos+2][0] != 0) || (board[start_pos][1] != 0); 
      
      pos[0][0] = start_pos; pos[0][1] = 0;
      board[start_pos][0] = current_color;
      
      pos[1][0] = start_pos+1; pos[1][1] = 0;
      board[start_pos+1][0] = current_color;
      
      pos[2][0] = start_pos+2; pos[2][1] = 0;
      board[start_pos+2][0] = current_color;
      
      pos[3][0] = start_pos; pos[3][1] = 1;
      board[start_pos][1] = current_color;
      block_type = LSHAPED;
      break;

    case SSHAPED:                // sshaped
      start_pos = random(5);
      game_over = (board[start_pos][1] != 0) || (board[start_pos+1][1] != 0) || (board[start_pos+1][0] != 0) || (board[start_pos+2][0] != 0); 
      
      pos[0][0] = start_pos; pos[0][1] = 1;
      board[start_pos][1] = current_color;
      
      pos[1][0] = start_pos+1; pos[1][1] = 1;
      board[start_pos+1][1] = current_color;
      
      pos[2][0] = start_pos+1; pos[2][1] = 0;
      board[start_pos+1][0] = current_color;
      
      pos[3][0] = start_pos+2; pos[3][1] = 0;
      board[start_pos+2][0] = current_color;
      block_type = SSHAPED;
      break;

    case TSHAPED:              // t shaped
      start_pos = random(5);
      game_over = (board[start_pos][0] != 0) || (board[start_pos+1][0] != 0) || (board[start_pos+2][0] != 0) || (board[start_pos+1][1] != 0); 
      
      pos[0][0] = start_pos; pos[0][1] = 0;
      board[start_pos][0] = current_color;
      
      pos[1][0] = start_pos+1; pos[1][1] = 0;
      board[start_pos+1][0] = current_color;
      
      pos[2][0] = start_pos+2; pos[2][1] = 0;
      board[start_pos+2][0] = current_color;
      
      pos[3][0] = start_pos+1; pos[3][1] = 1;
      board[start_pos+1][1] = current_color;
      block_type = TSHAPED;
      break;
  }

  Serial.println("exit new block");
}

// function to update the preview of the next block
void update_preview()
{
  switch(next_block)
  {
    case SQUARE:                                  //square
      leds[preview_leds[0][0]] = current_color;
      leds[preview_leds[0][1]] = current_color;
      leds[preview_leds[1][0]] = current_color;
      leds[preview_leds[1][1]] = current_color;
      break;
      
    case LONG:                     // long
      leds[preview_leds[1][0]] = current_color;
      leds[preview_leds[1][1]] = current_color;
      leds[preview_leds[1][2]] = current_color;
      leds[preview_leds[1][3]] = current_color;
      break;

    case JSHAPED:                     // j shape
      leds[preview_leds[0][1]] = current_color;
      leds[preview_leds[0][2]] = current_color;
      leds[preview_leds[1][2]] = current_color;
      leds[preview_leds[2][2]] = current_color;
      break;
    
    case ZSHAPED:                     // z shape
      leds[preview_leds[0][2]] = current_color;
      leds[preview_leds[1][2]] = current_color;
      leds[preview_leds[1][1]] = current_color;
      leds[preview_leds[2][1]] = current_color;
      break;

    case LSHAPED:                 // L shaped
      leds[preview_leds[1][0]] = current_color;
      leds[preview_leds[1][1]] = current_color;
      leds[preview_leds[1][2]] = current_color;
      leds[preview_leds[2][0]] = current_color;
      break;

    case SSHAPED:                // sshaped
      leds[preview_leds[2][2]] = current_color;
      leds[preview_leds[1][2]] = current_color;
      leds[preview_leds[1][1]] = current_color;
      leds[preview_leds[0][1]] = current_color;
      break;

    case TSHAPED:              // t shaped
      leds[preview_leds[0][1]] = current_color;
      leds[preview_leds[1][1]] = current_color;
      leds[preview_leds[2][1]] = current_color;
      leds[preview_leds[1][2]] = current_color;
      break;
  }
}

void restart_game()
{
  for(int i = 0; i < max_y; i++)
  {
    for(int j = 0; j < max_x; j++)
    {
      leds[led_pos[j][i]] = CRGB::Green;
      leds[led_pos[j][i]+1] = CRGB::Green;
      board[j][i] = 0;
    } 
    
    FastLED.show();
    delay(125);
  }

  new_block();
  update_board();
 
}

void drop_blocks()
{
  if (check_drop())
    drop();
  else
   {
      check_and_clear_rows();
      new_block();
   } 
}

void drop()
{
  int color = board[pos[0][0]][pos[0][1]];
  
  for(int i=0; i < 4; i++)
    board[pos[i][0]][pos[i][1]] = 0;
          
  pos[0][1]++; pos[1][1]++; pos[2][1]++; pos[3][1]++;

  for(int i=0; i < 4; i++)
    board[pos[i][0]][pos[i][1]] = color;
}

void move_block(int dir)
{
  int color = board[pos[0][0]][pos[0][1]];

  if ((dir == MOVE_LEFT) && check_left())
  {
    for(int i=0; i < 4; i++)
      board[pos[i][0]][pos[i][1]] = 0;
    
    for(int i=0; i < 4; i++)  
    {
      pos[i][0]--;
      board[pos[i][0]][pos[i][1]] = color;
    }
  }
  else if ((dir == MOVE_RIGHT) && check_right())
  {
    for(int i=0; i < 4; i++)
      board[pos[i][0]][pos[i][1]] = 0;

    for(int i=3; i >= 0; i--)
    {
      pos[i][0]++;
      board[pos[i][0]][pos[i][1]] = color;
    }
  }
}

boolean check_left()
{
  int b_one = board[pos[0][0]-1][pos[0][1]];
  int b_two = board[pos[1][0]-1][pos[1][1]];
  int b_three = board[pos[2][0]-1][pos[2][1]];
  int b_four = board[pos[3][0]-1][pos[3][1]];

  boolean check_under[4] = {false, false, false, false};
  
  //check if block to right is itelf
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
    {
      if (i == j)
        continue;

      check_under[i] = check_under[i] || ((pos[i][0]-1 == pos[j][0]) && (pos[i][1] == pos[j][1]));
    }

  return (((b_one == 0) ||  check_under[0]) && ((b_two == 0) ||  check_under[1]) &&
          ((b_three == 0) ||  check_under[2]) && ((b_four == 0) ||  check_under[3]) &&
          (pos[0][0] > 0) && (pos[1][0] > 0) && (pos[2][0] > 0) && (pos[3][0] > 0)); 
}

boolean check_right()
{
  int b_one = board[pos[0][0]+1][pos[0][1]];
  int b_two = board[pos[1][0]+1][pos[1][1]];
  int b_three = board[pos[2][0]+1][pos[2][1]];
  int b_four = board[pos[3][0]+1][pos[3][1]];

  boolean check_under[4] = {false, false, false, false};
  
  //check if block to right is itelf
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
    {
      if (i == j)
        continue;

      check_under[i] = check_under[i] || ((pos[i][0]+1 == pos[j][0]) && (pos[i][1] == pos[j][1]));
    }

  return (((b_one == 0) ||  check_under[0]) && ((b_two == 0) ||  check_under[1]) &&
          ((b_three == 0) ||  check_under[2]) && ((b_four == 0) ||  check_under[3]) &&
          (pos[0][0] < max_x-1) && (pos[1][0] < max_x-1) && (pos[2][0] < max_x-1) && (pos[3][0] < max_x-1));  
}

boolean check_drop()
{
  int b_one = board[pos[0][0]][pos[0][1]+1];
  int b_two = board[pos[1][0]][pos[1][1]+1];
  int b_three = board[pos[2][0]][pos[2][1]+1];
  int b_four = board[pos[3][0]][pos[3][1]+1];

  boolean check_under[4] = {false, false, false, false};
  
  //check if block underneath is itelf
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
    {
      if (i == j)
        continue;

      check_under[i] = check_under[i] || ((pos[i][0] == pos[j][0]) && (pos[i][1]+1 == pos[j][1]));
    }

  return (((b_one == 0) ||  check_under[0]) && ((b_two == 0) ||  check_under[1]) &&
          ((b_three == 0) ||  check_under[2]) && ((b_four == 0) ||  check_under[3]) &&
          (pos[0][1] < max_y-1) && (pos[1][1] < max_y-1) && (pos[2][1] < max_y-1) && (pos[3][1] < max_y-1));
}

void rotate_long()
{
  int pos1[2] = {pos[0][0], pos[0][1]};
  int pos2[2] = {pos[1][0], pos[1][1]};
  int pos3[2] = {pos[2][0], pos[2][1]};
  int pos4[2] = {pos[3][0], pos[3][1]};

  int new_pos1[2];
  int new_pos2[2];
  int new_pos3[2];
  int new_pos4[2];
  
  int color = board[pos1[0]][pos1[1]];

  switch(state)
  {
    case 0:
      new_pos1[0] = pos2[0]; new_pos1[1] =  pos2[1]-1;
      new_pos2[0] = pos2[0]; new_pos2[1] =  pos2[1];
      new_pos3[0] = pos2[0]; new_pos3[1] =  pos2[1]+1;
      new_pos4[0] = pos2[0]; new_pos4[1] =  pos2[1]+2;

      if ( (pos2[1]-1 < 0) || (pos2[1]+1 >= max_y)|| (pos2[1]+2 >= max_y))
        return;

      if ((board[new_pos1[0]][new_pos1[1]] == 0) && (board[new_pos3[0]][new_pos3[1]] == 0) && (board[new_pos4[0]][new_pos4[1]] == 0))
      {
        for(int i = 0; i < 4; i++)
          board[pos[i][0]][pos[i][1]] = 0;
        
        pos[0][0] = new_pos1[0]; pos[0][1] = new_pos1[1];
        pos[1][0] = new_pos2[0]; pos[1][1] = new_pos2[1];
        pos[2][0] = new_pos3[0]; pos[2][1] = new_pos3[1];
        pos[3][0] = new_pos4[0]; pos[3][1] = new_pos4[1];

        for(int i = 0; i < 4; i++)
          board[pos[i][0]][pos[i][1]] = color;
      }

      state = 1;

      break;
      
    case 1:
      new_pos1[0] = pos2[0]-1; new_pos1[1] =  pos2[1];
      new_pos2[0] = pos2[0]; new_pos2[1] =  pos2[1];
      new_pos3[0] = pos2[0]+1; new_pos3[1] =  pos2[1];
      new_pos4[0] = pos2[0]+2; new_pos4[1] =  pos2[1];

      if ( (pos2[0]-1 < 0) || (pos2[0]+1 >= max_x)|| (pos2[0]+2 >= max_x))
        return;

      if ((board[new_pos1[0]][new_pos1[1]] == 0) && (board[new_pos3[0]][new_pos3[1]] == 0) && (board[new_pos4[0]][new_pos4[1]] == 0))
      {
        for(int i = 0; i < 4; i++)
          board[pos[i][0]][pos[i][1]] = 0;
        
        pos[0][0] = new_pos1[0]; pos[0][1] = new_pos1[1];
        pos[1][0] = new_pos2[0]; pos[1][1] = new_pos2[1];
        pos[2][0] = new_pos3[0]; pos[2][1] = new_pos3[1];
        pos[3][0] = new_pos4[0]; pos[3][1] = new_pos4[1];

        for(int i = 0; i < 4; i++)
          board[pos[i][0]][pos[i][1]] = color;
      }

      state = 0;

      break;
  }
  
}

void execute_rotation(int new_pos1[2], int new_pos2[2], int new_pos3[2], int new_pos4[2])
{
   int color = board[new_pos2[0]][new_pos2[1]];
   
   for(int i = 0; i < 4; i++)
    board[pos[i][0]][pos[i][1]] = 0;
        
  pos[0][0] = new_pos1[0]; pos[0][1] = new_pos1[1];
  pos[1][0] = new_pos2[0]; pos[1][1] = new_pos2[1];
  pos[2][0] = new_pos3[0]; pos[2][1] = new_pos3[1];
  pos[3][0] = new_pos4[0]; pos[3][1] = new_pos4[1];

  for(int i = 0; i < 4; i++)
    board[pos[i][0]][pos[i][1]] = color;
}

void rotate_jshaped()
{
  int pos1[2] = {pos[0][0], pos[0][1]};
  int pos2[2] = {pos[1][0], pos[1][1]};
  int pos3[2] = {pos[2][0], pos[2][1]};
  int pos4[2] = {pos[3][0], pos[3][1]};

  int new_pos1[2];
  int new_pos2[2];
  int new_pos3[2];
  int new_pos4[2];
  
  int color = board[pos1[0]][pos1[1]];

  switch(state)
  {
    case 0:
      new_pos1[0] = pos2[0]; new_pos1[1] =  pos2[1]-1;
      new_pos2[0] = pos2[0]; new_pos2[1] =  pos2[1];
      new_pos3[0] = pos2[0]; new_pos3[1] =  pos2[1]+1;
      new_pos4[0] = pos2[0]-1; new_pos4[1] =  pos2[1]+1;

      if ( (pos2[0]-1 < 0) || (pos2[1]+1 >= max_y)|| (pos2[1]-1 < 0))
        return;
      
      if ((board[new_pos1[0]][new_pos1[1]] != 0) || (board[new_pos3[0]][new_pos3[1]] != 0) || (board[new_pos4[0]][new_pos4[1]] != 0))
        return;

      execute_rotation(new_pos1, new_pos2, new_pos3, new_pos4);
      state = 1;
      break;

    case 1:
      new_pos1[0] = pos2[0]+1; new_pos1[1] =  pos2[1];
      new_pos2[0] = pos2[0]; new_pos2[1] =  pos2[1];
      new_pos3[0] = pos2[0]-1; new_pos3[1] =  pos2[1];
      new_pos4[0] = pos2[0]-1; new_pos4[1] =  pos2[1]-1;

      if ( (pos2[0]-1 < 0) || (pos2[0]+1 >= max_x)|| (pos2[1]-1 < 0))
        return;

      if ((board[new_pos1[0]][new_pos1[1]] != 0) || (board[new_pos3[0]][new_pos3[1]] != 0) || (board[new_pos4[0]][new_pos4[1]] != 0))
        return;

      execute_rotation(new_pos1, new_pos2, new_pos3, new_pos4);
      state =  2;
      break;

   case 2:
      new_pos1[0] = pos2[0]; new_pos1[1] =  pos2[1]+1;
      new_pos2[0] = pos2[0]; new_pos2[1] =  pos2[1];
      new_pos3[0] = pos2[0]; new_pos3[1] =  pos2[1]-1;
      new_pos4[0] = pos2[0]+1; new_pos4[1] =  pos2[1]-1;

      if ( (pos2[1]-1 < 0) || (pos2[0]+1 >= max_x)|| (pos2[1]+1 >= max_y))
        return;

      if ((board[new_pos1[0]][new_pos1[1]] != 0) || (board[new_pos3[0]][new_pos3[1]] != 0) || (board[new_pos4[0]][new_pos4[1]] != 0))
        return;

      execute_rotation(new_pos1, new_pos2, new_pos3, new_pos4);
      state = 3;
      break;

    case 3:
      new_pos1[0] = pos2[0]-1; new_pos1[1] =  pos2[1];
      new_pos2[0] = pos2[0]; new_pos2[1] =  pos2[1];
      new_pos3[0] = pos2[0]+1; new_pos3[1] =  pos2[1];
      new_pos4[0] = pos2[0]+1; new_pos4[1] =  pos2[1]+1;

      if ( (pos2[0]-1 < 0) || (pos2[0]+1 >= max_x)|| (pos2[1]+1 >= max_y))
        return;

      if ((board[new_pos1[0]][new_pos1[1]] != 0) || (board[new_pos3[0]][new_pos3[1]] != 0) || (board[new_pos4[0]][new_pos4[1]] != 0))
        return;

      execute_rotation(new_pos1, new_pos2, new_pos3, new_pos4);
      state = 0;
      break;
  }
}

void rotate_zshaped()
{
  int pos1[2] = {pos[0][0], pos[0][1]};
  int pos2[2] = {pos[1][0], pos[1][1]};
  int pos3[2] = {pos[2][0], pos[2][1]};
  int pos4[2] = {pos[3][0], pos[3][1]};

  int new_pos1[2];
  int new_pos2[2];
  int new_pos3[2];
  int new_pos4[2];
  
  int color = board[pos1[0]][pos1[1]];

  switch(state)
  {
    case 0:
      new_pos1[0] = pos2[0]; new_pos1[1] =  pos2[1]-1;
      new_pos2[0] = pos2[0]; new_pos2[1] =  pos2[1];
      new_pos3[0] = pos2[0]-1; new_pos3[1] =  pos2[1];
      new_pos4[0] = pos2[0]-1; new_pos4[1] =  pos2[1]+1;

      if ( (pos2[0]-1 < 0) || (pos2[1]+1 >= max_y)|| (pos2[1]-1 < 0))
        return;

      if ((board[new_pos1[0]][new_pos1[1]] != 0) || (board[new_pos4[0]][new_pos4[1]] != 0))
        return;

      execute_rotation(new_pos1, new_pos2, new_pos3, new_pos4);
      state = 1;
      break;

    case 1:
      new_pos1[0] = pos2[0]-1; new_pos1[1] =  pos2[1];
      new_pos2[0] = pos2[0]; new_pos2[1] =  pos2[1];
      new_pos3[0] = pos2[0]; new_pos3[1] =  pos2[1]+1;
      new_pos4[0] = pos2[0]+1; new_pos4[1] =  pos2[1]+1;

      if ( (pos2[0]-1 < 0) || (pos2[0]+1 >= max_x)|| (pos2[1]+1 >= max_y))
        return;

      if ((board[new_pos3[0]][new_pos3[1]] != 0) || (board[new_pos4[0]][new_pos4[1]] != 0))
        return;

      execute_rotation(new_pos1, new_pos2, new_pos3, new_pos4);
      state =  0;
      break;

  }
}

void rotate_lshaped()
{
  int pos1[2] = {pos[0][0], pos[0][1]};
  int pos2[2] = {pos[1][0], pos[1][1]};
  int pos3[2] = {pos[2][0], pos[2][1]};
  int pos4[2] = {pos[3][0], pos[3][1]};

  int new_pos1[2];
  int new_pos2[2];
  int new_pos3[2];
  int new_pos4[2];

  switch(state)
  {
    case 0:
      new_pos1[0] = pos2[0]; new_pos1[1] =  pos2[1]-1;
      new_pos2[0] = pos2[0]; new_pos2[1] =  pos2[1];
      new_pos3[0] = pos2[0]; new_pos3[1] =  pos2[1]+1;
      new_pos4[0] = pos2[0]+1; new_pos4[1] =  pos2[1]+1;

      if ( (pos2[1]-1 < 0) || (pos2[1]+1 >= max_y) || (pos2[0]+1 >= max_x))
        return;

     if ((board[new_pos1[0]][new_pos1[1]] != 0) || (board[new_pos3[0]][new_pos3[1]] != 0) || (board[new_pos4[0]][new_pos4[1]] != 0))
        return;

      execute_rotation(new_pos1, new_pos2, new_pos3, new_pos4);
      state = 1;
      break;

    case 1:
      new_pos1[0] = pos2[0]-1; new_pos1[1] =  pos2[1];
      new_pos2[0] = pos2[0]; new_pos2[1] =  pos2[1];
      new_pos3[0] = pos2[0]+1; new_pos3[1] =  pos2[1];
      new_pos4[0] = pos2[0]+1; new_pos4[1] =  pos2[1]-1;

      if ( (pos2[0]-1 < 0) ||(pos2[1]-1 < 0) || (pos2[0]+1 >= max_x))
        return;

      if ((board[new_pos1[0]][new_pos1[1]] != 0) || (board[new_pos3[0]][new_pos3[1]] != 0) || (board[new_pos4[0]][new_pos4[1]] != 0))
        return;

      execute_rotation(new_pos1, new_pos2, new_pos3, new_pos4);
      state = 2;
      break;

   case 2:
      new_pos1[0] = pos2[0]; new_pos1[1] =  pos2[1]+1;
      new_pos2[0] = pos2[0]; new_pos2[1] =  pos2[1];
      new_pos3[0] = pos2[0]; new_pos3[1] =  pos2[1]-1;
      new_pos4[0] = pos2[0]-1; new_pos4[1] =  pos2[1]-1;

      if ( (pos2[0]-1 < 0) ||(pos2[1]-1 < 0) || (pos2[1]+1 >= max_y))
        return;

      if ((board[new_pos1[0]][new_pos1[1]] != 0) || (board[new_pos3[0]][new_pos3[1]] != 0) || (board[new_pos4[0]][new_pos4[1]] != 0))
        return;

      execute_rotation(new_pos1, new_pos2, new_pos3, new_pos4);
      state = 3;
      break;

    case 3:
      new_pos1[0] = pos2[0]-1; new_pos1[1] =  pos2[1];
      new_pos2[0] = pos2[0]; new_pos2[1] =  pos2[1];
      new_pos3[0] = pos2[0]+1; new_pos3[1] =  pos2[1];
      new_pos4[0] = pos2[0]-1; new_pos4[1] =  pos2[1]+1;

      if ( (pos2[0]-1 < 0) || (pos2[0]+1 >= max_x)|| (pos2[1]+1 >= max_y))
        return;

      if ((board[new_pos1[0]][new_pos1[1]] != 0) || (board[new_pos3[0]][new_pos3[1]] != 0) || (board[new_pos4[0]][new_pos4[1]] != 0))
        return;

      execute_rotation(new_pos1, new_pos2, new_pos3, new_pos4);
      state = 0;
      break;
  }
}

void rotate_sshaped()
{
  int pos1[2] = {pos[0][0], pos[0][1]};
  int pos2[2] = {pos[1][0], pos[1][1]};
  int pos3[2] = {pos[2][0], pos[2][1]};
  int pos4[2] = {pos[3][0], pos[3][1]};

  int new_pos1[2];
  int new_pos2[2];
  int new_pos3[2];
  int new_pos4[2];
  
  int color = board[pos1[0]][pos1[1]];

  switch(state)
  {
    case 0:
      new_pos1[0] = pos2[0]; new_pos1[1] =  pos2[1]+1;
      new_pos2[0] = pos2[0]; new_pos2[1] =  pos2[1];
      new_pos3[0] = pos2[0]-1; new_pos3[1] =  pos2[1];
      new_pos4[0] = pos2[0]-1; new_pos4[1] =  pos2[1]-1;

      if ( (pos2[0]-1 < 0) || (pos2[1]+1 >= max_y)|| (pos2[1]-1 < 0))
        return;

      if ((board[new_pos1[0]][new_pos1[1]] != 0) || (board[new_pos4[0]][new_pos4[1]] != 0))
        return;

      execute_rotation(new_pos1, new_pos2, new_pos3, new_pos4);
      state =  1;
      break;

    case 1:
      new_pos1[0] = pos2[0]-1; new_pos1[1] =  pos2[1];
      new_pos2[0] = pos2[0]; new_pos2[1] =  pos2[1];
      new_pos3[0] = pos2[0]; new_pos3[1] =  pos2[1]-1;
      new_pos4[0] = pos2[0]+1; new_pos4[1] =  pos2[1]-1;

      if ((board[new_pos3[0]][new_pos3[1]] != 0) || (board[new_pos4[0]][new_pos4[1]] != 0))
        return;

      execute_rotation(new_pos1, new_pos2, new_pos3, new_pos4);
      state =  0;
      break;

  }
}

void rotate_tshaped()
{
  int pos1[2] = {pos[0][0], pos[0][1]};
  int pos2[2] = {pos[1][0], pos[1][1]};
  int pos3[2] = {pos[2][0], pos[2][1]};
  int pos4[2] = {pos[3][0], pos[3][1]};

  int new_pos1[2];
  int new_pos2[2];
  int new_pos3[2];
  int new_pos4[2];

  switch(state)
  {
    case 0:
      new_pos1[0] = pos2[0]; new_pos1[1] =  pos2[1]+1;
      new_pos2[0] = pos2[0]; new_pos2[1] =  pos2[1];
      new_pos3[0] = pos2[0]; new_pos3[1] =  pos2[1]-1;
      new_pos4[0] = pos2[0]-1; new_pos4[1] =  pos2[1];

      //check bounds
      if ( (pos2[1]-1 < 0) || (pos2[1]+1 >= max_y) || (pos2[0]-1 < 0))
        return;
     
     //check collisions
      if ((board[new_pos3[0]][new_pos3[1]] != 0))
        return;

      execute_rotation(new_pos1, new_pos2, new_pos3, new_pos4);
      state = 1;
      break;

    case 1:
      new_pos1[0] = pos2[0]-1; new_pos1[1] =  pos2[1];
      new_pos2[0] = pos2[0]; new_pos2[1] =  pos2[1];
      new_pos3[0] = pos2[0]+1; new_pos3[1] =  pos2[1];
      new_pos4[0] = pos2[0]; new_pos4[1] =  pos2[1]-1;

      if ( (pos2[0]-1 < 0) ||(pos2[1]-1 < 0) || (pos2[0]+1 >= max_x))
        return;

      //check collisions
      if ((board[new_pos3[0]][new_pos3[1]] != 0))
        return;

      execute_rotation(new_pos1, new_pos2, new_pos3, new_pos4);
      state = 2;
      break;

   case 2:
      new_pos1[0] = pos2[0]; new_pos1[1] =  pos2[1]-1;
      new_pos2[0] = pos2[0]; new_pos2[1] =  pos2[1];
      new_pos3[0] = pos2[0]; new_pos3[1] =  pos2[1]+1;
      new_pos4[0] = pos2[0]+1; new_pos4[1] =  pos2[1];

      if ( (pos2[1]-1 < 0) ||(pos2[1]+1 >= max_y || (pos2[0]+1 >= max_x)))
        return;

      //check collisions
      if ((board[new_pos3[0]][new_pos3[1]] != 0))
        return;

      execute_rotation(new_pos1, new_pos2, new_pos3, new_pos4);
      state = 3;
      break;

    case 3:
      new_pos1[0] = pos2[0]-1; new_pos1[1] =  pos2[1];
      new_pos2[0] = pos2[0]; new_pos2[1] =  pos2[1];
      new_pos3[0] = pos2[0]+1; new_pos3[1] =  pos2[1];
      new_pos4[0] = pos2[0]; new_pos4[1] =  pos2[1]+1;

      if ( (pos2[0]-1 < 0) || (pos2[0]+1 >= max_x)|| (pos2[1]+1 >= max_y))
        return;

      //check collisions
      if ((board[new_pos1[0]][new_pos1[1]] != 0))
        return;

      execute_rotation(new_pos1, new_pos2, new_pos3, new_pos4);
      state = 0;
      break;
  }
}

void rotate()
{
  switch(block_type)
  {
    case SQUARE:                       //square
      break;
      
    case LONG:                     // long
      rotate_long();
      break;

    case JSHAPED:                     // j shape
      rotate_jshaped();
      break;
    
    case ZSHAPED:                     // z shape
      rotate_zshaped();     
      break;

    case LSHAPED:                 // L shaped
      rotate_lshaped();
      break;

    case SSHAPED:                // sshaped
      rotate_sshaped();
      break;

    case TSHAPED:              // t shaped
      rotate_tshaped();
      break;
  }
}
