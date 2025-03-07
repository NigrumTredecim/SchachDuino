#include <Keypad.h>
#include <AccelStepper.h>
#include <LiquidCrystal.h>

#define NOP __asm__ __volatile__ ("nop\n\t") //compiler macro for NOP

#define xStep 22 //pin definitions for AccelStepper
#define xDir 24
#define xEnable 26
#define xEndlage 28
#define xResolution 1000

#define yStep 30
#define yDir 32
#define yEnable 34
#define yEndlage 36
#define yResolution 1000

#define zStep 38
#define zDir 40
#define zEnable 42
#define zEndlage 44

AccelStepper xStepper = AccelStepper(1, xStep, xDir); //column stepper
AccelStepper yStepper = AccelStepper(1, yStep, yDir); //row stepper
AccelStepper zStepper = AccelStepper(1, zStep, zDir); //height stepper

#define magnetPin 13

char keyMap[4][4] = {{'1','2','3','4'}, {'5','6','7','8'}, {'A','B','C','D'}, {'E','F','G','H'}};

byte rowPins[4] = {14, 15, 16, 17};
byte colPins[4] = {18, 19, 20, 21};

Keypad Numpad = Keypad(makeKeymap(keyMap), rowPins, colPins, 4, 4); 

#define rs 33 //pin definitions for the lcd display
#define rw 35
#define enable 37
#define d0 39
#define d1 41
#define d2 43
#define d3 45
#define d4 47
#define d5 49
#define d6 51
#define d7 53

byte last3moves [8] [6];
/*
[value] [move]
#value
whites part of the move
0 = inpstartrow
1 = inpstartcol
2 = inpendrow
3 = inpendcol
black part of the move
4 = inpstartrow
5 = inpstartcol
6 = inpendrow
7 = inpendcol
#move
0 = current move
1 - 5 = previous move getting pushed through by the new one
*/

LiquidCrystal lcd (rs, rw, enable, d0, d1, d2, d3, d4, d5, d6, d7); //initializing the lcd display

bool checkformate(byte dim, byte kingcolour);
void movepiecelog(byte dim, byte startcol, byte startrow, byte endcol, byte endrow);
byte inpstartrow; //decoded input 1 to 8
byte inpstartcol; //decoded input 1 to 8
byte inpendrow; //decoded input 1 to 8
byte inpendcol; //decoded input 1 to 8

byte movingpieceid; //id of the moved piece
byte movingpiececol; //color of the moved piece
byte movingpiecetyp; //type of the moved piece
unsigned int movingpieceheight;
byte targetpieceid; //id of the target piece
byte targetpiececol; //color of the target piece
byte targetpiecetyp; //type of the target piece
unsigned int targetpieceheight;

byte activeplayercol; //gets swapped around by main

byte gamestate;
bool isfirstmove; //set for each piece in pieces [?] [3] by default gets removed by main
unsigned int enpassantpossible; //currently gets set by validatemove but should be moved to main as this may cause enpassant to become falsely possible due to a bug when checking for mate
byte enpassantpossibleon; //keeps track of pawns doing their double long opening move

bool castlingqueensideallowedblack;
bool castlingkingsideallowedblack;
bool castlingqueensideallowedwhite;
bool castlingkingsideallowedwhite;

byte headrow = 0; //row where the head is currently positioned
byte headcol = 0; //column where the head is currently positioned

