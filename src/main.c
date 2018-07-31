/*
/////////////////////////////////////////////////////////////////////////////
This code is free software.
	Not "free" as in "lol, here's a 20 page license file even I've never read. if you modify my code, your code now belongs to the 20 page license file too. dont even think about using this code in a way that doesnt align with my ideology. its freedom, I promise"
		Note: CC0 is not 20 pages and can be summarized in 3 words.
	"Free" as in "Do whatever you want, just credit me if you decide to give out the source code because i worked hard" For actual license, see LICENSE file.
/////////////////////////////////////////////////////////////////////////////
https://forums.libsdl.org/viewtopic.php?p=15228

todo - Add saving
todo - add optional update checker
	Don't do with libGeneralGood
		Should I use libCurl or SDL_Net?
todo - Add icon to the exe
	todo - Redo some of the more ugly icons, like BPM

SDL_SetClipboardText
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h> // For htons

#include <GeneralGoodConfig.h>
#include <GeneralGood.h>
#include <GeneralGoodExtended.h>
#include <GeneralGoodGraphics.h>
#include <GeneralGoodSound.h>
#include <GeneralGoodImages.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#ifndef NO_FANCY_DIALOG
	#include <nfd.h>
#endif

#include "fonthelper.h"
#include "luaDofileEmbedded.h"

///////////////////////////////////////
#define APPLICATIONVERSION 1
#define SETTINGSVERSION 1
#define HOTKEYVERSION 1
#define SONGFORMATVERSION 1
///////////////////////////////////////

#define ISTESTINGMOBILE 0
#define DISABLESOUND 0

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define s8 int8_t
#define s16 int16_t
#define s32 int32_t

//#define fileByteOrder(x) htons(x)
//#define hostByteOrder(x) ntohs(x)

#define BGMODE_SINGLE 1
#define BGMODE_PART 2

#define LUAREGISTER(x,y) lua_pushcfunction(L,x);\
	lua_setglobal(L,y);

#define CONSTCHARW (singleBlockSize/2)

// length of MAXINTPLAE in base 10
#define MAXINTINPUT 7
// No values bigger than this
#define MAXINTPLAE 1000000

#define CLICKTOGOBACK "Click this text to go back."

#define AUDIOGEARSPACE 5

#define BONUSENDCHARACTER 1 // char value to signal the true end of the message text in easyMessage

#define SAVEENDMARKER "FSMG"
#define SAVEFORMATMAGIC "GMSF"

////////////////////////////////////////////////
#include "main.h"
////////////////////////////////////////////////
u8 optionPlayOnPlace=1;
u8 optionZeroBasedPosition=0;
u8 optionDoFancyPage=1;
u8 optionDoCenterPlay=0;
////////////////////////////////////////////////
// From libGeneralGood
extern int _generalGoodRealScreenWidth;
extern int _generalGoodRealScreenHeight;
////////////////////////////////////////////////
int screenWidth;
int screenHeight;
int logicalScreenWidth;
int logicalScreenHeight;
int globalDrawXOffset;
int globalDrawYOffset;

lua_State* L = NULL;

u16 totalUI=0;
u16 totalVisibleUI=0;
u16 uiScrollOffset=0;
uiElement* myUIBar=NULL;
SDL_Keycode* uiHotkeys=NULL;

CrossTexture* playButtonImage;
CrossTexture* stopButtonImage;
CrossTexture* yellowPlayButtonImage;

CrossTexture* uiScrollImage;

u16 songWidth=400;
u16 songHeight=14;

double generalScale;

u16 singleBlockSize=32;

u8 isMobile;

s32 uiNoteIndex=0;
u16 totalNotes=0;

u8 currentlyPlaying=0;
s32 currentPlayPosition;
s32 nextPlayPosition;
u64 lastPlayAdvance=0;

// Only the width of a page can change and it depends on the scale set by the user
u16 pageWidth=25;
#define pageHeight 14

// How much of the page's height you can see without scrolling
u16 visiblePageHeight;
// These are display offsets
s32 songYOffset=0;
s32 songXOffset=0;

// 0 for normal PC mode
// 1 for split mobile mode
u8 backgroundMode=BGMODE_SINGLE;
CrossTexture* bigBackground=NULL;
CrossTexture** bgPartsEmpty=NULL;
CrossTexture** bgPartsLabel=NULL;

CrossTexture** noteImages=NULL;
CROSSSFX*** noteSounds=NULL;
noteInfo* extraNoteInfo=NULL;
u8* noteUIOrder=NULL; // Order of note IDs in the UI
SDL_Keycode* noteHotkeys=NULL;

noteSpot** songArray;

u32 bpm=100;

u8 repeatStartID=255;
u8 repeatEndID=255;
u8 audioGearID=255;
// Max X position the song goes to
u32 maxX=0;

int uiPageSize;

// Position in the song as a string
char positionString[12];

uiElement backButtonUI;
uiElement infoButtonUI;
uiElement volumeButtonUI; // Can use this for both master volume and audio gear volume

int uiUIScrollIndex=-1;

//extern SDL_Keycode lastSDLPressedKey;

const char noteNames[] = {'B','A','G','F','E','D','C','b','a','g','f','e','d','c'};

////////////////////////////////////////////////
// Code stolen from Happy Land.
// Displays whatever message you want. Text will wrap.
void easyMessage(char* _newMessage){
	char* currentTextboxMessage = malloc(strlen(_newMessage)+2);
	strcpy(currentTextboxMessage,_newMessage);

	// Step 1 - Put words into buffer with newlines
	//////////////////////////////////////////////////////////////////
	uint32_t _cachedMessageLength = strlen(currentTextboxMessage);
	currentTextboxMessage[_cachedMessageLength+1] = BONUSENDCHARACTER;
	uint32_t i;
	uint32_t j;
	// This will loop through the entire message, looking for where I need to add new lines. When it finds a spot that
	// needs a new line, that spot in the message will become 0.
	int _lastNewlinePosition=-1; // If this doesn't start at -1, the first character will be cut off
	for (i = 0; i < _cachedMessageLength; i++){
		if (currentTextboxMessage[i]==32){ // Only check when we meet a space. 32 is a space in ASCII
			currentTextboxMessage[i]='\0';
			if (bitmpTextWidth(&(currentTextboxMessage[_lastNewlinePosition+1]))>pageWidth*2*CONSTCHARW){
				uint8_t _didWork=0;
				for (j=i-1;j>_lastNewlinePosition+1;j--){
					//printf("J:%d\n",j);
					if (currentTextboxMessage[j]==32){
						currentTextboxMessage[j]='\0';
						_didWork=1;
						currentTextboxMessage[i]=32;
						_lastNewlinePosition=j;
						break;
					}
				}
				if (_didWork==0){
					currentTextboxMessage[i]='\0';
					_lastNewlinePosition=i+1;
				}
			}else{
				currentTextboxMessage[i]=32;
			}
		}
	}
	// This code will make a new line if there needs to be one because of the last word
	if (bitmpTextWidth(&(currentTextboxMessage[_lastNewlinePosition+1]))>pageWidth*2*CONSTCHARW){
		for (j=_cachedMessageLength-1;j>_lastNewlinePosition+1;j--){
			if (currentTextboxMessage[j]==32){
				currentTextboxMessage[j]='\0';
				break;
			}
		}
	}
	//////////////////////////////////////////////////////////////////
	// Step 2 - Display words
	//////////////////////////////////////////////////////////////////
	controlLoop();
	while(1){
		controlsStart();
		if (wasJustPressed(SCE_TOUCH)){
			break;
		}
		controlsEnd();
		startDrawing();
		int i;
		int _currentDrawPosition=0;
		for (i=0;;++i){
			drawString(&(currentTextboxMessage[_currentDrawPosition]),0,i*singleBlockSize);
			_currentDrawPosition+=strlen(&(currentTextboxMessage[_currentDrawPosition]));
			if (currentTextboxMessage[_currentDrawPosition+1]==BONUSENDCHARACTER){
				break;
			}else{
				_currentDrawPosition++;
			}
		}
		endDrawing();
	}
	free(currentTextboxMessage);
	controlLoop();
}

/*void laziestMessage(char* _myMessage){
	int _totalLines=ceil(strlen(_myMessage)/(pageWidth*2));
	char _duplicateMessage[strlen(_myMessage)+_totalLines];
	strcpy(_duplicateMessage,_myMessage);

	int i;
	for (i=1;i<_totalLines;++i){
		int _bytesToMove = strlen(_myMessage[])
		memmove()
	}
}*/

char* getDataFilePath(const char* _passedFilename){
	char* _fixedPathBuffer = malloc(strlen(_passedFilename)+strlen(getFixPathString(TYPE_DATA))+1);
	fixPath((char*)_passedFilename,_fixedPathBuffer,TYPE_DATA);
	return _fixedPathBuffer;
}

void saveHotkeys(){
	char* _settingsFilename = getDataFilePath("hotkeys.legSettings");
	FILE* fp = fopen(_settingsFilename,"w");
	if (fp!=NULL){
		u8 _tempHoldVersion = HOTKEYVERSION;
		fwrite(&_tempHoldVersion,sizeof(u8),1,fp);

		int i;
		fwrite(&totalNotes,sizeof(u16),1,fp);
		for (i=0;i<totalNotes;++i){
			fwrite(&(noteHotkeys[i]),sizeof(SDL_Keycode),1,fp);
		}
		fwrite(&totalUI,sizeof(u16),1,fp);
		for (i=0;i<totalUI;++i){
			fwrite(&(myUIBar[i].uniqueId),sizeof(u8),1,fp);
			fwrite(&(uiHotkeys[i]),sizeof(SDL_Keycode),1,fp);
		}
		fclose(fp);
	}else{
		printf("Could not write hotkey settings file to\n");
		printf("%s\n",_settingsFilename);
	}
	free(_settingsFilename);
}

void saveSettings(){
	char* _settingsFilename = getDataFilePath("generalSettings.legSettings");
	FILE* fp = fopen(_settingsFilename,"w");
	if (fp!=NULL){
		u8 _tempHoldVersion = SETTINGSVERSION;
		fwrite(&_tempHoldVersion,sizeof(u8),1,fp);

		fwrite(&optionPlayOnPlace,sizeof(u8),1,fp);
		fwrite(&optionZeroBasedPosition,sizeof(u8),1,fp);
		fwrite(&optionDoFancyPage,sizeof(u8),1,fp);
		fwrite(&optionDoCenterPlay,sizeof(u8),1,fp);
		fclose(fp);
	}else{
		printf("Could not write settings file to\n");
		printf("%s\n",_settingsFilename);
	}
	free(_settingsFilename);
}

