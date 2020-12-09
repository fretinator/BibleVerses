/*
 Reads lines from a file and displays on screen.
 I am using this to display bible verses.
*/

#include <SPI.h>
#include <SD.h>

// LCD constants
char ESC = 0xFE; // Used to issue command
const char CLS = 0x51;
const char CURSOR = 0x45;
const char LINE1_POS = 0x00;
const char LINE2_POS = 0x40;
const char LINE3_POS = 0x14;
const char LINE4_POS = 0x54;
const String NOT_FOUND = "n/a";
File myFile;
const char* BIBLE_FILE = "bible.txt";
bool card_opened = false;
bool file_opened = false;
const int max_buffer = 512;
char vBuffer[max_buffer + 1]; // Read up to max_buffer characters, 1 char for '\0'
const int SCREEN_ROWS = 4;
const int SCREEN_COLS = 20;
const int SCREEN_CHARS = 80;
const int VERSES_DELAY = 10 * 1000;
const String TRUNC_CHARS = "...";
const String NULL_STRING = "@@@@@@@";

char getLinePos(int line_num) {
  switch(line_num) {
    case 1:
      return LINE1_POS;
    case 2:
      return LINE2_POS;
    case 3:
      return LINE3_POS;
    case 4:
      return LINE4_POS;
    default:
      return LINE1_POS; // Default to line 1
  }
}
void clearScreen() {
  Serial2.write(ESC);
  Serial2.write(CLS);  
  delay(10);
}

// Lines are 1 based
void printScreen(const char* line, bool cls = true, int whichLine = 1) {
  // Clear screen
  if(cls) {
    clearScreen();
  }

  Serial2.write(ESC);
  Serial2.write(CURSOR);
  Serial2.write(getLinePos(whichLine));
  delay(10);
  Serial2.print(line);
}

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  Serial2.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // Setup LCD
    // Initialize LCD module
  Serial2.write(ESC);
  Serial2.write(0x41);
  Serial2.write(ESC);
  Serial2.write(0x51);
  
  // Set Contrast
  Serial2.write(ESC);
  Serial2.write(0x52);
  Serial2.write(40);
  
  // Set Backlight
  Serial2.write(ESC);
  Serial2.write(0x53);
  Serial2.write(8);

  Serial2.write(ESC);
  Serial2.write(CLS);
  
  Serial2.print(" NKC Electronics");
  
  // Set cursor line 2, column 0
  Serial1.write(ESC);
  Serial1.write(CURSOR);
  Serial1.write(LINE2_POS);
  
  Serial2.print(" 16x2 Serial LCD");
  
  delay(1000);


  Serial.print("Initializing SD card...");

  if (!SD.begin(53)) {
    Serial.println("initialization failed!");
  } else {
    Serial.println("initialization done.");
    card_opened = true;
  }
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another

  // open the file for reading:
  myFile = SD.open(BIBLE_FILE);
  
  if (myFile) {
    Serial.println("Bible file opened");
    file_opened = true;
  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening bible file");
  }

  if(!card_opened) {
    printScreen("Unable to open SD card");
  } else {
    if(!file_opened) {
      printScreen("Unable to open bible file");
    } else {
      printScreen("Bible initialized");
    }
  }

  delay(2000);
}

String getNextVerse() {
  if(!myFile.available()) {
    myFile.seek(0);
    if(!myFile.available()) {
      Serial.println("Empty file");
      return NULL_STRING;
    }
  }

  int curBufferPos = 0;
  char nextChar = '\0';
  bool verseComplete = false;
  
  while(myFile.available() && !verseComplete && curBufferPos < (max_buffer - 1)) {
    if(myFile.peek() == '\n') {
      myFile.read();
      vBuffer[curBufferPos] = '\0';
      verseComplete = true;
    } else {
      nextChar = myFile.read();
      vBuffer[curBufferPos++] = nextChar;
    }
  }

  if(!verseComplete) { // We reached EOF, last verse
    vBuffer[curBufferPos] = '\0';
  }

  if(strlen(vBuffer) > 0) {
    Serial.print("Current verse: ");
    Serial.println(vBuffer);
    return String(vBuffer);
  } else {
    return NULL_STRING;
  }
}

int getNextChunkPos(int startPos, String verse, int max_chars,
    bool truncate) {
  int spacePos = 0;
  int lastPos = startPos + max_chars;
  
  if(lastPos > verse.length()) {
    lastPos = verse.length();
  } else {
    spacePos = verse.lastIndexOf(' ', lastPos);
    if(spacePos != -1 && spacePos > startPos) {
      lastPos = spacePos;
    }

    if(truncate && 
        ((lastPos - startPos) > (max_chars - TRUNC_CHARS.length()))) {
      // We need to allow room for ...
      spacePos = verse.lastIndexOf(' ', lastPos - 1);
      
      if(spacePos != -1 && spacePos > startPos) {
        lastPos = spacePos; 
      }
    }
  }

  return lastPos;
}
 
void displayVerse(String verse) {
  int startPos = 0;
  int curLine = 1;
  int lastPos = 0;
  bool moreChunks = true;
  String line = "";

  while(moreChunks) {
    lastPos = getNextChunkPos(startPos, verse, SCREEN_COLS, curLine == 4);

    if(lastPos == -1) {
      moreChunks = false;
    } else {
      moreChunks = lastPos < verse.length();

      if(curLine == 4 && moreChunks) {
        line = verse.substring(startPos, lastPos) + TRUNC_CHARS;
      } else {
        line = verse.substring(startPos, lastPos);
      }

      printScreen(line.c_str(), curLine == 1, curLine);
  
      if(moreChunks) {
        startPos = lastPos;
        
        if(verse.charAt(startPos) == ' ') {
          startPos++;
        }
        
        if(++curLine == 5) {
          curLine = 1;
          delay(VERSES_DELAY); // Leave this part of the verse up
        }
      } else {
          delay(VERSES_DELAY); // we are done with verse
      }
    }
  }
}
void showBible() {
  String verse = getNextVerse();
  while(verse != NULL_STRING) {
    displayVerse(verse);
    verse = getNextVerse();
  }

  // We somehow ran out of verses
  printScreen("No more verses [error]");
  while(1);
}

void loop() {
  if(card_opened && file_opened) {
    showBible();
  }
}