byte movevalid = 0; //buffer variable for the return value of validatemvoe
//create array for the board
byte board [10] [10] [3]; //board [row] [col] [dim] were playing 3d chess now btw
// [0] [0] is the top left corner when looking from white
//create and populate array for the pieces 
//the first int in the arrays is the colour and the second is the type
//1xx is black
//2xx is white
//x1x is pawn
//x2x is rook
//X3x is knight
//x4x is bishop
//x5x is queen
//x6x is king
//x7x is pawn promotion placeholder type
//xx0 piece has been moved before
//xx1 piece has not been moved 
//[3] row of the piece when its out
//[4] column of the piece when its out
//[5] row of the piece
//[6] column of the piece
//[7] row of the piece on the default board #written once by main
//[8] column of the piece on the default board #written once by main
byte pieces [33] [9] = {{0, 0, 0, 0, 0, 0, 0},
{1, 2, 1, 0, 1}, {1, 3, 1, 0, 2}, {1, 4, 1, 0, 3}, {1, 5, 1, 0, 4}, {1, 6, 1, 0, 5}, {1, 4, 1, 0, 6}, {1, 3, 1, 0, 7}, {1, 2, 1, 0, 8},
{1, 1, 1, 1, 0}, {1, 1, 1, 2, 0}, {1, 1, 1, 3, 0}, {1, 1, 1, 4, 0}, {1, 1, 1, 5, 0}, {1, 1, 1, 6, 0}, {1, 1, 1, 7, 0}, {1, 1, 1, 8, 0},
{2, 1, 1, 1, 9}, {2, 1, 1, 2, 9}, {2, 1, 1, 3, 9}, {2, 1, 1, 4, 9}, {2, 1, 1, 5, 9}, {2, 1, 1, 6, 9}, {2, 1, 1, 7, 9}, {2, 1, 1, 8, 9},
{2, 2, 1, 9, 1}, {2, 3, 1, 9, 2}, {2, 4, 1, 9, 3}, {2, 5, 1, 9, 4}, {2, 6, 1, 9, 5}, {2, 4, 1, 9, 6}, {2, 3, 1, 9, 7}, {2, 2, 1, 9, 8}};
void boardcopydimension(int sourcedimension, int targetdimension){
	byte row = 0;
	byte col = 0;
	while (row < 10){
		while (col < 10){
			board [row] [col] [targetdimension] = board [row] [col] [sourcedimension];
			++col;
		}
		++row;
		col = 0;
	}
	row = 0;
}
void dumplogicfieldtoshell(byte dim){
	int horpos;
	int verpos;
	int num;
	horpos = 0;
	verpos = 0;
	num = 10;
  Serial.println("       A   B   C   D   E   F   G   H");
	while (verpos < 10){
    Serial.print(num - 1);
		--num;
		while (horpos < 10){
			Serial.print(" ");
			if (2 == pieces [board [verpos] [horpos] [dim]] [0]){Serial.print("W");}
			if (1 == pieces [board [verpos] [horpos] [dim]] [0]){Serial.print("S");}
			if (0 == pieces [board [verpos] [horpos] [dim]] [0]){Serial.print("0");}
			Serial.print(pieces [board [verpos] [horpos] [dim]] [1]);
			Serial.print(pieces [board [verpos] [horpos] [dim]] [2]);
			horpos++;
	  }
	Serial.print(" ");
  Serial.print(num);
  Serial.print("\n");
	horpos = 0;
	verpos++;
	}
  Serial.println("       A   B   C   D   E   F   G   H");
}
int convchartoint (char input, bool invert = 0){
  if (invert != 0){
    switch (input){
      case '1': return 8;
      case '2': return 7;
      case '3': return 6;
      case '4': return 5;
      case '5': return 4;
      case '6': return 3;
      case '7': return 2;
      case '8': return 1;
    }
  }
  else {
    switch (input){
      case '1': return 1;
      case '2': return 2;
      case '3': return 3;
      case '4': return 4;
      case '5': return 5;
      case '6': return 6;
      case '7': return 7;
      case '8': return 8;
    }
  }
  return 0;
}
void readinput(){
  inpstartcol = 0;
  inpstartrow = 0;
  inpendcol = 0;
  inpendrow = 0;
  char readbuffer;
  lcd.setCursor(2, 1);
  lcd.print("->");
  while (inpstartcol < 1 || inpstartcol > 8 || inpstartrow < 1 || inpstartrow > 8 || inpendcol < 1 || inpendcol > 8 || inpendrow < 1 || inpendrow > 8){ //keep reading the input until all variables have valid data
    lcd.setCursor(0, 2);
    if(0==1){NOP;}
    else if (inpstartcol < 1 || inpstartcol > 8){
      lcd.print("Buchstabe zuganf");
      lcd.setCursor(0, 1);
    }
    else if (inpstartrow < 1 || inpstartrow > 8){
      lcd.print("Ziffer zuganfang");
      lcd.setCursor(1, 1);
    }
    else if (inpendcol < 1 || inpendcol > 8){
      lcd.print("Buchstabe zugend");
      lcd.setCursor(4, 1);
    }
    else if (inpendrow < 1 || inpendrow > 8){
      lcd.print("Ziffer zugende");
      lcd.setCursor(5, 1);
    }
    lcd.print("X");
    readbuffer = Numpad.waitForKey();
    if (readbuffer == 'A' || readbuffer == 'B' || readbuffer == 'C' || readbuffer == 'D' || readbuffer == 'E' || readbuffer == 'F' || readbuffer == 'G' || readbuffer == 'H'){
      switch (readbuffer) {
        case 'A': inpstartcol = 0; inpstartrow = 0; inpendcol = 0; inpendrow = 0; break;
        case 'B': 
        case 'C':
        case 'D':
        case 'E':
        case 'F': gamestate = 0; break;
        case 'G': zerostepper(); break;
        case 'H': debug(); break;
      }
    }
    else if (inpstartcol < 1 || inpstartcol > 8){
      inpstartcol = convchartoint(readbuffer);
      lcd.setCursor(0, 1);
      switch (inpstartcol){
        case 1:lcd.print("A");break;
        case 2:lcd.print("B");break;
        case 3:lcd.print("C");break;
        case 4:lcd.print("D");break;
        case 5:lcd.print("E");break;
        case 6:lcd.print("F");break;
        case 7:lcd.print("G");break;
        case 8:lcd.print("H");break;
      }
    }
    else if (inpstartrow < 1 || inpstartrow > 8){
      inpstartrow = convchartoint(readbuffer, 1);
      lcd.setCursor(1, 1);
      switch (inpstartrow){
        case 1:lcd.print("8");break;
        case 2:lcd.print("7");break;
        case 3:lcd.print("6");break;
        case 4:lcd.print("5");break;
        case 5:lcd.print("4");break;
        case 6:lcd.print("3");break;
        case 7:lcd.print("2");break;
        case 8:lcd.print("1");break;
      }
    }
    else if (inpendcol < 1 || inpendcol > 8){
      inpendcol = convchartoint(readbuffer);
      lcd.setCursor(4, 1);
      switch (inpendcol){
        case 1:lcd.print("A");break;
        case 2:lcd.print("B");break;
        case 3:lcd.print("C");break;
        case 4:lcd.print("D");break;
        case 5:lcd.print("E");break;
        case 6:lcd.print("F");break;
        case 7:lcd.print("G");break;
        case 8:lcd.print("H");break;
      }
    }
    else if (inpendrow < 1 || inpendrow > 8){
      inpendrow = convchartoint(readbuffer, 1);
      lcd.setCursor(5, 1);
      switch (inpendrow){
        case 1:lcd.print("8");break;
        case 2:lcd.print("7");break;
        case 3:lcd.print("6");break;
        case 4:lcd.print("5");break;
        case 5:lcd.print("4");break;
        case 6:lcd.print("3");break;
        case 7:lcd.print("2");break;
        case 8:lcd.print("1");break;
      }
    }
  }
}
unsigned int getpieceheight(byte piecetype){//returns the height of that type of piece
  switch (piecetype) {
    case 1: return 100; //pawn
    case 2: return 100; //rook
    case 3: return 100; //knight
    case 4: return 100; //bishop
    case 5: return 100; //queen
    case 6: return 100; //king
  }
  return 0;
}
byte getpieceid(byte dim, byte row, byte col){return board [row] [col] [dim];} //return the id of the piece in that position
byte getpiececol(byte id){return pieces [id] [0];} //returns the colour of the piece
byte getpiecetype(byte id){return pieces [id] [1];} //returns the type of the piece
bool getisfirstmove(byte id){return pieces [id] [2];} //return true if the piece hasnt been moved yet
void setisfirstmove(byte id, bool state = true){pieces [id] [2] = state;}
byte getoutrow(byte id){return pieces [id] [3];} //returns the row where the piece should be positioned when it is out #NOT TO BE MODIFIED
byte getoutcol(byte id){return pieces [id] [4];} //returns the column where the piece should be positioned when it is out #NOT TO BE MODIFIED
byte getcurrrow(byte id){return pieces [id] [5];} //returns the row where the piece currently should be gets updated by readpiecedata()
byte getcurrcol(byte id){return pieces [id] [6];} //returns the column where the piece currently should be gets updated by readpiecedata() 
byte getdefrow(byte id){return pieces [id] [7];} //returns the row where the piece should be on the undisturbed board
byte getdefcol(byte id){return pieces [id] [8];} //returns the column where the piece should be on the undisturbed board
void readpiecedata(byte dim, byte startcol, byte startrow, byte endcol, byte endrow){
	movingpieceid = getpieceid(dim, startrow, startcol); //load the id of the piece to move
  movingpiececol = getpiececol(movingpieceid); //copy the color of the selected piece into ram
	movingpiecetyp = getpiecetype(movingpieceid); //copy the type of the selected piece into ram
  movingpieceheight = getpieceheight(movingpiecetyp);
	isfirstmove = getisfirstmove(movingpieceid); //copies the value that checks if the piece has been moved beforehand
	targetpieceid = getpieceid(dim, endrow, endcol); //read the target field
	targetpiececol = getpiececol(targetpieceid); //copy the color of the target piece into ram
	targetpiecetyp = getpiecetype(targetpieceid); //copy the type of the target piece into ram
  targetpieceheight = getpieceheight(targetpiecetyp);
  byte col = 0;
  byte row = 0;
  while (row < 10){ //run over the whole field and place the coordinates into the id of the piece positioned there
		while (col < 10){
      pieces [getpieceid(dim, row, col)] [5] = row;
      pieces [getpieceid(dim, row, col)] [6] = col;
			++col;
		}
		++row;
		col = 0;
	}
	row = 0;
}
void updatecastlingallowed(){//checks if the pieces needed for a castle have been moved previously
	castlingqueensideallowedblack = pieces [1] [2] && pieces [5] [2] ; //checks if the pieces involved in castling havent been moved yet
	castlingkingsideallowedblack = pieces [8] [2] && pieces [5] [2] ; //still the same
	castlingqueensideallowedwhite = pieces [25] [2] && pieces [29] [2] ; //nothing changed
	castlingkingsideallowedwhite = pieces [32] [2] && pieces [29] [2] ; //learn to read
}
byte validatemove(byte dim, byte startcol, byte startrow, byte endcol, byte endrow, byte activeplayercol){ //checks if the move that has been input is legal mate checks are done externally using this function to avoid recursion
  int i = 0; //return value is 0 for failed checks and any postive integer for succesful checks (different values for different situations example en passand or castle)
	readpiecedata(dim, startcol, startrow, endcol, endrow);
	if (movingpiececol != activeplayercol || movingpiececol == targetpiececol){ //rereads a new input if the old one picks an empty field, the oponents piece, or moves a piece on a field with a piece of the same colour
		return 0;
	}
  else if (startcol < 1 || startcol > 8){return 0;}
  else if (startrow < 1 || startcol > 8){return 0;}
  else if (endcol < 1 || endcol > 8){return 0;}
  else if (endrow < 1 || endrow > 8){return 0;}
	else { //executes the movement check if input validation doesnt fail
		if (movingpiecetyp == 0){
			Serial.println("how did this happen"); //this should NEVER be reached as moving an empty field should already be filtered out
			return 0;
		}
		else if (movingpiecetyp == 1){ //pawn checks
			if (activeplayercol == 1){	//actions if black moves a pawn
				if (targetpiecetyp == 0){ //check if the target field is empty as this influences how the pawns can move
					if (0 == startcol - endcol){ //check if the move is straight
						if (startrow - endrow == -1){ //check if the move is one field straight
							return 1;
						}
						else if (isfirstmove == 1 && startrow - endrow == -2){ //check for double long opening pawn move
							if (pieces [board [endrow - 1] [startcol] [dim]] [0] == 0){	//check if the field you are jumping over is empty
								return 2;
							}
						} 
					}
					else if (enpassantpossible == 1 && board [startrow] [endcol] [dim] == enpassantpossibleon){ //check if the move is en passant
						return 3;
					}
				}
				else if (targetpiecetyp != 0){ //checks for when the target field isnt empty
					if (abs(startcol - endcol) == 1 && (startrow - endrow) == -1){ //checks if the move is one field diagonal
						return 1;
					}
				}
			}
			else if (activeplayercol == 2){ //actions if white moves a pawn
				if (targetpiecetyp == 0){ //check if the target field is empty as this influences how the pawns can move
					if (0 == startcol - endcol){ //check if the move is straight
						if (startrow - endrow == 1){ //check if the move is one field straight
							return 1;
						}
						else if (isfirstmove == 1 && startrow - endrow == 2){ //check for double long opening pawn move
							if (pieces [board [endrow + 1] [startcol] [dim]] [0] == 0){ //check if the field you are jumping over is empty
								return 2;
							} 
						} 
					}
					else if (enpassantpossible == 1 && board [startrow] [endcol] [dim] == enpassantpossibleon){ //check if the move is en passant
						return 3;
					} 
				}
				else if (targetpiecetyp != 0){ //checks for when the target field isnt empty
					if (abs(startcol - endcol) == 1 && (startrow - endrow) == 1){ //checks if the move is one field diagonal
						return 1;
					}
				} 
			}
		}
		else if (movingpiecetyp == 2){ //rook checks
			if (startrow == endrow){ //check if the rook move is a straight horizontal line
				if (startcol < endcol){ //moving right
					i = 0;
					while (startcol + 1 < endcol){
						++startcol;
						i = i + board [startrow] [startcol] [dim];
					}
					if (i == 0) {return 1;}
				}
				else if (startcol > endcol){ //moving left
					i = 0;
					while (startcol - 1 > endcol){
						--startcol;
						i = i + board [startrow] [startcol] [dim];
					}
					if (i == 0) {return 1;}
				}
			}
			else if (startcol == endcol){ //check if the move is a straight vertical line
				if (startrow < endrow){ //moving down
					i = 0;
					while (startrow + 1 < endrow){
						++startrow;
						i = i + board [startrow] [startcol] [dim];
					}
					if (i == 0) {return 1;}
				}
				else if (startrow > endrow){ //moving up
					i = 0;
					while (startrow - 1 > endrow){
						--startrow;
						i = i + board [startrow] [startcol] [dim];
					}
					if (i == 0) {return 1;}
				}
			} 
		}
		else if (movingpiecetyp == 3){ //knight checks
			if ((abs(startcol - endcol) == 1 && abs(startrow - endrow) == 2) || (abs(startcol - endcol) == 2 && abs(startrow - endrow) == 1)){
				return 1;
			}
		}
		else if (movingpiecetyp == 4){ //bishop checks
			if (abs(startrow - endrow) == abs(startcol - endcol)){ //check if the move is diagonal
				if (startcol < endcol && startrow > endrow){
					i = 0;
					while (startcol + 1 < endcol && startrow - 1 > endrow){
						++startcol;
						--startrow;
						i = i + board [startrow] [startcol] [dim];
					}
					if (i == 0) {return 1;}
				}
				else if (startcol > endcol && startrow > endrow){
					i = 0;
					while (startcol - 1 > endcol && startrow - 1 > endrow){
						--startcol;
						--startrow;
						i = i + board [startrow] [startcol] [dim];
					}
					if (i == 0) {return 1;}
				}
				else if (startcol < endcol && startrow < endrow){
					i = 0;
					while (startcol + 1 < endcol && startrow + 1 < endrow){
						++startcol;
						++startrow;
						i = i + board [startrow] [startcol] [dim];
					}
					if (i == 0) {return 1;}
				}
				else if (startcol > endcol && startrow < endrow){
					i = 0;
					while (startcol - 1 > endcol && startrow + 1 < endrow){
						--startcol;
						++startrow;
						i = i + board [startrow] [startcol] [dim];
					}
					if (i == 0) {return 1;}
				}
			}
		}
		else if (movingpiecetyp == 5){ //queen checks
			if (startrow == endrow){ //check if the queen move is a straight horizontal line
				if (startcol < endcol){ //moving right
					i = 0;
					while (startcol + 1 < endcol){
						++startcol;
						i = i + board [startrow] [startcol] [dim];
					}
					if (i == 0) {return 1;}
				}
				else if (startcol > endcol){ //moving left
					i = 0;
					while (startcol - 1 > endcol){
						--startcol;
						i = i + board [startrow] [startcol] [dim];
					}
					if (i == 0) {return 1;}
				}
			}
			else if (startcol == endcol){ //check if the move is a straight vertical line
				if (startrow < endrow){ //moving down
					i = 0;
					while (startrow + 1 < endrow){
						++startrow;
						i = i + board [startrow] [startcol] [dim];
					}
					if (i == 0) {return 1;}
				}
				else if (startrow > endrow){ //moving up
					i = 0;
					while (startrow - 1 > endrow){
						--startrow;
						i = i + board [startrow] [startcol] [dim];
					}
					if (i == 0) {return 1;}
				}
			}
			else if (abs(startrow - endrow) == abs(startcol - endcol)){ //check if the move is diagonal
				if (startcol < endcol && startrow > endrow){
					i = 0;
					while (startcol + 1 < endcol && startrow - 1 > endrow){
						++startcol;
						--startrow;
						i = i + board [startrow] [startcol] [dim];
					}
					if (i == 0) {return 1;}
				}
				else if (startcol > endcol && startrow > endrow){
					i = 0;
					while (startcol - 1 > endcol && startrow - 1 > endrow){
						--startcol;
						--startrow;
						i = i + board [startrow] [startcol] [dim];
					}
					if (i == 0) {return 1;}
				}
				else if (startcol < endcol && startrow < endrow){
					i = 0;
					while (startcol + 1 < endcol && startrow + 1 < endrow){
						++startcol;
						++startrow;
						i = i + board [startrow] [startcol] [dim];
					}
					if (i == 0) {return 1;}
				}
				else if (startcol > endcol && startrow < endrow){
					i = 0;
					while (startcol - 1 > endcol && startrow + 1 < endrow){
						--startcol;
						++startrow;
						i = i + board [startrow] [startcol] [dim];
					}
					if (i == 0) {return 1;}
				}
			}
		}
		else if (movingpiecetyp == 6){ //king checks
			if (1 >= abs(startcol - endcol) && 1 >= abs(startrow - endrow)){ //check if the king move is not longer than 1 field
				return 1;
			} 
			else if (startrow == endrow && abs(startcol - endcol) == 2){  //check for castle (stage 1 still need to verify if the rook you want to castle with has already been moved)
				if (movingpiececol == 1){ //black is moving
					if (endcol == 3 && castlingqueensideallowedblack == 1){ //queenside castle
						boardcopydimension(1, 2);
						if (checkformate(2, 2) == 0){
							movepiecelog(2, 5, 1, 4, 1);
							if (checkformate(2, 2) == 0){
								movepiecelog(2, 4, 1, 3, 1);
								if (checkformate(2, 2) == 0){
									Serial.println("black queenside castle");
									if (0 == board [1] [2] [1] && 0 == board [1] [3] [1] && 0 == board [1] [4] [1]){
										return 11;
									}
								}
							}
						}
					}
					else if (endcol == 7 && castlingkingsideallowedblack == 1){ //kingside castle
						boardcopydimension(1, 2);
						if (checkformate(2, 2) == 0){
							movepiecelog(2, 5, 1, 6, 1);
							if (checkformate(2, 2) == 0){
								movepiecelog(2, 4, 1, 7, 1);
								if (checkformate(2, 2) == 0){
									Serial.println("black kingside castle");
									if (0 == board [1] [6] [1] && 0 == board [1] [7] [1]) {
										return 12;
									}
								}
							}
						}
					}
				}
				else if (movingpiececol == 2){ //white is moving
					if (endcol == 3 && castlingqueensideallowedwhite == 1){ //queenside castle
						boardcopydimension(1, 2);
						if (checkformate(2, 1) == 0){
							movepiecelog(2, 5, 8, 4, 8);
							if (checkformate(2, 1) == 0){
								movepiecelog(2, 4, 8, 3, 8);
								if (checkformate(2, 1) == 0){
									Serial.println("white queenside castle");
									if (0 == board [8] [2] [1] && 0 == board [8] [3] [1] && 0 == board [8] [4] [1]) {
										return 13;
									}
								}
							}
						}
					}
					else if (endcol == 7 && castlingkingsideallowedwhite == 1){ //kingside castle
						boardcopydimension(1, 2);
						if (checkformate(2, 1) == 0){
							movepiecelog(2, 5, 8, 6, 8);
							if (checkformate(2, 1) == 0){
								movepiecelog(2, 4, 8, 7, 8);
								if (checkformate(2, 1) == 0){
									Serial.println("white kingside castle");
									if (0 == board [8] [6] [1] && 0 == board [8] [7] [1]) {
										return 14;
									}
								}
							}
						}
					}
				}
			}
		}
		else if (movingpiecetyp == 7){ //pawn promotion checks
			
		}
		else { //catchall if no of the previous checks reacts
			Serial.println("what did you do?");
			return 0;
		}
	}
	return 0;
}
bool checkformate(byte dim, byte kingcolour){ //checks if the king of the chosen color is under attack from any piece
	byte kingrow;
	byte kingcol;
	byte row;
	byte col;
	byte playerkingcheckcol;
	kingrow = 1;
	kingcol = 1;
	row = 1;
	col = 1;
	if (kingcolour == 1){playerkingcheckcol = 2;}
	if (kingcolour == 2){playerkingcheckcol = 1;}
	while (kingrow < 9){//this loop gets the position of the king for the wanted colour
		while (kingcol < 9){
			if (pieces [board [kingrow] [kingcol] [dim]] [0] == kingcolour && pieces [board [kingrow] [kingcol] [dim]] [1] == 6){break;} //überprüfung ob das gewählte feld ein könig der gewünschten farbe ist
			++kingcol;
		}
		if (pieces [board [kingrow] [kingcol] [dim]] [0] == kingcolour && pieces [board [kingrow] [kingcol] [dim]] [1] == 6){break;} //überprüfung ob das gewählte feld ein könig der gewünschten farbe ist
		++kingrow;
		kingcol = 1;
	}
	while (row < 9){
		while (col < 9){
			if (validatemove(dim, col, row, kingcol, kingrow, playerkingcheckcol)){return 1;}
			++col;
		}
		++row;
		col = 1;
	}
	return 0;
}
void removepiecelog(byte dim, byte col, byte row){
  //board [pieces[getpieceid(dim, row, col)][3]] [pieces[getpieceid(dim, row, col)][4]] [dim] = getpieceid(dim, row, col);
  board [getoutrow(getpieceid(dim, row, col))] [getoutcol(getpieceid(dim, row, col))] [dim] = getpieceid(dim, row, col);
  board [row] [col] [dim] = 0;
}
void movepiecelog(byte dim, byte startcol, byte startrow, byte endcol, byte endrow){	//function to move the selected piece in the memory
  if (targetpieceid != 0){
    removepiecelog(dim, endcol, endrow);
  }
	board [startrow] [startcol] [dim] = 0; //clear the field of the figure to move
	board [endrow] [endcol] [dim] = movingpieceid; //place the piece at the target location
	if (1 == dim) {pieces [movingpieceid] [2] = 0;}
}
void movesteppers(){ //runs all steppers to their set position
  xStepper.runToPosition(); //column stepper
  yStepper.runToPosition(); //row stepper
  zStepper.runToPosition(); //height stepper
}
void moveheadto(byte col, byte row, int execsteppermove = 1){//moves the head to the chosen square on the board
  headrow = row; //y
  headcol = col; //x
  if (execsteppermove == 1){
    xStepper.moveTo(col * xResolution);
    yStepper.moveTo(row * yResolution);
    movesteppers();
  }
}
void lowerhead(unsigned int pieceheight = getpieceheight(getpiecetype(getpieceid(1, headrow, headcol)))){zStepper.moveTo(pieceheight);movesteppers();}
void lifthead(){zStepper.moveTo(0);movesteppers();}
void magnetenable(){digitalWrite(magnetPin, 1);delay(1000);analogWrite(magnetPin, 127);}
void magnetdisable(){digitalWrite(magnetPin, 0);}
void removepiecephys(byte col, byte row){ //function to move a piece to the outside of the playfield, doesnt assume the piece variables to be set correctly
  int removedpieceid = getpieceid(1, row, col);
  int removedpieceheight = getpieceheight(getpiecetype(removedpieceid));
  moveheadto(col, row);
  lowerhead();
  magnetenable();
  lifthead();
  moveheadto(getoutcol(removedpieceid), getoutrow(removedpieceid));
  lowerhead(removedpieceheight);
  magnetdisable();
  lifthead();
}
void movepiecephys(byte startcol, byte startrow, byte endcol, byte endrow){	//function to move the selected piece on the board, assumes the piece variables to be set correctly
	if (targetpieceid != 0){
		removepiecephys(endcol, endrow);
	}
  moveheadto(startcol, startrow);
  lowerhead();
  magnetenable();
  lifthead();
  moveheadto(endcol, endrow);
  lowerhead(movingpieceheight);
  magnetdisable();
  lifthead();
}
void zerostepper(){ //will zero all steppers
  while (digitalRead(xEndlage) == 0){
    xStepper.move(-1);
    movesteppers();
  }
  while (digitalRead(yEndlage) == 0){
    yStepper.move(-1);
    movesteppers();
  }
  while (digitalRead(zEndlage) == 0){
    zStepper.move(-1);
    movesteppers();
  }
  xStepper.setCurrentPosition(0);
  yStepper.setCurrentPosition(0);
  zStepper.setCurrentPosition(0);
  moveheadto(0, 0, 0);
}
void execplayermove(){ //executes the move the player input
  readpiecedata(1, inpstartcol, inpstartrow, inpendcol, inpendrow);
  movepiecephys(inpstartcol, inpstartrow, inpendcol, inpendcol);
  movepiecelog(1, inpstartcol, inpstartrow, inpendcol, inpendcol);
  byte i = 0;
  while (i < 5){
    if (activeplayercol == 2){
      last3moves [0] [1 + i] = last3moves [0] [i];
      last3moves [1] [1 + i] = last3moves [1] [i];
      last3moves [2] [1 + i] = last3moves [2] [i];
      last3moves [3] [1 + i] = last3moves [3] [i];
    }
    if (activeplayercol == 1){
      last3moves [4] [1 + i] = last3moves [4] [i];
      last3moves [5] [1 + i] = last3moves [5] [i];
      last3moves [6] [1 + i] = last3moves [6] [i];
      last3moves [7] [1 + i] = last3moves [7] [i];
    }
    i++;
  }
  if (activeplayercol == 2){
    last3moves [0] [0] = inpstartcol;
    last3moves [1] [0] = inpstartrow;
    last3moves [2] [0] = inpendcol;
    last3moves [3] [0] = inpendrow;
  }
  if (activeplayercol == 1){
    last3moves [4] [0] = inpstartcol;
    last3moves [5] [0] = inpstartrow;
    last3moves [6] [0] = inpendcol;
    last3moves [7] [0] = inpendrow;
  }
}
void removepiece(byte col, byte row){ //wrapper to prepare all variables for the remove phys/log functions also ensures that they are called in the correct order
  removepiecephys(col, row);
  removepiecelog(1, col, row);
}
void movepiece(byte startcol, byte startrow, byte endcol, byte endrow){ //wrapper to prepare all variables for the move phys/log functions also ensures that they are called in the correct order
  readpiecedata(1, startcol, startrow, endcol, endrow);
  movepiecephys(startcol, startrow, endcol, endrow);
  movepiecelog(1, startcol, startrow, endcol, endrow);
}
void restorefield(){for (byte i = 1; i < 33; i++) {movepiece(getcurrcol(i), getcurrrow(i), getdefcol(i), getdefrow(i));}} //puts every piece on its default position
void(* resetFunc) (void) = 0;
void reset(){Serial.println("resetting soon");delay(100);resetFunc();}
bool ischeckmate(byte playercolour){//checks if the player is standing in checkmate
  boardcopydimension(1, 2);
  byte enemycolour;
  if (playercolour == 1){enemycolour = 2;}
  if (playercolour == 2){enemycolour = 1;}
  if (checkformate(2, playercolour) == 1){
    for (int i = 1; i < 9; i++){
      for (int ii = 1; ii < 9; ii++){
        for (int iii = 1; iii < 9; iii++){
          for (int iiii = 1; iiii < 9; iiii++){
            readpiecedata(2, i, ii, iii, iiii);
            if (validatemove(2, i, ii, iii, iiii, enemycolour) != 0){
              movepiecelog(2, i, ii, iii, iiii);
              if (checkformate(2, playercolour) == 0){return 0;}
              boardcopydimension(1, 2);
            }
          }
        }
      }
    }
    return 1;
  }
  return 0;
}
bool haslegalmove(byte playercol){//checks if the player has any valid moves
  for (int i = 1; i < 9; i++){
    for (int ii = 1; ii < 9; ii++){
      for (int iii = 1; iii < 9; iii++){
        for (int iiii = 1; iiii < 9; iiii++){
          readpiecedata(1, i, ii, iii, iiii);
          if (validatemove(1, i, ii, iii, iiii, playercol) != 0){
            boardcopydimension(1, 2);
            movepiecelog(2, i, ii, iii, iiii);
            if (checkformate(2, playercol) == 0){
              return 1;
            }
          }
        }
      }
    }
  }
  return 0;
}
void help(int mode = 0){//helptext for the debug() function
  Serial.println("Available Commands:");
  if (mode == 0){
    Serial.println("##############");
    Serial.println("help");
    Serial.println("reset");
    Serial.println("exit");
    Serial.println("move");
    Serial.println("movelog");
    Serial.println("movephy");
    Serial.println("movehead");
    Serial.println("stepper");
    Serial.println("#############");
 }
}
void debug(){//hell aka usb serial debugging
  String buffer;
  Serial.println("starting loop");
  while (Serial){//prevent the code from softlocking here if no usb is connected and the debug button is pressed or if usb is disconnected during debugging
    help();
    buffer = Serial.readStringUntil('\n');
    Serial.println(buffer);
    if (buffer == "reset"){reset();}
    if (buffer == "help"){help();}
    if (buffer == "exit"){break;}
    if (buffer == "move"){
      byte buf1 = Serial.parseInt(); //read into temporary variables to prevent parseInt from messing up the order of the inputs
      byte buf2 = Serial.parseInt(); //parseInt for some damn reason inverts the order of the variables so
      byte buf3 = Serial.parseInt(); //movepiece(Serial.parseInt(), Serial.parseInt(), Serial.parseInt(), Serial.parseInt());
      byte buf4 = Serial.parseInt(); //so the last input would land in the first position and the last input in the first position
      Serial.read(); //parse Serial into nothingness to clear out remaining line breaks
      movepiece(buf1, buf2, buf3, buf4);
    }
    if (buffer == "movelog"){
      byte buf1 = Serial.parseInt();
      byte buf2 = Serial.parseInt();
      byte buf3 = Serial.parseInt();
      byte buf4 = Serial.parseInt();
      byte buf5 = Serial.parseInt();
      Serial.read();
      movepiecelog(buf1, buf2, buf3, buf4, buf5);
    }
    if (buffer == "movephy"){
      byte buf1 = Serial.parseInt();
      byte buf2 = Serial.parseInt();
      byte buf3 = Serial.parseInt();
      byte buf4 = Serial.parseInt();
      Serial.read();
      movepiecephys(buf1, buf2, buf3, buf4);
    }
    if (buffer == "movehead"){
      byte buf1 = Serial.parseInt();
      byte buf2 = Serial.parseInt();
      byte buf3 = Serial.parseInt();
      Serial.read();
      moveheadto(buf1, buf2, buf3);
    }
    if (buffer == "stepper"){ //manually move the steppers without modifying any variables
      Serial.println("Enter Amount to move xStepper");
      byte buf1 = Serial.parseInt();
      Serial.println("Enter Amount to move yStepper");
      byte buf2 = Serial.parseInt();
      Serial.println("Enter Amount to move zStepper");
      byte buf3 = Serial.parseInt();
      Serial.read();
      xStepper.move(buf1);
      yStepper.move(buf2);
      zStepper.move(buf3);
      movesteppers();
    }
  }
  Serial.println("###end of debug mode###");
}
int main (){
  init(); //arduino setup stuff
  Serial.begin(9600); //setup serial for usb debugging
  Serial.setTimeout(4294967295);
  
  lcd.begin(16, 4); //start the lcd library

  xStepper.setMaxSpeed(100); //initialize the accelStepper settings
  xStepper.setAcceleration(10);
  xStepper.setPinsInverted(0, 0, 0);

  yStepper.setMaxSpeed(100);
  yStepper.setAcceleration(10);
  yStepper.setPinsInverted(0, 0, 0);
  
  zStepper.setMaxSpeed(50);
  zStepper.setAcceleration(10);
  zStepper.setPinsInverted(0, 0, 0);

	//populate the board array with chess pieces
	//board [row] [col] [dim]
  //xx0 is the default chess board
  //xx1 is the current board (synced with physical pieces)
  //xx2 is a copy of the current board for preventing the player from mating themselves
	board [1] [1] [0] = 1; //rook
	board [1] [2] [0] = 2; //knight
	board [1] [3] [0] = 3; //bishop
	board [1] [4] [0] = 4; //queen
	board [1] [5] [0] = 5; //king
	board [1] [6] [0] = 6; //bishop
	board [1] [7] [0] = 7; //knight
	board [1] [8] [0] = 8; //rook
	board [2] [1] [0] = 9; //pawn
	board [2] [2] [0] = 10; //pawn
	board [2] [3] [0] = 11; //pawn
	board [2] [4] [0] = 12; //pawn
	board [2] [5] [0] = 13; //pawn
	board [2] [6] [0] = 14; //pawn
	board [2] [7] [0] = 15; //pawn
	board [2] [8] [0] = 16; //pawn
	board [7] [1] [0] = 17; //pawn
	board [7] [2] [0] = 18; //pawn
	board [7] [3] [0] = 19; //pawn
	board [7] [4] [0] = 20; //pawn
	board [7] [5] [0] = 21; //pawn
	board [7] [6] [0] = 22; //pawn
	board [7] [7] [0] = 23; //pawn
	board [7] [8] [0] = 24; //pawn
	board [8] [1] [0] = 25; //rook
	board [8] [2] [0] = 26; //knight
	board [8] [3] [0] = 27; //bishop
	board [8] [4] [0] = 28; //queen
	board [8] [5] [0] = 29; //king
	board [8] [6] [0] = 30; //bishop
	board [8] [7] [0] = 31; //knight
	board [8] [8] [0] = 32; //rook
  //copy the positions of the pieces on the freshly initialized default board into the pieces array to serve as the default positions for the board cleanup function
  byte col = 0;
  byte row = 0;
  while (row < 10){
		while (col < 10){
      pieces [getpieceid(0, row, col)] [7] = row;
      pieces [getpieceid(0, row, col)] [8] = col;
			++col;
		}
		++row;
		col = 0;
	}
	row = 0;
	boardcopydimension(0, 1); //copy the default board onto the current board
  Serial.println("init done now entering game loop");
  while(1){
    activeplayercol = 2; //set the player colour to white to prepare for a new game after last has been finished
	  gamestate = 1; //start a new game
    dumplogicfieldtoshell(1);
    zerostepper(); //null all steppers to restore lost accuracy
    while (gamestate == 1){ //1 == game is ongoing
      dumplogicfieldtoshell(1); //dump the imitation of the physical field to shell
      if (haslegalmove(activeplayercol) == 0){lcd.setCursor(0, 0);lcd.print("Remis");break;}
      readinput(); //get the player input
      if (gamestate == 0){break;}
      readpiecedata(1, inpstartcol, inpstartrow, inpendcol, inpendrow);
      updatecastlingallowed(); //check if castling is possible (used in validatemove)
      movevalid = validatemove(1, inpstartcol, inpstartrow, inpendcol, inpendrow, activeplayercol); //check if the selected move is legal, also save the return value for further use in special cases
      if (movevalid != 0){ //check if the move itself obey the rules by wich the pieces can move
        readpiecedata(1, inpstartcol, inpstartrow, inpendcol, inpendrow); //read the data for the move the player made
        boardcopydimension(1, 2); //update the second board to the current state
        readpiecedata(2, inpstartcol, inpstartrow, inpendcol, inpendrow); //prepare the piecedata for the check check
        movepiecelog(2, inpstartcol, inpstartrow, inpendcol, inpendrow); //modify the second board as if the select move was already done
        if (0 == checkformate(2, activeplayercol)){ //prevent the player from mating themselves only checks for mate after the move
          readpiecedata(1, inpstartcol, inpstartrow, inpendcol, inpendrow); //restore the piece data as it was modified for the check check
          if (movevalid == 2){enpassantpossible = 0; enpassantpossibleon = movingpieceid;} //set variables to make en passant possible on double long pawn move
          execplayermove(); //make the move the player entered
          if (movevalid == 3){removepiece(inpendcol, inpstartrow);} //remove the pawn that just got revolutiond
          enpassantpossible++; //set enpassantpossible to 1 for the next turn if it has been set to 0 by a doubble long pawn move
          //move the tower for a castle the target field should already be empty due to a castle being ilegal if all target pieces arent emtpy
          if (movevalid == 11){movepiece(1, 1, 4, 1);} //black queenside castle
          if (movevalid == 12){movepiece(8, 1, 6, 1);} //black kingside castle
          if (movevalid == 13){movepiece(1, 8, 4, 8);} //white queenside castle
          if (movevalid == 14){movepiece(8, 8, 6, 8);} //white kingside castle
          lcd.setCursor(0, 0);
          if ((last3moves [0] [0] == last3moves [0] [1] && last3moves [0] [1] == last3moves [0] [2] && last3moves [0] [2] == last3moves [0] [3] && last3moves [0] [3] == last3moves [0] [4] && last3moves [0] [4] == last3moves [0] [5] && last3moves [1] [0] == last3moves [1] [1] && last3moves [1] [1] == last3moves [1] [2] && last3moves [1] [2] == last3moves [1] [3] && last3moves [1] [3] == last3moves [1] [4] && last3moves [1] [4] == last3moves [1] [5] && last3moves [2] [0] == last3moves [2] [1] && last3moves [2] [1] == last3moves [2] [2] && last3moves [2] [2] == last3moves [2] [3] && last3moves [2] [3] == last3moves [2] [4] && last3moves [2] [4] == last3moves [2] [5] && last3moves [3] [0] == last3moves [3] [1] && last3moves [3] [1] == last3moves [3] [2] && last3moves [3] [2] == last3moves [3] [3] && last3moves [3] [3] == last3moves [3] [4] && last3moves [3] [4] == last3moves [3] [5]) || (last3moves [4] [0] == last3moves [4] [1] && last3moves [4] [1] == last3moves [4] [2] && last3moves [4] [2] == last3moves [4] [3] && last3moves [4] [3] == last3moves [4] [4] && last3moves [4] [4] == last3moves [4] [5] && last3moves [5] [0] == last3moves [5] [1] && last3moves [5] [1] == last3moves [5] [2] && last3moves [5] [2] == last3moves [5] [3] && last3moves [5] [3] == last3moves [5] [4] && last3moves [5] [4] == last3moves [5] [5] && last3moves [6] [0] == last3moves [6] [1] && last3moves [6] [1] == last3moves [6] [2] && last3moves [6] [2] == last3moves [6] [3] && last3moves [6] [3] == last3moves [6] [4] && last3moves [6] [4] == last3moves [6] [5] && last3moves [7] [0] == last3moves [7] [1] && last3moves [7] [1] == last3moves [7] [2] && last3moves [7] [2] == last3moves [7] [3] && last3moves [7] [3] == last3moves [7] [4] && last3moves [7] [4] == last3moves [7] [5])){
            lcd.print("Remis");
            lcd.setCursor(0,3);
            lcd.print("Wiederholung");
            break;
          }
          if (activeplayercol == 1){
            activeplayercol = 2;
            if (ischeckmate(activeplayercol) == 1){lcd.print("Weiss Matt      ");break;}
            else {lcd.print("Weiss ist am Zug");}
          } //change the player colour after a succesful move
          else if (activeplayercol == 2){
            activeplayercol = 1;
            if (ischeckmate(activeplayercol) == 1){lcd.print("Schwarz Matt    ");break;}
            else {lcd.print("Schwarz am Zug  ");}
          } //same as above but for the other direction
          else  {Serial.println("?"); lcd.setCursor(0, 0); lcd.print("error no colour");} //you are not supposed to be here!!!
        }
        else{//actions for when the move would endanger the king
          lcd.setCursor(0, 2);
          lcd.print("Check");
        }
      }
    }

    Serial.print("you left the game loop btw");
    void restorefield();
  }
}