void loadSettings(){
	char* _settingsFilename = getDataFilePath("generalSettings.legSettings");
	FILE* fp = fopen(_settingsFilename,"r");
	if (fp!=NULL){
		u8 _tempHoldVersion = SETTINGSVERSION;
		fread(&_tempHoldVersion,sizeof(u8),1,fp);
		if (_tempHoldVersion>=1){
			fread(&optionPlayOnPlace,sizeof(u8),1,fp);
			fread(&optionZeroBasedPosition,sizeof(u8),1,fp);
			fread(&optionDoFancyPage,sizeof(u8),1,fp);
			fread(&optionDoCenterPlay,sizeof(u8),1,fp);
		}
		fclose(fp);
	}
	free(_settingsFilename);
}

void loadHotkeys(){
	char* _settingsFilename = getDataFilePath("hotkeys.legSettings");
	FILE* fp = fopen(_settingsFilename,"r");
	if (fp!=NULL){
		u8 _tempReadVersion = HOTKEYVERSION;
		fread(&_tempReadVersion,sizeof(u8),1,fp);
		if (_tempReadVersion>=1){
			int i;
			fread(&totalNotes,sizeof(u16),1,fp);
			for (i=0;i<totalNotes;++i){
				fread(&(noteHotkeys[i]),sizeof(SDL_Keycode),1,fp);
			}
			fread(&totalUI,sizeof(u16),1,fp);
			for (i=0;i<totalUI;++i){
				u8 _lastLoadedID;
				fread(&(_lastLoadedID),sizeof(u8),1,fp);
				if (_lastLoadedID!=U_NOTUNIQUE){
					int j;
					for (j=0;j<totalUI;++j){
						if (myUIBar[j].uniqueId==_lastLoadedID){
							fread(&(uiHotkeys[j]),sizeof(SDL_Keycode),1,fp);
							break;
						}
					}
				}
			}
		}
		fclose(fp);
	}
	free(_settingsFilename);
}

u8 getUINoteID(){
	return noteUIOrder[uiNoteIndex];
}

u8* getGearVolume(u8* _passedExtraData){
	return &(_passedExtraData[AUDIOGEARSPACE*sizeof(u8)*2]);
}
int touchXToBlock(int _passedTouchX){
	return floor((_passedTouchX-globalDrawXOffset)/singleBlockSize);
}
int touchYToBlock(int _passedTouchY){
	return floor((_passedTouchY-globalDrawYOffset)/singleBlockSize);
}

char isBigEndian(){
	volatile u32 i=0x01234567;
	return !((*((u8*)(&i))) == 0x67);
}

s16 fixShort(s16 _passedShort){
	if (isBigEndian()){ // I've never tested this
		printf("Big boy!\n");
		// Swap them because C# stores in files as little endian
		s16 _readASBigEndian;
		((u8*)(&_readASBigEndian))[0] = ((u8*)(&_passedShort))[1];
		((u8*)(&_readASBigEndian))[1] = ((u8*)(&_passedShort))[0];
		return _readASBigEndian;
	}else{
		return _passedShort;
	}
}

void setSongXOffset(int _newValue){
	songXOffset = _newValue;
	sprintf(positionString,"%d/%d",songXOffset+!optionZeroBasedPosition,songWidth);
}

// Will memory leak audio gears
void clearSong(){
	setSongXOffset(0);
	setSongWidth(songArray,songWidth,400);
	songWidth=400;
	int i;
	for (i=0;i<songHeight;++i){
		int j;
		for (j=0;j<songWidth;++j){
			songArray[i][j].id=0;
			songArray[i][j].extraData=NULL;
		}
	}
}

char* sharedFilePicker(char _isSaveDialog, const char* _filterList, char _forceExtension){
	#ifdef NO_FANCY_DIALOG
		#ifdef MANUALPATHENTRY
			printf("Input path:\n");
			char* _readLine=NULL;
			size_t _readLineBufferSize = 0;
			getline(&_readLine,&_readLineBufferSize,stdin);
			removeNewline(_readLine);
			return _readLine;
		#else // Input for mobile
			controlLoop();
			char* _userInput = calloc(1,pageWidth*2+1);
			char _isDone=0;
			SDL_StartTextInput();
			while (!_isDone){
				SDL_Event event;
				if (SDL_PollEvent(&event)) {
					switch (event.type) {
						case SDL_QUIT:
							XOutFunction();
							break;
						case SDL_TEXTINPUT:
							if (_userInput!=NULL){
								// Skip some invalid characters
								if (event.text.text[0]==' ' || event.text.text[0]=='/' || event.text.text[0]=='?' || event.text.text[0]=='%' || event.text.text[0]=='*' || event.text.text[0]==':' || event.text.text[0]=='|' || event.text.text[0]=='\"' || event.text.text[0]=='<' || event.text.text[0]=='>'){
									break;
								}
								// Add character the user typed as long as it's not going to overflow
								if (strlen(_userInput)<pageWidth*2){
									strcat(_userInput, event.text.text);
								}
							}
							break;
						case SDL_KEYDOWN:
							if (_userInput!=NULL){
								if (event.key.keysym.sym == SDLK_BACKSPACE){
									if (strlen(_userInput)>0){
										_userInput[strlen(_userInput)-1]='\0';
									}
								}else if (event.key.keysym.sym == SDLK_KP_ENTER || event.key.keysym.sym==SDLK_RETURN){
									_isDone=1;
								}else if (event.key.keysym.sym == SDLK_ESCAPE){
									free(_userInput);
									_userInput=NULL;
									_isDone=1;
								}
							}
							break;
						case SDL_FINGERDOWN:
							;
							int _foundY = event.tfinger.y * _generalGoodRealScreenHeight;
							if(_foundY<singleBlockSize){
								_isDone=1;
							}else if  (_foundY<singleBlockSize*2){
								free(_userInput);
								_userInput=NULL;
								_isDone=1;
							}
							break;
					}
				}

				startDrawing();
				drawRectangle(0,0,logicalScreenWidth,singleBlockSize,0,255,0,255);
					drawString("Done",0,CONSTCHARW/2);
				drawRectangle(0,singleBlockSize,logicalScreenWidth,singleBlockSize,255,0,0,255);
					drawString("Cancel",0,CONSTCHARW/2+singleBlockSize);
				drawRectangle(0,singleBlockSize*2,logicalScreenWidth,singleBlockSize,255,255,255,255);
					drawString(_userInput,0,CONSTCHARW/2+singleBlockSize*2);
				endDrawing();
			}
			SDL_StopTextInput();

			controlsResetEmpty(); // Because we didn't catch the finger up or anything
			if (_userInput!=NULL){
				char* _completeFilepath = getDataFilePath(_userInput);
				free(_userInput);
				return _completeFilepath;
			}else{
				return NULL;
			}
		#endif
	#else
		nfdchar_t *outPath = NULL;
		nfdresult_t result;
		if (_isSaveDialog){
			result = NFD_SaveDialog( _filterList, NULL, &outPath );
		}else{
			result = NFD_OpenDialog( _filterList, NULL, &outPath );
		}
		if (result == NFD_OKAY){
			// If we're saving, add a file extension if the user didn't give one
			if (_isSaveDialog && _forceExtension){
				signed int i;
				char _foundDot=0;
				for (i=strlen(outPath)-1;i>=0;--i){
					if (outPath[i]=='\\' || outPath[i]=='/'){
						_foundDot=0;
						break;
					}else if (outPath[i]=='.'){
						_foundDot=1;
						break;
					}
				}
				if (!_foundDot){
					char* _newPath = malloc(strlen(outPath)+strlen(".GMSF")+1);
					strcpy(_newPath,outPath);
					strcat(_newPath,".GMSF");
					free(outPath);
					outPath = _newPath;
				}
			}
			return outPath;
		}else if ( result == NFD_CANCEL ){
			return NULL;
		}else{
			printf("Error: %s\n", NFD_GetError() );
			return NULL;
		}
	#endif
}

// This function returns malloc'd string or NULL
char* selectLoadFile(){
	return sharedFilePicker(0,"GMSF,gtmusic,AngryLegGuy,mylegguy",0);
}

char* selectSaveFile(){
	return sharedFilePicker(1,"GMSF",1);
}

void findMaxX(){
	maxX=0;
	int i,j;
	for (i=0;i<songWidth;++i){
		for (j=0;j<songHeight;++j){
			if (songArray[j][i].id!=0){
				maxX=i;
				break;
			}
		}
	}
}

void controlLoop(){
	controlsStart();
	controlsEnd();
}

void int2str(char* _outBuffer, int _inNumber){
	sprintf(_outBuffer,"%d",_inNumber);
}

