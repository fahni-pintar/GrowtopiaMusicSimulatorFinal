/*
https://www.lua.org/manual/5.3/manual.html#lua_pushlightuserdata
https://github.com/mlabbe/nativefiledialog

todo - Add volume setting. Morons want it and don't know how to use volume mixer
todo - note names are useless for anything but audio gears, note counter can display icons
todo - don't forget to add audio gear volume setting
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <GeneralGoodConfig.h>
#include <GeneralGood.h>
#include <GeneralGoodExtended.h>
#include <GeneralGoodGraphics.h>
#include <GeneralGoodSound.h>
#include <GeneralGoodText.h>
#include <GeneralGoodImages.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define ISTESTINGMOBILE 0
#define DISABLESOUND 0
#define DOFANCYPAGE 1
#define DOCENTERPLAY 0

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define s8 int8_t
#define s16 int16_t
#define s32 int32_t

#define BGMODE_SINGLE 1
#define BGMODE_PART 2

#define UNIQUE_SELICON 1 // Stands for "select icon"
#define UNIQUE_PLAY 2
#define UNIQUE_YPLAY 3

#define LUAREGISTER(x,y) lua_pushcfunction(L,x);\
	lua_setglobal(L,y);

////////////////////////////////////////////////
typedef void(*voidFunc)();
typedef struct{
	CrossTexture* image;
	voidFunc activateFunc;
	s16 uniqueId;
}uiElement;
typedef struct{
	u8 id;
	void* extraData; // For audio gears, this could be their data. For repeat notes, it could be temp data about if they've been used or not.
}noteSpot;
void drawSong();
void drawUI();
void playColumn(s32 _columnNumber);
void pageTransition(int _destX);
////////////////////////////////////////////////
u8 optionPlayOnPlace=1;
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

CrossTexture* playButtonImage;
CrossTexture* stopButtonImage;
CrossTexture* yellowPlayButtonImage;

u16 songWidth=400;
u16 songHeight=14;

double generalScale;

u16 singleBlockSize=32;

u8 isMobile;

s32 selectedNote=0;
u16 totalNotes=0;

u8 currentlyPlaying=0;
s32 currentPlayPosition;
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

noteSpot** songArray;

u32 bpm=100;

////////////////////////////////////////////////

u32 bpmFormula(u32 re){
	return 60000 / (4 * re);
}
// Given note wait time, find BPM
u32 reverseBPMformula(u32 re){
	return 15000/re;
}

void centerAround(u32 _passedPosition){
	#if DOCENTERPLAY
		u16 _halfScreenWidth = ceil(pageWidth/2);
		if (_passedPosition<_halfScreenWidth){
			songXOffset=0;
			return;
		}
		if (_passedPosition>songWidth-1-_halfScreenWidth){
			songXOffset = songWidth-1-pageWidth;
			return;
		}
		songXOffset = _passedPosition-_halfScreenWidth;
	#else
		songXOffset = (_passedPosition/pageWidth)*pageWidth;
	#endif
}

uiElement* getUIByID(s16 _passedId){
	int i;
	for (i=0;i<totalUI;++i){
		if (myUIBar[i].uniqueId==_passedId){
			return &(myUIBar[i]);
		}
	}
	printf("Couldn't find the UI with id %d. You didn't delete it, right?",_passedId);
	return NULL;
}

void updateNoteIcon(){
	uiElement* _noteIconElement = getUIByID(UNIQUE_SELICON);
	if (_noteIconElement!=NULL){
		_noteIconElement->image=noteImages[selectedNote];
	}
}

void togglePlayUI(){
	uiElement* _playButtonUI = getUIByID(UNIQUE_PLAY);
	if (_playButtonUI->image==stopButtonImage){
		_playButtonUI->image = playButtonImage;
	}else{
		_playButtonUI->image = stopButtonImage;
	}
	_playButtonUI = getUIByID(UNIQUE_YPLAY);
	if (_playButtonUI->image==stopButtonImage){
		_playButtonUI->image = yellowPlayButtonImage;
	}else{
		_playButtonUI->image = stopButtonImage;
	}
}

void playAtPosition(s32 _startPosition){
	if (!currentlyPlaying){
		pageTransition(_startPosition);
		songXOffset=_startPosition;
		togglePlayUI();
		currentlyPlaying=1;
		currentPlayPosition = songXOffset;
		lastPlayAdvance=getTicks();
		centerAround(currentPlayPosition);
		playColumn(currentPlayPosition);
	}else{
		togglePlayUI();
		currentlyPlaying=0;
		songXOffset = (currentPlayPosition/pageWidth)*pageWidth;
	}
}

void uiYellowPlay(){
	playAtPosition(songXOffset);
}

void uiPlay(){
	playAtPosition(0);
}

void pageTransition(int _destX){
	#if DOFANCYPAGE
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
			songXOffset+=_changeAmount;
			if (songXOffset>=_positiveDest){
				songXOffset = _positiveDest;
				break;
			}
			if (songXOffset<=_negativeDest){
				songXOffset = _negativeDest;
				break;
			}
			startDrawing();
			drawSong();
			drawUI();
			endDrawing();
		}
	#else
		songXOffset = _destX;
	#endif
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
	if (songXOffset+pageWidth>=songWidth-1){
		pageTransition(0);
	}else{
		pageTransition(songXOffset+pageWidth);
	}
}
void uiLeft(){
	if (songXOffset<pageWidth){
		pageTransition(songWidth-1-pageWidth);
	}else{
		pageTransition(songXOffset-pageWidth);
	}
}

void uiNoteIcon(){
	selectedNote++;
	if (selectedNote==totalNotes){
		selectedNote=0;
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
	myUIBar[totalUI-1].uniqueId=-1;
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
void setSongWidth(noteSpot** _passedArray, u16 _passedOldWidth, u16 _passedWidth){
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

void XOutFunction(){
	exit(0);
}

void setTotalNotes(u16 _newTotal){
	if (_newTotal<=totalNotes){
		return;
	}
	noteImages = realloc(noteImages,sizeof(CrossTexture*)*_newTotal);
	noteSounds = realloc(noteSounds,sizeof(CROSSSFX**)*_newTotal);

	int i;
	for (i=totalNotes;i<_newTotal;++i){
		noteSounds[i] = malloc(songHeight*sizeof(CROSSSFX*));
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
	return 0;
}
// Add a UI slot and return its number
int L_addUI(lua_State* passedState){
	addUI();
	lua_pushnumber(passedState,totalUI-1);
	return 1;
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
	LUAREGISTER(L_addUI,"addUI");
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

void goodPlaySound(CROSSSFX* _passedSound){
	#if !DISABLESOUND
		if (_passedSound!=NULL){
			playSound(_passedSound,1,0);
		}
	#endif
}

void playColumn(s32 _columnNumber){
	int i;
	for (i=0;i<pageHeight;++i){
		if (songArray[i][_columnNumber].id!=0){
			goodPlaySound(noteSounds[songArray[i][_columnNumber].id][i]);
		}
	}
}

void placeNote(int _x, int _y, u16 _noteId){
	if (songArray[_y][_x].id!=_noteId && noteSounds[_noteId][_y]!=NULL && optionPlayOnPlace){
		goodPlaySound(noteSounds[_noteId][_y]);
	}
	songArray[_y][_x].id=_noteId;
}

void drawPlayBar(int _x){
	drawRectangle((_x-songXOffset)*singleBlockSize,0,singleBlockSize,visiblePageHeight*singleBlockSize,128,128,128,100);
}

void drawSong(){
	int i;
	if (backgroundMode==BGMODE_SINGLE){
		drawImageScaleAlt(bigBackground,0,0,generalScale,generalScale);
	}
	for (i=0;i<visiblePageHeight;++i){
		int j;
		for (j=0;j<pageWidth;++j){
			if (songArray[i+songYOffset][j+songXOffset].id==0){
				// If we're doing the special part display mode
				if (backgroundMode==BGMODE_PART){
					drawImageScaleAlt(bgPartsEmpty[i+songYOffset],j*singleBlockSize,i*singleBlockSize,generalScale,generalScale);
				}
			}else{
				drawImageScaleAlt(noteImages[songArray[i+songYOffset][j+songXOffset].id],j*singleBlockSize,i*singleBlockSize,generalScale,generalScale);
			}
		}
		if (backgroundMode==BGMODE_PART){
			drawImageScaleAlt(bgPartsLabel[i+songYOffset],pageWidth*singleBlockSize,i*singleBlockSize,generalScale,generalScale);
		}
	}
}

void drawUI(){
	int i;
	for (i=0;i<totalUI;++i){
		drawImageScaleAlt(myUIBar[i].image,i*singleBlockSize,visiblePageHeight*singleBlockSize,generalScale,generalScale);
	}
}

void init(){
	initGraphics(832,480,&screenWidth,&screenHeight);
	initAudio();
	Mix_AllocateChannels(14*2); // We need a lot of channels for all these music notes
	setClearColor(192,192,192,255);
	if (screenWidth!=832 || screenHeight!=480){
		isMobile=1;
	}else{
		isMobile=ISTESTINGMOBILE;
	}
	if (isMobile){
		// An odd value for testing.
		generalScale=1.3;
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
	_newButton->uniqueId = UNIQUE_PLAY;

	// Add selected note icon
	_newButton = addUI();
	_newButton->image = NULL;
	_newButton->activateFunc = uiNoteIcon;
	_newButton->uniqueId = UNIQUE_SELICON;

	// Add save Button
	_newButton = addUI();
	_newButton->image = loadEmbeddedPNG("assets/Free/Images/saveButton.png");
	_newButton->activateFunc = NULL;

	// Add page buttons
	_newButton = addUI();
	_newButton->image = loadEmbeddedPNG("assets/Free/Images/leftButton.png");
	_newButton->activateFunc = uiLeft;
	_newButton = addUI();
	_newButton->image = loadEmbeddedPNG("assets/Free/Images/rightButton.png");
	_newButton->activateFunc = uiRight;
	if (visiblePageHeight!=pageHeight){
		_newButton = addUI();
		_newButton->image = loadEmbeddedPNG("assets/Free/Images/upButton.png");
		_newButton->activateFunc = uiUp;
		_newButton = addUI();
		_newButton->image = loadEmbeddedPNG("assets/Free/Images/downButton.png");
		_newButton->activateFunc = uiDown;
	}

	// Add yellow play button
	_newButton = addUI();
	yellowPlayButtonImage = loadEmbeddedPNG("assets/Free/Images/yellowPlayButton.png");
	_newButton->image = yellowPlayButtonImage;
	_newButton->activateFunc = uiYellowPlay;
	_newButton->uniqueId = UNIQUE_YPLAY;

	// Very last, run the init script
	fixPath("assets/Free/Scripts/init.lua",tempPathFixBuffer,TYPE_EMBEDDED);
	goodLuaDofile(L,tempPathFixBuffer);

	// Hopefully we've added some note images in the init.lua.
	updateNoteIcon();
}

int main(int argc, char *argv[]){
	selectedNote=1;
	init();
	// If not -1, draw a red rectangle at this UI slot
	s32 _uiSelectedHighlight=-1;
	while(1){
		int i;
		controlsStart();
		if (isDown(SCE_TOUCH)){
			// We need to use floor so that negatives are not made positive
			int _placeX = floor((touchX-globalDrawXOffset)/singleBlockSize);
			int _placeY = floor((touchY-globalDrawYOffset)/singleBlockSize);
			if (_placeY==visiblePageHeight){ // If we click the UI section
				if (wasJustPressed(SCE_TOUCH)){
					for (i=0;i<totalUI;++i){
						if (_placeX==i){
							if (myUIBar[i].activateFunc==NULL){
								printf("NULL activateFunc for %d\n",i);
							}else{
								myUIBar[i].activateFunc();
							}
							_uiSelectedHighlight=i;
						}
					}
				}
			}else{ // If we click the main section
				if (!(_placeX>=pageWidth || _placeX<0)){ // In bounds in the main section, I mean.
					placeNote(_placeX+songXOffset,_placeY+songYOffset,selectedNote);
				}
			}
		}
		if (wasJustPressed(SCE_MOUSE_SCROLL)){
			if (mouseScroll<0){
				selectedNote--;
				if (selectedNote<0){
					selectedNote=totalNotes-1;
				}
				updateNoteIcon();
			}else if (mouseScroll>0){
				selectedNote++;
				if (selectedNote==totalNotes){
					selectedNote=0;
				}
				updateNoteIcon();
			}// 0 scroll is ignored
		}
		controlsEnd();

		// Process playing
		if (currentlyPlaying){
			if (getTicks()>=lastPlayAdvance+bpmFormula(bpm)){
				lastPlayAdvance = getTicks();
				currentPlayPosition++;
				centerAround(currentPlayPosition);
				playColumn(currentPlayPosition);
			}
		}

		// Start drawing
		startDrawing();
		drawSong();
		if (currentlyPlaying){
			drawPlayBar(currentPlayPosition);
		}
		drawUI();
		// If we need to draw UI highlight
		if (_uiSelectedHighlight!=-1){
			drawRectangle(_uiSelectedHighlight*singleBlockSize,visiblePageHeight*singleBlockSize,singleBlockSize,singleBlockSize,0,0,0,100);
			_uiSelectedHighlight=-1;
		}
		endDrawing();
	}

	/* code */
	return 0;
}