void _addNumberInput(long* _outNumber, char* _outBuffer, int _addNumber){
	if (*_outNumber>=MAXINTPLAE){
		drawRectangle(0,0,logicalScreenWidth,logicalScreenHeight,255,0,0,255);
	}else{
		*_outNumber*=10;
		*_outNumber+=_addNumber;
		int2str(_outBuffer,*_outNumber);
	}
}
void _delNumberInput(long* _outNumber, char* _outBuffer){
	if (*_outNumber==0){
		drawRectangle(0,0,logicalScreenWidth,logicalScreenHeight,255,0,0,255);
	}else{
		*_outNumber/=10;
		int2str(_outBuffer,*_outNumber);
	}
}
long getNumberInput(const char* _prompt, long _defaultNumber){
	controlLoop();
	long _userInput=_defaultNumber;
	char _userInputAsString[MAXINTINPUT+1]={'\0'};
	
	int _backspaceButtonX = 0;
	int _exitButtonX = CONSTCHARW*4;
	int _doneButtonX = CONSTCHARW*9;
	char* _backspaceString="Del ";
	char* _exitString="Exit ";
	char* _doneString="Done";
	int _backspaceWidth=strlen(_backspaceString)*CONSTCHARW;
	int _exitWidth=strlen(_exitString)*CONSTCHARW;
	int _doneWidth=strlen(_doneString)*CONSTCHARW;
	
	char _shouldExit=0;

	int2str(_userInputAsString,_userInput);
	//itoa(num, buffer, 10);
	while (!_shouldExit){
		startDrawing(); // Start drawing here because we draw a red background if we try to input too much text
		controlsStart();
		#if RENDERER == REND_SDL
			if (lastSDLPressedKey!=SDLK_UNKNOWN){
				int _pressedValue=-1;

				// Special cases first
				if (lastSDLPressedKey==SDLK_BACKSPACE){
					_delNumberInput(&_userInput,_userInputAsString);
				}else if (lastSDLPressedKey==SDLK_KP_ENTER || lastSDLPressedKey==SDLK_RETURN){
					_shouldExit=1;
				}else if (lastSDLPressedKey==SDLK_ESCAPE){
					_shouldExit=1;
					_userInput=_defaultNumber;
				}else{
					switch (lastSDLPressedKey){
						case SDLK_KP_0:
						case SDLK_0:
							_pressedValue=0;
							break;
						case SDLK_KP_1:
						case SDLK_1:
							_pressedValue=1;
							break;
						case SDLK_KP_2:
						case SDLK_2:
							_pressedValue=2;
							break;
						case SDLK_KP_3:
						case SDLK_3:
							_pressedValue=3;
							break;
						case SDLK_KP_4:
						case SDLK_4:
							_pressedValue=4;
							break;
						case SDLK_KP_5:
						case SDLK_5:
							_pressedValue=5;
							break;
						case SDLK_KP_6:
						case SDLK_6:
							_pressedValue=6;
							break;
						case SDLK_KP_7:
						case SDLK_7:
							_pressedValue=7;
							break;
						case SDLK_KP_8:
						case SDLK_8:
							_pressedValue=8;
							break;
						case SDLK_KP_9:
						case SDLK_9:
							_pressedValue=9;
							break;
					}
					// If they actually pressed a number
					if (_pressedValue!=-1){
						_addNumberInput(&_userInput,_userInputAsString,_pressedValue);
					}
				}
			}
		#endif
		if (wasJustPressed(SCE_TOUCH)){
			int _fixedTouchX = touchX-globalDrawXOffset;
			int _fixedTouchY = touchY-globalDrawYOffset;
			if (_fixedTouchY>logicalScreenHeight-singleBlockSize*2){
				if (_fixedTouchY<logicalScreenHeight-singleBlockSize){
					_addNumberInput(&_userInput,_userInputAsString,_fixedTouchX/singleBlockSize);
				}else{
					if (_fixedTouchX>_backspaceButtonX && _fixedTouchX<_backspaceButtonX+_backspaceWidth){
						_delNumberInput(&_userInput,_userInputAsString);
					}else if (_fixedTouchX>_exitButtonX && _fixedTouchX<_exitButtonX+_exitWidth){
						_shouldExit=1;	
						_userInput=_defaultNumber;
					}else if (_fixedTouchX>_doneButtonX && _fixedTouchX<_doneButtonX+_doneWidth){
						_shouldExit=1;
					}
				}
			}
		}
		controlsEnd();
		
		drawString(_prompt,0,0);
		drawString(_userInputAsString,0,singleBlockSize);
		char _currentDrawNumber=0;
		char j;
		for (j=0;j<=9;++j){
			char _tempString[2];
			_tempString[0]=_currentDrawNumber+48;
			_tempString[1]='\0';
			int _drawX = j*singleBlockSize;
			int _drawY = logicalScreenHeight-2*singleBlockSize;
			
			if (j%2==0){
				drawRectangle(_drawX,_drawY,singleBlockSize,singleBlockSize,204,255,204,255);
			}else{
				drawRectangle(_drawX,_drawY,singleBlockSize,singleBlockSize,255,204,255,255);
			}
			drawString(_tempString,_drawX+CONSTCHARW/2,_drawY+CONSTCHARW/2);
			_currentDrawNumber++;
		}
		drawRectangle(_backspaceButtonX,logicalScreenHeight-singleBlockSize,_backspaceWidth,singleBlockSize,204,204,255,255);
		drawRectangle(_exitButtonX,logicalScreenHeight-singleBlockSize,_exitWidth,singleBlockSize,255,204,204,255);
		drawRectangle(_doneButtonX,logicalScreenHeight-singleBlockSize,_doneWidth,singleBlockSize,204,204,255,255);

		drawString(_backspaceString,_backspaceButtonX,logicalScreenHeight-singleBlockSize+CONSTCHARW/2);
		drawString(_exitString,_exitButtonX,logicalScreenHeight-singleBlockSize+CONSTCHARW/2);
		drawString(_doneString,_doneButtonX,logicalScreenHeight-singleBlockSize+CONSTCHARW/2);

		endDrawing();
	}

	controlLoop();
	return _userInput;
}

// Inclusive bounds
void resetRepeatNotes(int _startResetX, int _endResetX){
	int i;
	for (i=_startResetX;i<=_endResetX;++i){
		int j;
		for (j=0;j<songHeight;++j){
			if (songArray[j][i].id==repeatEndID){
				songArray[j][i].extraData=NULL;
			}
		}
	}
}

// Does not change song data, just temporary note states and stuff
void resetPlayState(){
	resetRepeatNotes(0,songWidth-1);
}

u16 bitmpTextWidth(char* _passedString){
	return strlen(_passedString)*(CONSTCHARW);
}

u32 bpmFormula(u32 re){
	return 60000 / (4 * re);
}
// Given note wait time, find BPM
u32 reverseBPMformula(u32 re){
	return 15000/re;
}

void centerAround(u32 _passedPosition){
	if (optionDoCenterPlay){
		u16 _halfScreenWidth = ceil(pageWidth/2);
		if (_passedPosition<_halfScreenWidth){
			setSongXOffset(0);
			return;
		}
		if (_passedPosition>=songWidth-_halfScreenWidth){
			setSongXOffset(songWidth-pageWidth);
			return;
		}
		setSongXOffset(_passedPosition-_halfScreenWidth);
	}else{
		setSongXOffset((_passedPosition/pageWidth)*pageWidth);
	}
}

uiElement* getUIByID(s16 _passedId){
	int i;
	for (i=0;i<totalUI;++i){
		if (myUIBar[i].uniqueId==_passedId){
			return &(myUIBar[i]);
		}
	}
	//printf("Couldn't find the UI with id %d. You didn't delete it, right?",_passedId);
	return NULL;
}
void updateNoteIcon(){
	uiElement* _noteIconElement = getUIByID(U_SELICON);
	if (_noteIconElement!=NULL){
		_noteIconElement->image=noteImages[getUINoteID()];
	}
}

void pageTransition(int _destX){
	if (optionDoFancyPage){
		int _positiveDest=songWidth;
		int _negativeDest=-1;
		int _changeAmount = (_destX-songXOffset)/30;
		if (_changeAmount==0){
			if (_destX-songXOffset<0){
				_changeAmount=-1;
			}else{
				_changeAmount=1;
			}
		}
		if (_destX<songXOffset){
			_negativeDest = _destX;
		}else if (_destX>songXOffset){
			_positiveDest = _destX;
		}else{
			//printf("Bad, are equal, nothing to do.\n");
			return;
		}
		while(1){
			setSongXOffset(songXOffset+_changeAmount);
			if (songXOffset>=_positiveDest){
				setSongXOffset(_positiveDest);
				break;
			}
			if (songXOffset<=_negativeDest){
				setSongXOffset(_negativeDest);
				break;
			}
			startDrawing();
			doUsualDrawing();
			endDrawing();
		}
	}else{
		setSongXOffset(_destX);
	}
}

void togglePlayUI(){
	uiElement* _playButtonUI = getUIByID(U_PLAY);
	if (_playButtonUI->image==stopButtonImage){
		_playButtonUI->image = playButtonImage;
	}else{
		_playButtonUI->image = stopButtonImage;
	}
	_playButtonUI = getUIByID(U_YPLAY);
	if (_playButtonUI->image==stopButtonImage){
		_playButtonUI->image = yellowPlayButtonImage;
	}else{
		_playButtonUI->image = stopButtonImage;
	}
}

void playAtPosition(s32 _startPosition){
	resetPlayState();
	if (!currentlyPlaying){
		pageTransition(_startPosition);
		setSongXOffset(_startPosition);
		togglePlayUI();
		currentlyPlaying=1;
		//currentPlayPosition = songXOffset;
		lastPlayAdvance=0;
		nextPlayPosition=songXOffset;
	}else{
		togglePlayUI();
		currentlyPlaying=0;
		setSongXOffset((currentPlayPosition/pageWidth)*pageWidth);
	}
}

// Here we hard code the number of bytes to write because we want the same number of bytes on all systems so that save files are cross platform
void saveSong(char* _passedFilename){
	FILE* fp = fopen(_passedFilename,"w");
	if (fp!=NULL){
		// Magic
		fwrite(SAVEFORMATMAGIC,strlen(SAVEFORMATMAGIC),1,fp);
		u8 _tempHoldVersion = SONGFORMATVERSION;
			fwrite(&_tempHoldVersion,1,1,fp);
		// Special IDs needed for loading
		fwrite(&audioGearID,1,1,fp);
		// Song info
		s16 _tempHoldBPM = bpm;
		s16 _tempHoldWidth = songWidth;
		s16 _tempHoldHeight = songHeight;
		_tempHoldBPM = fixShort(_tempHoldBPM);
		_tempHoldWidth = fixShort(_tempHoldWidth);
		_tempHoldHeight = fixShort(_tempHoldHeight);
		fwrite(&_tempHoldBPM,2,1,fp);
		fwrite(&_tempHoldWidth,2,1,fp);
		fwrite(&_tempHoldHeight,2,1,fp);
		
		int _y;
		for (_y=0;_y<songHeight;++_y){
			int _x;
			for (_x=0;_x<songWidth;++_x){
				fwrite(&(songArray[_y][_x].id),1,1,fp);
				if (songArray[_y][_x].id==audioGearID){
					fwrite(songArray[_y][_x].extraData,1,AUDIOGEARSPACE*2,fp);
					u8* _tempHoldVolume = getGearVolume(songArray[_y][_x].extraData);
					fwrite(_tempHoldVolume,1,1,fp);
				}
			}
		}
		fwrite(SAVEENDMARKER,strlen(SAVEENDMARKER),1,fp);
		fclose(fp);
	}else{
		easyMessage("Failed to open file.");
		easyMessage(_passedFilename);
	}
	return;
}

#include "songLoaders.h"

void uiSave(){
	char* _chosenFile = selectSaveFile();
	if (_chosenFile!=NULL){
		saveSong(_chosenFile);
	}
	free(_chosenFile);
}

void uiLoad(){
	char* _chosenFile = selectLoadFile();
	if (_chosenFile!=NULL){
		loadSong(_chosenFile);
	}
	free(_chosenFile);
}

void uiKeyConf(){
	int _columnWidth=-1;
	int _iconHeight = singleBlockSize+CONSTCHARW;
	int _elementsPerColumn = (visiblePageHeight*singleBlockSize)/(_iconHeight);
	int _totalNoteColumns = ceil(totalNotes/(double)_elementsPerColumn);
	int _totalUIColumns = ceil(totalUI/(double)_elementsPerColumn);

	char _readyForInput=0;
	char _isUISelected;
	int _selectedIndex;
	while(1){
		controlsStart();
		noteUIControls();
		if (!_readyForInput){
			if (lastSDLPressedKey==SDLK_ESCAPE){
				break;
			}
			if (wasJustPressed(SCE_TOUCH)){
				int _fixedTouchX = touchX-globalDrawXOffset;
				int _fixedTouchY = touchY-globalDrawYOffset;
				int _selectedColumn = _fixedTouchX/_columnWidth;
				int _iconYSelect = _fixedTouchY/_iconHeight;
				if (_selectedColumn<_totalNoteColumns){
					_isUISelected=0;
					_selectedIndex = _iconYSelect+_selectedColumn*_elementsPerColumn;
					if (_selectedIndex<totalNotes){
						_readyForInput=1;
					}
				}else{
					_isUISelected=1;
					_selectedIndex = _iconYSelect+(_selectedColumn-_totalNoteColumns)*_elementsPerColumn;
					if (_selectedIndex<totalUI){
						_readyForInput=1;
					}
				}

				if (lastClickWasRight){
					if (_isUISelected){
						uiHotkeys[_selectedIndex]=SDLK_UNKNOWN;
					}else{
						noteHotkeys[_selectedIndex]=SDLK_UNKNOWN;
					}
					_readyForInput=0;
					_columnWidth=-1;
				}
			}
		}else{
			if (wasJustPressed(SCE_TOUCH)){
				// Do a frame perfect keypress and click at the same time to reset all hotkeys
				if (lastSDLPressedKey!=SDLK_UNKNOWN){
					printf("Reset all hotkeys.\n");
					int i;
					for (i=0;i<totalNotes;++i){
						noteHotkeys[i]=SDLK_UNKNOWN;
					}
					for (i=0;i<totalUI;++i){
						uiHotkeys[i] = SDLK_UNKNOWN;
					}
				}else{
					_readyForInput=0;
				}
			}else{
				if (lastSDLPressedKey!=SDLK_UNKNOWN){
					// Special cases first
					if (lastSDLPressedKey==SDLK_ESCAPE){
						_readyForInput=0;
					}else{
						// Overwrite duplicate hotkeys
						int i;
						for (i=0;i<totalNotes;++i){
							if (noteHotkeys[i]==lastSDLPressedKey){
								noteHotkeys[i] = SDLK_UNKNOWN;
							}
						}
						for (i=0;i<totalUI;++i){
							if (uiHotkeys[i]==lastSDLPressedKey){
								uiHotkeys[i] = SDLK_UNKNOWN;
							}
						}
						//
						if (_isUISelected){
							uiHotkeys[_selectedIndex]=lastSDLPressedKey;
						}else{
							noteHotkeys[_selectedIndex]=lastSDLPressedKey;
						}
						_readyForInput=0;
						_columnWidth=-1;
					}
				}
			}
		}
		controlsEnd();

		if (_columnWidth==-1){
			int _foundMaxStrlen=1;
			int i;
			for (i=0;i<totalNotes;++i){
				if (noteHotkeys[i]!=SDLK_UNKNOWN){
					const char* _lastStr = SDL_GetKeyName(noteHotkeys[i]);
					if (strlen(_lastStr)>_foundMaxStrlen){
						_foundMaxStrlen=strlen(_lastStr);
					}
				}
			}
			for (i=0;i<totalUI;++i){
				if (uiHotkeys[i]!=SDLK_UNKNOWN){
					const char* _lastStr = SDL_GetKeyName(uiHotkeys[i]);
					if (strlen(_lastStr)>_foundMaxStrlen){
						_foundMaxStrlen=strlen(_lastStr);
					}
				}
			}
			_columnWidth = singleBlockSize+CONSTCHARW*_foundMaxStrlen;
		}

		startDrawing();
		if (_readyForInput==0){
			int i;
			// Draw notes
			for (i=0;i<_totalNoteColumns;++i){
				int j;
				int _totalDraw = totalNotes-i*_elementsPerColumn<_elementsPerColumn ? totalNotes-i*_elementsPerColumn : _elementsPerColumn;
				for (j=0;j<_totalDraw;++j){
					int _drawX = i*(_columnWidth);
					int _drawY = j*(_iconHeight);
					drawImageScaleAlt(noteImages[i*_elementsPerColumn+j],_drawX,_drawY,generalScale,generalScale);
					if (noteHotkeys[i*_elementsPerColumn+j]!=SDLK_UNKNOWN){
						drawString(SDL_GetKeyName(noteHotkeys[i*_elementsPerColumn+j]),_drawX+singleBlockSize,_drawY+CONSTCHARW/2);
					}
				}
			}
			// Draw UI
			for (i=0;i<_totalUIColumns;++i){
				int j;
				int _totalDraw = totalUI-i*_elementsPerColumn<_elementsPerColumn ? totalUI-i*_elementsPerColumn : _elementsPerColumn;
				for (j=0;j<_totalDraw;++j){
					int _drawX = (i+_totalNoteColumns)*(_columnWidth);
					int _drawY = j*(_iconHeight);
					drawImageScaleAlt(myUIBar[i*_elementsPerColumn+j].image,_drawX,_drawY,generalScale,generalScale);
					if (uiHotkeys[i*_elementsPerColumn+j]!=SDLK_UNKNOWN){
						drawString(SDL_GetKeyName(uiHotkeys[i*_elementsPerColumn+j]),_drawX+singleBlockSize,_drawY+CONSTCHARW/2);
					}
				}
			}

			drawString("Click an icon to set a hotkey for it.",CONSTCHARW,visiblePageHeight*singleBlockSize);
			drawString("Press esc to go back.",CONSTCHARW,visiblePageHeight*singleBlockSize+CONSTCHARW);
		}else{
			drawImageScaleAlt(_isUISelected ? myUIBar[_selectedIndex].image : noteImages[_selectedIndex],CONSTCHARW,CONSTCHARW,generalScale*2,generalScale*2);
			drawString("Press a key to bind it to this.",CONSTCHARW,generalScale*2*singleBlockSize+CONSTCHARW*2);
			drawString("Click anywhere to cancel.",CONSTCHARW,generalScale*2*singleBlockSize+CONSTCHARW*3);
		}

		endDrawing();
	}
	saveHotkeys();
}

void uiUIScroll(){
	if (uiScrollOffset==0){
		uiScrollOffset=uiUIScrollIndex;
	}else{
		uiScrollOffset=0;
	}
}

void uiCredits(){
	while(1){
		controlsStart();
		if (wasJustPressed(SCE_TOUCH)){
			break;
		}
		controlsEnd();
		startDrawing();
		drawString("MyLegGuy - Programming",0,0);
		drawString("HonestyCow - Sound matching",0,CONSTCHARW);
		drawString("D.RS - Theme",0,CONSTCHARW*2);
		drawString("Bonk - BPM Forumla",0,CONSTCHARW*3);
		endDrawing();
	}
}

void uiSettings(){
	controlLoop(); // Because we're coming from the middle of a control loop.
	u8 _totalSettings=4;
	char* _settingsText[_totalSettings];
	u8* _settingsValues[_totalSettings];

	_settingsText[0]="Play on place";
		_settingsValues[0]=&optionPlayOnPlace;
	_settingsText[1]="Zero based position";
		_settingsValues[1]=&optionZeroBasedPosition;
	_settingsText[2]="Fancy page transition";
		_settingsValues[2]=&optionDoFancyPage;
	_settingsText[3]="Centered play bar";
		_settingsValues[3]=&optionDoCenterPlay;

	while (1){
		controlsStart();
		if (wasJustPressed(SCE_TOUCH)){
			int _fixedTouchY = touchY-globalDrawYOffset;
			// If we tapped the exit string
			if (_fixedTouchY>logicalScreenHeight-singleBlockSize){
				break;
			}
			int _touchedEntry = _fixedTouchY/singleBlockSize;
			if (_touchedEntry<_totalSettings){
				*_settingsValues[_touchedEntry] = !*_settingsValues[_touchedEntry];
			}
		}
		controlsEnd();
		startDrawing();
		u8 i;
		for (i=0;i<_totalSettings;++i){
			if (*_settingsValues[i]==1){
				drawRectangle(0,i*singleBlockSize,singleBlockSize,singleBlockSize,0,255,0,255);
			}else{
				drawRectangle(0,i*singleBlockSize,singleBlockSize,singleBlockSize,255,0,0,255);
			}
			drawString(_settingsText[i],singleBlockSize,i+i*singleBlockSize+CONSTCHARW/2);
		}
		drawString(CLICKTOGOBACK,0,logicalScreenHeight-CONSTCHARW);
		endDrawing();
	}
	setSongXOffset(songXOffset);
	controlLoop();
	//
	saveSettings();
}

void uiResizeSong(){
	int _newSongWidth=0;
	do{
		_newSongWidth = getNumberInput("Enter song width",songWidth);
	}while(_newSongWidth<pageWidth);
	if (songWidth!=_newSongWidth){
		setSongWidth(songArray,songWidth,_newSongWidth);
		songWidth=_newSongWidth;
		setSongXOffset(songXOffset);
	}
}

void uiBPM(){
	do{
		bpm=getNumberInput("Input beats per minute.",bpm);
	}while(bpm==0);
	if (bpm<20 || bpm>200){
		// LazyMessage("Too wierd for real Growtopia")
	}
}

void uiYellowPlay(){
	playAtPosition(songXOffset);
}

void uiCount(){
	controlsEnd();
	if (totalNotes==1){
		printf("Phew, prevented division by zero there.");
		return;
	}
	u16 _amountFound[totalNotes-1];
	int i, j, k;
	// Zero array
	for (i=1;i<totalNotes;++i){
		_amountFound[i-1]=0;
	}
	// Fill number array
	for (i=0;i<songHeight;++i){
		for (j=0;j<songWidth;++j){
			if (songArray[i][j].id!=0){
				++_amountFound[songArray[i][j].id-1];
			}
		}
	}
	// Convert number array to string
	char _amountsAsString[totalNotes-1][6]; // up to 99999 for each note
	u16 _maxFontWidth=0;
	for (k=1;k<totalNotes;++k){
		sprintf(_amountsAsString[k-1],"%d",_amountFound[k-1]);
		if (bitmpTextWidth(_amountsAsString[k-1])>_maxFontWidth){
			_maxFontWidth = bitmpTextWidth(_amountsAsString[k-1]);
		}
	}
	_maxFontWidth+=CONSTCHARW;

	// Space between the notes on y axis
	u16 _noteVSpace;
	if (isMobile){ // On mobile, keep everything as compressed as possible
		_noteVSpace = singleBlockSize;
	}else{
		_noteVSpace = singleBlockSize + CONSTCHARW;
	}
	u16 _columnTotalNotes = ((visiblePageHeight*singleBlockSize)/_noteVSpace);
	u16 _totalColumns = ceil((totalNotes-1)/(double)_columnTotalNotes);
	while (1){

		controlsStart();
		if (wasJustPressed(SCE_TOUCH)){
			if (floor((touchY-globalDrawYOffset)/singleBlockSize)==visiblePageHeight){
				// Because we break info control loop. This will make it so SCE_TOUCH is no longer wasJustPressed, but it will still be isDown, so we have to make sure the user tapped not on the song, but on the buttons, which require wasJustPressed
				controlLoop();
				break;
			}
		}
		controlsEnd();

		startDrawing();
		u16 _nextNoteDrawId=1;
		for (i=0;i<_totalColumns;++i){
			s16 _maxPerColumn=totalNotes-1-i*_columnTotalNotes;
			if (_maxPerColumn>_columnTotalNotes){
				_maxPerColumn = _columnTotalNotes;
			}
			for (j=0;j<_maxPerColumn;++j){
				drawImageScaleAlt(noteImages[_nextNoteDrawId],i*_maxFontWidth+singleBlockSize*i,j*_noteVSpace,generalScale,generalScale);
				drawString(_amountsAsString[_nextNoteDrawId-1],i*_maxFontWidth+singleBlockSize*i+singleBlockSize+CONSTCHARW/2,j*_noteVSpace+CONSTCHARW/2);
				++_nextNoteDrawId;
			}
		}
		drawString(CLICKTOGOBACK,0,logicalScreenHeight-CONSTCHARW);
		endDrawing();
	}
}

void uiPlay(){
	playAtPosition(0);
}

void uiUp(){
	if (songYOffset!=0){
		songYOffset--;
	}
}
void uiDown(){
	songYOffset++;
	if (songYOffset+visiblePageHeight>pageHeight){
		songYOffset = pageHeight-visiblePageHeight;
	}
}

void uiRight(){
	if (songXOffset+pageWidth>songWidth-pageWidth){ // If we would go too far
		if (songXOffset!=songWidth-pageWidth){
			pageTransition(songWidth-pageWidth);
		}else{
			pageTransition(0); // Wrap
		}
	}else{
		pageTransition(songXOffset+pageWidth);
	}
}
void uiLeft(){
	// Fix it if we're not on a mulitple of a page.
	if (songXOffset%pageWidth!=0){
		pageTransition((songXOffset/pageWidth)*pageWidth);
	}else{
		if (songXOffset<pageWidth){
			pageTransition(songWidth-pageWidth); // wrap
		}else{
			pageTransition(songXOffset-pageWidth);
		}
	}
}

void uiNoteIcon(){
	uiNoteIndex++;
	if (uiNoteIndex==totalNotes){
		uiNoteIndex=0;
	}
	updateNoteIcon();
}

int fixX(int _x){
	return _x+globalDrawXOffset;
}

int fixY(int _y){
	return _y+globalDrawYOffset;
}

uiElement* addUI(){
	++totalUI;
	myUIBar = realloc(myUIBar,sizeof(uiElement)*totalUI);
	uiHotkeys = recalloc(uiHotkeys,sizeof(SDL_Keycode)*(totalUI-1),sizeof(SDL_Keycode)*totalUI);
	myUIBar[totalUI-1].uniqueId=U_NOTUNIQUE;
	return &(myUIBar[totalUI-1]);
}

// realloc, but new memory is zeroed out
void* recalloc(void* _oldBuffer, int _oldSize, int _newSize){
	void* _newBuffer = realloc(_oldBuffer,_newSize);
	if (_newSize > _oldSize){
		void* _startOfNewData = ((char*)_newBuffer)+_oldSize;
		memset(_startOfNewData,0,_newSize-_oldSize);
	}
	return _newBuffer;
}

// Can't change song height
// I thought about it. This function is so small, it wouldn't be worth it to just make a helper function for all arrays I want to resize.
// TODO - Memory leak can happen if user puts audio gears at the end of the song and then shrinks the song
void setSongWidth(noteSpot** _passedArray, u16 _passedOldWidth, u16 _passedWidth){
	if (_passedOldWidth==_passedWidth){
		return;
	}
	int i;
	for (i=0;i<songHeight;++i){
		_passedArray[i] = recalloc(_passedArray[i],_passedOldWidth*sizeof(noteSpot),_passedWidth*sizeof(noteSpot));
	}
}

void drawImageScaleAlt(CrossTexture* _passedTexture, int _x, int _y, double _passedXScale, double _passedYScale){
	if (_passedTexture!=NULL){
		drawTextureScale(_passedTexture,_x,_y,_passedXScale,_passedYScale);
	}
}

CrossTexture* loadEmbeddedPNG(const char* _passedFilename){
	char* _fixedPathBuffer = malloc(strlen(_passedFilename)+strlen(getFixPathString(TYPE_EMBEDDED))+1);
	fixPath((char*)_passedFilename,_fixedPathBuffer,TYPE_EMBEDDED);
	CrossTexture* _loadedTexture = loadPNG(_fixedPathBuffer);
	free(_fixedPathBuffer);
	return _loadedTexture;
}

// Return a path that may or may not be fixed to TYPE_EMBEDDED
char* possiblyFixPath(const char* _passedFilename, char _shouldFix){
	if (_shouldFix){
		char* _fixedPathBuffer = malloc(strlen(_passedFilename)+strlen(getFixPathString(TYPE_EMBEDDED))+1);
		fixPath((char*)_passedFilename,_fixedPathBuffer,TYPE_EMBEDDED);
		return _fixedPathBuffer;
	}else{
		return strdup(_passedFilename);
	}
}

void drawString(const char* _passedString, int _x, int _y){
	// See fonthelper.h
	_drawString(_passedString,_x,_y,generalScale,CONSTCHARW);
}

void XOutFunction(){
	exit(0);
}

void setTotalNotes(u16 _newTotal){
	if (_newTotal<=totalNotes){
		return;
	}

	noteImages = realloc(noteImages,sizeof(CrossTexture*)*_newTotal);
	noteSounds = realloc(noteSounds,sizeof(CROSSSFX**)*_newTotal);
	extraNoteInfo = recalloc(extraNoteInfo,sizeof(noteInfo)*totalNotes,sizeof(noteInfo)*_newTotal);
	noteUIOrder = recalloc(noteUIOrder,sizeof(u8)*totalNotes,sizeof(u8)*_newTotal);
	noteHotkeys = recalloc(noteHotkeys,sizeof(SDL_Keycode)*totalNotes,sizeof(SDL_Keycode)*_newTotal);

	int i;
	for (i=totalNotes;i<_newTotal;++i){
		noteSounds[i] = calloc(1,songHeight*sizeof(CROSSSFX*));
	}

	totalNotes = _newTotal;
}

// string
int L_loadSound(lua_State* passedState){
	char* _fixedPath = possiblyFixPath(lua_tostring(passedState,1), (lua_gettop(passedState)==2 && lua_toboolean(passedState,2)==0) ? 0 : 1);
	lua_pushlightuserdata(passedState,loadSound(_fixedPath));
	free(_fixedPath);
	return 1;
}
int L_loadImage(lua_State* passedState){
	char* _fixedPath = possiblyFixPath(lua_tostring(passedState,1), (lua_gettop(passedState)==2 && lua_toboolean(passedState,2)==0) ? 0 : 1);
	lua_pushlightuserdata(passedState,loadPNG(_fixedPath));
	free(_fixedPath);
	return 1;
}
int L_getMobile(lua_State* passedState){
	lua_pushboolean(passedState,isMobile);
	return 1;
}
// table of 14 loaded images
int L_setBgParts(lua_State* passedState){
	backgroundMode=BGMODE_PART;
	bgPartsEmpty = malloc(sizeof(CrossTexture*)*songHeight);
	bgPartsLabel = malloc(sizeof(CrossTexture*)*songHeight);
	int i;
	for (i=0;i<songHeight;++i){
		// Get empty image
		lua_rawgeti(passedState,1,i+1);
		bgPartsEmpty[i] = lua_touserdata(passedState,-1);
		lua_pop(passedState,1);
		// Get label image
		lua_rawgeti(passedState,2,i+1);
		bgPartsLabel[i] = lua_touserdata(passedState,-1);
		lua_pop(passedState,1);
	}
	return 0;
}
int L_setBigBg(lua_State* passedState){
	backgroundMode=BGMODE_SINGLE;
	char* _fixedPath = possiblyFixPath(lua_tostring(passedState,1), (lua_gettop(passedState)==2 && lua_toboolean(passedState,2)==0) ? 0 : 1);
	bigBackground = loadPNG(_fixedPath);
	free(_fixedPath);
	return 0;
}
// <int slot> <loaded image> <table of loaded sounds 14 elements long>
// Third argument is optional to disable sound effect
int L_addNote(lua_State* passedState){
	int _passedSlot = lua_tonumber(passedState,1);
	// Don't need to check if we're actually adding
	setTotalNotes(_passedSlot+1);

	noteUIOrder[_passedSlot]=_passedSlot;
	noteImages[_passedSlot] = lua_touserdata(passedState,2);
	
	if (lua_gettop(passedState)==3){
		// Load array data
		int i;
		for (i=0;i<songHeight;++i){
			lua_rawgeti(passedState,3,i+1);
			noteSounds[_passedSlot][i] = lua_touserdata(passedState,-1);
			lua_pop(passedState,1);
		}
	}else{
		int i;
		for (i=0;i<songHeight;++i){
			noteSounds[_passedSlot][i] = NULL;
		}
	}
	return 0;
}
int L_setNoteGearData(lua_State* passedState){
	int i = lua_tonumber(passedState,1);
	extraNoteInfo[i].letter = lua_tostring(passedState,2)[0];
	extraNoteInfo[i].accidental = lua_tostring(passedState,3)[0];
	return 0;
}
int L_swapUI(lua_State* passedState){
	int _slot1 = lua_tonumber(passedState,1);
	int _slot2 = lua_tonumber(passedState,2);
	uiElement _tempSwapHold = myUIBar[_slot1];
	myUIBar[_slot1] = myUIBar[_slot2];
	myUIBar[_slot2]=_tempSwapHold;
	return 0;
}
int L_deleteUI(lua_State* passedState){
	int _slotIHate = lua_tonumber(passedState,1);
	myUIBar[_slotIHate].image=NULL;
	myUIBar[_slotIHate].activateFunc=NULL;
	myUIBar[_slotIHate].uniqueId=-1;
	if (_slotIHate==totalUI-1){
		totalUI--;
	}
	return 0;
}
// Add a UI slot and return its number
int L_addUI(lua_State* passedState){
	addUI();
	lua_pushnumber(passedState,totalUI-1);
	return 1;
}
int L_getTotalUI(lua_State* passedState){
	lua_pushnumber(passedState,totalUI);
	return 1;
}
int L_setSpecialID(lua_State* passedState){
	const char* _passedIdentifier = lua_tostring(passedState,1);
	if (strcmp(_passedIdentifier,"repeatStart")==0){
		repeatStartID = lua_tonumber(passedState,2);
	}else if (strcmp(_passedIdentifier,"repeatEnd")==0){
		repeatEndID = lua_tonumber(passedState,2);
	}else if (strcmp(_passedIdentifier,"audioGear")==0){
		audioGearID = lua_tonumber(passedState,2);
	}else{
		printf("Invalid identifier %s\n",_passedIdentifier);
	}
	return 0;
}
int L_swapNoteUIOrder(lua_State* passedState){
	int _sourceIndex = lua_tonumber(passedState,1);
	int _destIndex = lua_tonumber(passedState,2);
	u8 _oldDestValue = noteUIOrder[_destIndex];
	noteUIOrder[_destIndex]=noteUIOrder[_sourceIndex];
	noteUIOrder[_sourceIndex]=_oldDestValue;
	return 0;
}
int L_getSongWidth(lua_State* passedState){
	lua_pushnumber(passedState,songWidth);
	return 1;
}
int L_getSongHeight(lua_State* passedState){
	lua_pushnumber(passedState,songHeight);
	return 1;
}
int L_setSongWidth(lua_State* passedState){
	setSongWidth(songArray,songWidth,lua_tonumber(passedState,1));
	songWidth = lua_tonumber(passedState,1);
	return 0;
}

int L_getNoteSpot(lua_State* passedState){
	int _x = lua_tonumber(passedState,1);
	int _y = lua_tonumber(passedState,2);
	lua_pushnumber(passedState,songArray[_y][_x].id);
	return 1;
}
int L_setNoteSpot(lua_State* passedState){
	_placeNoteLow(lua_tonumber(passedState,1),lua_tonumber(passedState,2),lua_tonumber(passedState,3),0,songArray);
	return 0;
}
int L_getAudioGearSize(lua_State* passedState){
	lua_pushnumber(passedState,AUDIOGEARSPACE);
	return 1;
}
// x y
int L_getAudioGear(lua_State* passedState){
	int _x = lua_tonumber(passedState,1);
	int _y = lua_tonumber(passedState,2);
	if (songArray[_y][_x].id!=audioGearID){
		return 0;
	}
	u8* _gearData = songArray[_y][_x].extraData;
	// New table is at the top of the stack
	lua_newtable(passedState);
	int i;
	for (i=0;i<AUDIOGEARSPACE;++i){
		// Add the note ID to the table first
		// First push index of the place we want to set in the table
		lua_pushnumber(passedState,(i*2)+1);
		// Push our value for that index
		lua_pushnumber(passedState,_gearData[i*2]);
		// This pops the top two from the stack, the table is now back at the top
		lua_settable(passedState,-3);

		// Same as before, but we push add the Y position this time. It is possible to push uninitialized values here, but only if the pushed note ID is 0, so the programmer should ignore it anyway
		lua_pushnumber(passedState,(i*2)+2);
		lua_pushnumber(passedState,_gearData[i*2+1]);
		lua_settable(passedState,-3);
	}
	// Table is still at the top of the stack
	// Push the volume
	lua_pushnumber(passedState,*getGearVolume(_gearData));
	return 2;
}
// x y table volume
int L_setAudioGear(lua_State* passedState){
	int _x = lua_tonumber(passedState,1);
	int _y = lua_tonumber(passedState,2);
	lua_len(passedState,3);
		int _tableLength = lua_tonumber(passedState,-1);
		lua_pop(passedState,1);
	if (songArray[_y][_x].id!=audioGearID){
		_placeNoteLow(_x,_y,audioGearID,0,songArray);
	}
	u8* _gearData = songArray[_y][_x].extraData;
	_tableLength/=2; // Half of the entries
	int i;
	for (i=0;i<_tableLength;++i){
		lua_rawgeti(passedState,3,(i*2)+1); // Get ID from table
		_gearData[i*2]=lua_tonumber(passedState,-1);
		lua_pop(passedState,1);

		lua_rawgeti(passedState,3,(i*2)+2); // Get Y position from table
		_gearData[i*2+1]=lua_tonumber(passedState,-1);
		lua_pop(passedState,1);
	}
	*getGearVolume(_gearData) = lua_tonumber(passedState,4);
	return 0;
}
// one argument, the allowed file types
int L_selectFile(lua_State* passedState){
	char* _gottenString = sharedFilePicker(0,lua_tostring(passedState,1),0);
	if (_gottenString!=NULL){
		lua_pushstring(passedState,_gottenString);
		free(_gottenString);
		return 1;
	}else{
		return 0;
	}
}
int L_saveFile(lua_State* passedState){
	char* _gottenString = sharedFilePicker(1,lua_tostring(passedState,1),0);
	if (_gottenString!=NULL){
		lua_pushstring(passedState,_gottenString);
		free(_gottenString);
		return 1;
	}else{
		return 0;
	}
}
int L_findMaxX(lua_State* passedState){
	findMaxX();
	return 0;
}
int L_setMaxX(lua_State* passedState){
	maxX = lua_tonumber(passedState,1);
	return 0;
}
// prompt default
int L_getNumberInput(lua_State* passedState){
	long _returnedValue = getNumberInput(lua_tostring(passedState,1),lua_tonumber(passedState,2));
	lua_pushnumber(passedState,_returnedValue);
	return 1;
}
int L_getBPM(lua_State* passedState){
	lua_pushnumber(passedState,bpm);
	return 1;
}
int L_setBPM(lua_State* passedState){
	bpm = lua_tonumber(passedState,1);
	return 0;
}
void pushLuaFunctions(){
	LUAREGISTER(L_addNote,"addNote");
	LUAREGISTER(L_setBigBg,"setBigBg");
	LUAREGISTER(L_setBgParts,"setBgParts");
	LUAREGISTER(L_getMobile,"isMobile");
	LUAREGISTER(L_loadSound,"loadSound");
	LUAREGISTER(L_loadImage,"loadImage");
	LUAREGISTER(L_swapUI,"swapUI");
	LUAREGISTER(L_deleteUI,"deleteUI");
	LUAREGISTER(L_getTotalUI,"getTotalUI");
	LUAREGISTER(L_addUI,"addUI");
	LUAREGISTER(L_setSpecialID,"setSpecialID");
	LUAREGISTER(L_setNoteGearData,"setGearInfo");
	LUAREGISTER(L_swapNoteUIOrder,"swapNoteUIOrder");
	//
	LUAREGISTER(L_getSongWidth,"getSongWidth");
	LUAREGISTER(L_getSongHeight,"getSongHeight");
	LUAREGISTER(L_setSongWidth,"setSongWidth");
	LUAREGISTER(L_getNoteSpot,"getNoteSpot");
	LUAREGISTER(L_setNoteSpot,"setNoteSpot");
	LUAREGISTER(L_getAudioGearSize,"getAudioGearSize");
	LUAREGISTER(L_getAudioGear,"getAudioGear");
	LUAREGISTER(L_setAudioGear,"setAudioGear");
	LUAREGISTER(L_selectFile,"selectFile");
	LUAREGISTER(L_saveFile,"saveFile");
	LUAREGISTER(L_findMaxX,"findMaxX");
	LUAREGISTER(L_setMaxX,"setMaxX");
	LUAREGISTER(L_getNumberInput,"getNumberInput");
	LUAREGISTER(L_getBPM,"getBPM");
	LUAREGISTER(L_setBPM,"setBPM");
}

void die(const char* message){
  printf("die:\n%s\n", message);
  exit(EXIT_FAILURE);
}

void goodLuaDofile(lua_State* passedState, char* _passedFilename){
	if (luaL_dofile(passedState, _passedFilename) != LUA_OK) {
		die(lua_tostring(L,-1));
	}
}

void goodPlaySound(CROSSSFX* _passedSound, int _volume){
	#if !DISABLESOUND
		if (_passedSound!=NULL){
			int _noteChannel = Mix_PlayChannel( -1, _passedSound, 0 );
			Mix_Volume(_noteChannel,(_volume/(double)100)*MIX_MAX_VOLUME);
		}
	#endif
}

void playColumn(s32 _columnNumber){
	if (_columnNumber>maxX){
		_columnNumber=0;
		currentPlayPosition=0;
		nextPlayPosition=1;
		setSongXOffset(0);
	}
	int i;
	char _repeatAlreadyFound=0;
	char _foundRepeatY=0;
	for (i=0;i<pageHeight;++i){
		if (songArray[i][_columnNumber].id!=0){
			if (songArray[i][_columnNumber].id==repeatEndID && !_repeatAlreadyFound && songArray[i][_columnNumber].extraData!=(void*)1){
				_repeatAlreadyFound=1;
				_foundRepeatY=i;
				songArray[i][_columnNumber].extraData=(void*)1;
			}else if (songArray[i][_columnNumber].id==audioGearID){
				u8 j;
				for (j=0;j<AUDIOGEARSPACE;++j){
					if (songArray[i][_columnNumber].extraData[j*2]!=0){
						goodPlaySound(noteSounds[songArray[i][_columnNumber].extraData[j*2]][songArray[i][_columnNumber].extraData[j*2+1]],*getGearVolume(songArray[i][_columnNumber].extraData));
					}
				}
			}else{
				goodPlaySound(noteSounds[songArray[i][_columnNumber].id][i],100);
			}
		}
	}
	// Process the repeat note we found
	if (_repeatAlreadyFound){
		int _searchX;
		for (_searchX=_columnNumber-1;_searchX>=0;--_searchX){
			if (songArray[_foundRepeatY][_searchX].id==repeatStartID){
				resetRepeatNotes(_searchX+1,_columnNumber-1);
				nextPlayPosition = _searchX;
				_repeatAlreadyFound=0;
				break;
			}
		}
		// If we didn't find a repeat start to go with our repeat end
		if (_repeatAlreadyFound){
			// Jump to song start
			nextPlayPosition=0;
		}
	}
}
void _placeNoteLow(int _x, int _y, u8 _noteId, u8 _shouldPlaySound, noteSpot** _passedSong){
	if (_passedSong[_y][_x].id==audioGearID){
		free(_passedSong[_y][_x].extraData);
	}
	if (_noteId==audioGearID){
		_passedSong[_y][_x].extraData = calloc(1,AUDIOGEARSPACE*sizeof(u8)*2+1);
		*getGearVolume(_passedSong[_y][_x].extraData)=100;
	}else{
		_passedSong[_y][_x].extraData=NULL;
	}
	if (_shouldPlaySound){
		if (noteSounds[_noteId][_y]!=NULL){
			goodPlaySound(noteSounds[_noteId][_y],100);
		}
	}
	_passedSong[_y][_x].id=_noteId;
}
void placeNote(int _x, int _y, u16 _noteId){
	
	if (_noteId==audioGearID && songArray[_y][_x].id==audioGearID){
		controlLoop();
		audioGearGUI(songArray[_y][_x].extraData);
		controlLoop();
	}/*else if (songArray[_y][_x].id==_noteId){
		return;
	}*/else{
		if (_x>maxX){
			maxX=_x;
		}
		_placeNoteLow(_x,_y,_noteId,optionPlayOnPlace,songArray);
		if (_x==maxX && _noteId==0){
			findMaxX();
		}
	}
}

void drawPlayBar(int _x){
	drawRectangle((_x-songXOffset)*singleBlockSize,0,singleBlockSize,visiblePageHeight*singleBlockSize,128,128,128,100);
}

void drawSong(noteSpot** _songToDraw, int _drawWidth, int _drawHeight, int _xOffset, int _yOffset){
	int i;
	if (backgroundMode==BGMODE_SINGLE){
		if (_drawWidth!=25){
			drawImageScaleAlt(bigBackground,(-1*getTextureWidth(bigBackground))+singleBlockSize+singleBlockSize*_drawWidth,0,generalScale,generalScale);
		}else{
			drawImageScaleAlt(bigBackground,0,0,generalScale,generalScale);
		}
	}
	for (i=0;i<_drawHeight;++i){
		int j;
		for (j=0;j<_drawWidth;++j){
			if (_songToDraw[i+_yOffset][j+_xOffset].id==0){
				// If we're doing the special part display mode
				if (backgroundMode==BGMODE_PART){
					drawImageScaleAlt(bgPartsEmpty[i+_yOffset],j*singleBlockSize,i*singleBlockSize,generalScale,generalScale);
				}
			}else{
				drawImageScaleAlt(noteImages[_songToDraw[i+_yOffset][j+_xOffset].id],j*singleBlockSize,i*singleBlockSize,generalScale,generalScale);
			}
		}
		if (backgroundMode==BGMODE_PART){
			drawImageScaleAlt(bgPartsLabel[i+_yOffset],_drawWidth*singleBlockSize,i*singleBlockSize,generalScale,generalScale);
		}
	}
}

void noteUIControls(){
	if (wasJustPressed(SCE_MOUSE_SCROLL)){
		if (mouseScroll<0){
			uiNoteIndex--;
			if (uiNoteIndex<0){
				uiNoteIndex=totalNotes-1;
			}
			updateNoteIcon();
		}else if (mouseScroll>0){
			uiNoteIndex++;
			if (uiNoteIndex==totalNotes){
				uiNoteIndex=0;
			}
			updateNoteIcon();
		}// 0 scroll is ignored
	}
}

void drawSingleUI(uiElement* _passedElement, int _drawSlot){
	drawImageScaleAlt(_passedElement->image,_drawSlot*singleBlockSize,visiblePageHeight*singleBlockSize,generalScale,generalScale);
}

void drawUIPointers(uiElement** _passedUIBar, int _totalLength){
	int i;
	for (i=0;i<_totalLength;++i){
		drawSingleUI(_passedUIBar[i],i);
	}
}

void drawUIBar(uiElement* _passedUIBar){
	int i;
	for (i=uiScrollOffset;i<uiPageSize+uiScrollOffset;++i){
		if (i==totalUI){
			break;
		}
		drawSingleUI(&(_passedUIBar[i]),(i-uiScrollOffset));
	}
}

void doUsualDrawing(){
	drawSong(songArray,pageWidth,visiblePageHeight,songXOffset,songYOffset);
	if (currentlyPlaying){
		drawPlayBar(currentPlayPosition);
	}
	drawUIBar(myUIBar);
	drawString(positionString,uiPageSize*singleBlockSize+CONSTCHARW/2,visiblePageHeight*singleBlockSize+CONSTCHARW/2);
}

void addChar(char* _sourceString, char _addChar){
	int _gottenLength = strlen(_sourceString);
	_sourceString[_gottenLength]=_addChar;
	_sourceString[_gottenLength+1]='\0';
}

char uiHotkeyCheck(){
	int i;
	for (i=0;i<totalUI;++i){
		if (uiHotkeys[i]==lastSDLPressedKey){
			controlsEnd();
			myUIBar[i].activateFunc();
			controlLoop();
			return 1;
		}
	}
	return 0;
}

char noteHotkeyCheck(){
	int i;
	for (i=0;i<totalNotes;++i){
		if (noteHotkeys[i]==lastSDLPressedKey){
			// noteUIOrder
			int j;
			for (j=0;j<totalNotes;++j){
				if (noteUIOrder[j]==i){
					uiNoteIndex=j;
					updateNoteIcon();
					return 1;
				}
			}
		}
	}
	return 0;
}

void audioGearGUI(u8* _gearData){
	// UI variables
	uiElement* _foundUpUI = NULL;
	uiElement* _gearUIPointers[7];
	u8* _gearVolume = getGearVolume(_gearData);
	u8 _totalGearUI=0;
	// Add some UI elements
	_gearUIPointers[_totalGearUI++] = getUIByID(U_SELICON);
	// If we have up and down buttons, add them to this too.
	_foundUpUI = getUIByID(U_UPBUTTON);
	if (_foundUpUI!=NULL){
		_gearUIPointers[_totalGearUI++]=_foundUpUI;
		_gearUIPointers[_totalGearUI++]=getUIByID(U_DOWNBUTTON);
	}
	// Always have these
	_gearUIPointers[_totalGearUI++]=&volumeButtonUI;
	_gearUIPointers[_totalGearUI++]=&infoButtonUI;
	_gearUIPointers[_totalGearUI++]=&backButtonUI;

	noteSpot** _fakedMapArray=malloc(sizeof(noteSpot*)*songHeight);
	int i;
	for (i=0;i<songHeight;++i){
		_fakedMapArray[i] = calloc(1,sizeof(noteSpot)*AUDIOGEARSPACE);
	}
	for (i=0;i<AUDIOGEARSPACE;++i){
		if (_gearData[i*2]!=0){
			_fakedMapArray[_gearData[i*2+1]][i].id=_gearData[i*2];
		}
	}
	while(1){
		controlsStart();
		if (wasJustPressed(SCE_TOUCH)){
			int _placeX = touchXToBlock(touchX);
			int _placeY = touchYToBlock(touchY);
			if (_placeY==visiblePageHeight){
				if (_placeX<_totalGearUI){
					if (_gearUIPointers[_placeX]->uniqueId==U_BACK){
						break;
					}else if (_gearUIPointers[_placeX]->uniqueId==U_INFO){
						controlLoop();
						char _completeString[AUDIOGEARSPACE*3+1+(AUDIOGEARSPACE-1)];
						char _volumeString[strlen("Vol: ")+3+1];
						sprintf(_volumeString,"Vol: %d",*_gearVolume);
						_completeString[0]='\0';
						for (i=0;i<AUDIOGEARSPACE;++i){
							int j;
							for (j=0;j<songHeight;++j){
								if (_fakedMapArray[j][i].id!=0){
									if (i!=0){
										addChar(_completeString,' ');
									}
									addChar(_completeString,extraNoteInfo[_fakedMapArray[j][i].id].letter);
									addChar(_completeString,noteNames[j]);
									addChar(_completeString,extraNoteInfo[_fakedMapArray[j][i].id].accidental);
									break;
								}
							}
						}
						if (strlen(_completeString)==0){
							strcpy(_completeString,"NoData"); // Shrinking AUDIOGEARSPACE too much could cause this to overflow
						}
						while(1){
							controlsStart();
							if (wasJustPressed(SCE_TOUCH)){
								break;
							}
							controlsEnd();
							startDrawing();
							drawString(_completeString,0,0);
							drawString(_volumeString,0,CONSTCHARW);
							endDrawing();
						}

						controlLoop();
					}else if (_gearUIPointers[_placeX]->uniqueId==U_VOL){
						u8 _newVolume;
						do{
							_newVolume = getNumberInput("Audio Gear volume (1-100)",*_gearVolume);
						}while(_newVolume<=0 || _newVolume>100);
						*_gearVolume=_newVolume;
					}else{
						controlLoop();
						_gearUIPointers[_placeX]->activateFunc();
						controlLoop();
					}
				}
			}else{
				if (getUINoteID()==0 || extraNoteInfo[getUINoteID()].letter!=0){ // Restirct notes we can't use in audio gear to notes with audio gear letters or symbols
					// Clear any other notes we have in the same column.
					// Because 0 is a valid note id to place, clicking anywhere in a column with note id 0 will erase everything in that column
					for (i=0;i<songHeight;++i){
						_fakedMapArray[i][_placeX].id=0;
					}
					// Place our new note
					_placeNoteLow(_placeX,_placeY+songYOffset,getUINoteID(),optionPlayOnPlace,_fakedMapArray);
				}
			}
		}
		if (wasJustPressed(SCE_ANDROID_BACK)){
			break;
		}
		noteUIControls();
		if (lastSDLPressedKey!=SDLK_UNKNOWN){
			noteHotkeyCheck();
		}
		controlsEnd();
		startDrawing();
		drawSong(_fakedMapArray,AUDIOGEARSPACE,visiblePageHeight,0,songYOffset);
		drawUIPointers(_gearUIPointers,_totalGearUI);
		endDrawing();
	}
	//
	// Transfer data from song map to audio gear	
	for (i=0;i<AUDIOGEARSPACE;++i){
		int j;
		_gearData[i*2]=0; // If we don't find anything in the loop, this column will be empty.
		for (j=0;j<songHeight;++j){
			if (_fakedMapArray[j][i].id!=0){
				_gearData[i*2]=_fakedMapArray[j][i].id;
				_gearData[i*2+1]=j;
				break;
			}
		}
	}
	// Free faked map
	for (i=0;i<songHeight;++i){
		free(_fakedMapArray[i]);
	}
	free(_fakedMapArray);
}
void init(){
	initGraphics(832,480,&screenWidth,&screenHeight);
	initAudio();
	Mix_AllocateChannels(14*4); // We need a lot of channels for all these music notes
	setClearColor(192,192,192,255);
	if (screenWidth!=832 || screenHeight!=480){
		isMobile=1;
	}else{
		isMobile=ISTESTINGMOBILE;
	}
	if (isMobile){
		// An odd value for testing.
		generalScale=1.7;
	}else{
		generalScale=1;
	}
	if (generalScale!=0){
		singleBlockSize = 32*generalScale;
		logicalScreenWidth = floor(screenWidth/singleBlockSize)*singleBlockSize;
		logicalScreenHeight = floor(screenHeight/singleBlockSize)*singleBlockSize;
		globalDrawXOffset = (screenWidth-logicalScreenWidth)/2;
		globalDrawYOffset = (screenHeight-logicalScreenHeight)/2;
	}else{
		globalDrawXOffset=0;
		globalDrawYOffset=0;
		logicalScreenWidth = screenWidth;
		logicalScreenHeight = screenHeight;
		singleBlockSize=32;
	}
	visiblePageHeight = logicalScreenHeight/singleBlockSize;
	if (visiblePageHeight>15){
		visiblePageHeight=15;
	}
	visiblePageHeight--; // Leave a space for the toolbar.

	pageWidth = (logicalScreenWidth/singleBlockSize)-1;

	L = luaL_newstate();
	luaL_openlibs(L);
	pushLuaFunctions();

	songArray = calloc(1,sizeof(noteSpot*)*songHeight);
	setSongWidth(songArray,0,400);
	songWidth=400;
	setSongXOffset(0);

	initEmbeddedFont();

	// Init note array before we do the Lua
	setTotalNotes(1);
	noteImages[0] = loadEmbeddedPNG("assets/Free/Images/grid.png");

	// Set up UI
	uiElement* _newButton;

	// Add play button
	_newButton = addUI();
	playButtonImage = loadEmbeddedPNG("assets/Free/Images/playButton.png");
	stopButtonImage = loadEmbeddedPNG("assets/Free/Images/stopButton.png");
	_newButton->image = playButtonImage;
	_newButton->activateFunc = uiPlay;
	_newButton->uniqueId = U_PLAY;

	// Add selected note icon
	_newButton = addUI();
	_newButton->image = NULL;
	_newButton->activateFunc = uiNoteIcon;
	_newButton->uniqueId = U_SELICON;

	// Add save Button
	_newButton = addUI();
	_newButton->image = loadEmbeddedPNG("assets/Free/Images/saveButton.png");
	_newButton->activateFunc = uiSave;
	_newButton->uniqueId = U_SAVE;

	// Add page buttons
	_newButton = addUI();
	_newButton->image = loadEmbeddedPNG("assets/Free/Images/leftButton.png");
	_newButton->activateFunc = uiLeft;
	_newButton->uniqueId = U_LEFT;
	_newButton = addUI();
	_newButton->image = loadEmbeddedPNG("assets/Free/Images/rightButton.png");
	_newButton->activateFunc = uiRight;
	_newButton->uniqueId = U_RIGHT;
	if (visiblePageHeight!=pageHeight){
		_newButton = addUI();
		_newButton->image = loadEmbeddedPNG("assets/Free/Images/upButton.png");
		_newButton->activateFunc = uiUp;
		_newButton->uniqueId = U_UPBUTTON;
		_newButton = addUI();
		_newButton->image = loadEmbeddedPNG("assets/Free/Images/downButton.png");
		_newButton->activateFunc = uiDown;
		_newButton->uniqueId = U_DOWNBUTTON;
	}

	// Add yellow play button
	_newButton = addUI();
	yellowPlayButtonImage = loadEmbeddedPNG("assets/Free/Images/yellowPlayButton.png");
	_newButton->image = yellowPlayButtonImage;
	_newButton->activateFunc = uiYellowPlay;
	_newButton->uniqueId = U_YPLAY;

	// Put the less used buttons here
	_newButton = addUI();
	_newButton->image = loadEmbeddedPNG("assets/Free/Images/bpmButton.png");
	_newButton->activateFunc = uiBPM;
	_newButton->uniqueId = U_BPM;
	// 
	_newButton = addUI();
	_newButton->image = loadEmbeddedPNG("assets/Free/Images/resizeButton.png");
	_newButton->activateFunc = uiResizeSong;
	_newButton->uniqueId = U_SIZE;
	//
	_newButton = addUI();
	_newButton->image = loadEmbeddedPNG("assets/Free/Images/settingsButton.png");
	_newButton->activateFunc = uiSettings;
	_newButton->uniqueId = U_SETTINGS;
	//
	_newButton = addUI();
	_newButton->image = loadEmbeddedPNG("assets/Free/Images/countButton.png");
	_newButton->activateFunc = uiCount;
	_newButton->uniqueId = U_COUNT;
	//
	_newButton = addUI();
	_newButton->image = loadEmbeddedPNG("assets/Free/Images/creditsButton.png");
	_newButton->activateFunc = uiCredits;
	_newButton->uniqueId = U_CREDITS;
	//
	_newButton = addUI();
	_newButton->image = loadEmbeddedPNG("assets/Free/Images/loadButton.png");
	_newButton->activateFunc = uiLoad;
	_newButton->uniqueId = U_LOAD;
	//
	_newButton = addUI();
	_newButton->image = loadEmbeddedPNG("assets/Free/Images/optionsButton.png");
	_newButton->activateFunc = uiKeyConf;
	_newButton->uniqueId = U_KEYCONF;

	// Two general use UI buttons
	backButtonUI.image = loadEmbeddedPNG("assets/Free/Images/backButton.png");
	backButtonUI.activateFunc = NULL;
	backButtonUI.uniqueId=U_BACK;

	infoButtonUI.image = loadEmbeddedPNG("assets/Free/Images/infoButton.png");
	infoButtonUI.activateFunc = NULL;
	infoButtonUI.uniqueId=U_INFO;

	volumeButtonUI.image = loadEmbeddedPNG("assets/Free/Images/volumeButton.png");
	volumeButtonUI.activateFunc=NULL;
	volumeButtonUI.uniqueId=U_VOL;

	// If we need more space for the UI because screen in small, add moreUI button
	if (pageWidth<totalUI+3){ // 4 spaces for page number display. We only subtract 3 because we can use the labels on the far right as a UI spot
		uiPageSize=pageWidth-3;
		uiScrollImage = loadEmbeddedPNG("assets/Free/Images/moreUI.png");
		
		addUI(); // We just need an extra slot.
		uiElement _uiScrollButton;
		_uiScrollButton.image = uiScrollImage;
		_uiScrollButton.activateFunc=uiUIScroll;

		int i;
		for (i=totalUI-2;i>=uiPageSize-1;--i){
			myUIBar[i+1] = myUIBar[i];
		}
		memcpy(&(myUIBar[uiPageSize-1]),&_uiScrollButton,sizeof(uiElement));
		uiUIScrollIndex = uiPageSize-1;
	}else{
		uiPageSize=totalUI;
	}

	// Very last, run the init script
	fixPath("assets/Free/Scripts/init.lua",tempPathFixBuffer,TYPE_EMBEDDED);
	goodLuaDofile(L,tempPathFixBuffer);

	makeDataDirectory();

	loadSettings();
	// Load hotkey config here because all UI and notes should be added by now.
	loadHotkeys();

	// Hopefully we've added some note images in the init.lua.
	updateNoteIcon();
}
int main(int argc, char *argv[]){
	printf("Loading...\n");
	uiNoteIndex=1;
	init();
	printf("Done loading.\n");

	s16 _lastPlaceX=-1;
	s16 _lastPlaceY=-1;

	#ifdef ENABLEFPSCOUNTER
		int _countedFrames=0;
		int _lastFrameReport=getTicks();
	#endif
	// If not -1, draw a red rectangle at this UI slot
	s32 _uiSelectedHighlight=-1;
	while(1){
		controlsStart();
		if (isDown(SCE_TOUCH)){
			if (wasJustPressed(SCE_TOUCH)){ // If we're not dragging the mouse, you're allowed to place anywhere.
				_lastPlaceY=-1;
				_lastPlaceX=-1;
			}
			// We need to use floor so that negatives are not made positive
			int _placeX = touchXToBlock(touchX);
			int _placeY = touchYToBlock(touchY);
			if (_placeY==visiblePageHeight){ // If we click the UI section
				if (wasJustPressed(SCE_TOUCH)){
					_placeX+=uiScrollOffset;
					if (!(_placeX>=totalUI)){ // If we didn't click out of bounds
						if (myUIBar[_placeX].activateFunc==NULL){
							printf("NULL activateFunc for %d\n",_placeX);
						}else{
							controlsEnd();
							myUIBar[_placeX].activateFunc();
							controlLoop();
						}
						_uiSelectedHighlight=_placeX-uiScrollOffset;
					}
				}
			}else{ // If we click the main section
				if (!(_placeX>=pageWidth || _placeX<0)){ // In bounds in the main section, I mean.
					if (!(_placeX==_lastPlaceX && _placeY==_lastPlaceY)){ // Don't place where we've just placed. Otherwise we'd be placing the same note on top of itself 60 times per second
						_lastPlaceX = _placeX;
						_lastPlaceY = _placeY;
						placeNote(_placeX+songXOffset,_placeY+songYOffset,lastClickWasRight ? 0 : getUINoteID());
					}
				}
			}
		}
		noteUIControls();
		if (lastSDLPressedKey!=SDLK_UNKNOWN){
			if (!noteHotkeyCheck()){
				uiHotkeyCheck();
			}
		}
		controlsEnd();

		// Process playing
		if (currentlyPlaying){
			// If our body is ready for the next column of notes
			if (getTicks()>=lastPlayAdvance+bpmFormula(bpm)){
				lastPlayAdvance = getTicks(); // How long needs to pass before the next column
				currentPlayPosition = nextPlayPosition;
				nextPlayPosition = currentPlayPosition+1; // Advance one.
				centerAround(currentPlayPosition); // Move camera
				playColumn(currentPlayPosition);
			}
		}

		// Start drawing
		startDrawing();
		doUsualDrawing();
		// If we need to draw UI highlight
		if (_uiSelectedHighlight!=-1){
			drawRectangle(_uiSelectedHighlight*singleBlockSize,visiblePageHeight*singleBlockSize,singleBlockSize,singleBlockSize,0,0,0,100);
			_uiSelectedHighlight=-1;
		}
		endDrawing();

		#ifdef ENABLEFPSCOUNTER
			_countedFrames++;
			if (getTicks()>=_lastFrameReport+1000){
				printf("Fps:%d\n",_countedFrames);
				_lastFrameReport=getTicks();
				_countedFrames=0;
			}
		#endif
	}

	/* code */
	return 0;
}