#ifndef __INTERFACE_H
#define __INTERFACE_H

/*

Elecard STB820 Demo Application
Copyright (C) 2007  Elecard Devices

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA  02110-1301 USA

*/

/** @file interface.h Interface wifdets API
 */
/** @defgroup interface Interface features
 *  @ingroup StbMainApp
 *  @{
 */

/****************
* INCLUDE FILES *
*****************/

#include <limits.h>
#include <directfb.h>

#include "defines.h"
#include "stb_resource.h"
#include "app_info.h"

#ifdef WCHAR_SUPPORT
#include <wchar.h>
#include <wctype.h>
#include <locale.h>
#endif

/*******************
* EXPORTED MACROS  *
********************/

#ifndef GFX_USE_HELPER_SURFACE
#define DRAWING_SURFACE                         (pgfx_frameBuffer)
#else
#define DRAWING_SURFACE                         (pgfx_helperFrameBuffer)
#endif

#ifdef WCHAR_SUPPORT
#define ALTLAYOUT_DISABLED                      (-1)
#define ALTLAYOUT_OFF                           (0)
#define ALTLAYOUT_ON                            (1)
#endif

/* Length constraints */

#define MENU_MAX_ENTRIES                        (512)
#define SHORT_PATH                              (100)

#define MAX_MESSAGE_BOX_LENGTH                  (2048)

#define MAX_FIELD_PATTERN_LENGTH                (128)
#define MAX_FIELD_PATTERNS                      (10)
#define MAX_FIELD_VALUE                         (MAX_FIELD_PATTERNS*MAX_FIELD_PATTERN_LENGTH)

/* interfaceInfo.selectedItem special values */
#define MENU_ITEM_BACK                          (-1)
#define MENU_ITEM_MAIN                          (-2)

#define MAX_EVENTS                              (16)

#define NO_LOGO                                 (-1)

#define ALIGN_LEFT                              (0)
#define ALIGN_CENTER                            (1)
#define ALIGN_RIGHT                             (-1)

#define VALUE_NEXT                              (-1)
#define VALUE_PREV                              (-2)

#define PLAYCONTROL_TIMEOUT_MAX                 (5)
#define PLAYCONTROL_TIMEOUT_SLIDER              (10)

/* sizeof(interface_colors)/sizeof(*interface_colors) */
#define PREDEFINED_COLORS                       (2)

/* Layout */

#define SLIDER_DEFAULT_HEIGHT                   (18)

#define VKEYPAD_BUTTON_HEIGHT                   (24)
#define VKEYPAD_BUTTON_WIDTH                    (24)

#define KEYPAD_MAX_ROWS                         (8)
#define KEYPAD_MAX_CELLS                        (16)

#define INTERFACE_ARROW_IMAGE                   "arrow.png"
#define INTERFACE_BACKGROUND_IMAGE              "background.png"

/*! \def INTERFACE_SPLASH_IMAGE
    \brief Defines splash image.
   
    \warning Now it sets throw makefile.
*/
#if !defined(INTERFACE_SPLASH_IMAGE)
#define INTERFACE_SPLASH_IMAGE                  "splash.jpg"
#endif

#ifndef INTERFACE_WALLPAPER_IMAGE
#ifdef ENABLE_VIDIMAX
#define INTERFACE_WALLPAPER_IMAGE               "/usr/local/share/vidimax/images/main_background.png"
#else
#define INTERFACE_WALLPAPER_IMAGE               IMAGE_DIR "wallpaper.jpg"
#endif
#endif

#define INTERFACE_MARGIN_SIZE                   (90)
#define INTERFACE_BORDER_WIDTH                  (2)
#define INTERFACE_PADDING                       (5)
#define INTERFACE_THUMBNAIL_SIZE                (48)
#define INTERFACE_BIG_THUMBNAIL_SIZE            (120)
#define INTERFACE_VALUE_WIDTH                   (120)

#define INTERFACE_STATUSBAR_ICON_WIDTH          (66)
#define INTERFACE_STATUSBAR_ICON_HEIGHT         (34)

#define INTERFACE_ARROW_SIZE                    (16)
#define INTERFACE_PLAY_CONTROL_BUTTON_WIDTH     (40)
#define INTERFACE_PLAY_CONTROL_BUTTON_HEIGHT    (34)
#define INTERFACE_SLIDESHOW_BUTTON_WIDTH        (40)
#define INTERFACE_SLIDESHOW_BUTTON_HEIGHT       (40)
#define INTERFACE_SLIDER_CURSOR_WIDTH           (12)
#define INTERFACE_SLIDER_CURSOR_HEIGHT          (26)
#define INTERFACE_SPLASH_FRAMES                 (10)

#define INTERFACE_CLOCK_COLON_WIDTH             (8)
#define INTERFACE_CLOCK_DIGIT_WIDTH             (24)
#define INTERFACE_CLOCK_DIGIT_HEIGHT            INTERFACE_STATUSBAR_ICON_HEIGHT


#define INTERFACE_SOUND_CONTROL_BAR_WIDTH       (4)
#define INTERFACE_SOUND_CONTROL_BAR_HEIGHT      (100)

#define INTERFACE_SCROLLBAR_WIDTH               (20)
#define INTERFACE_SCROLLBAR_ARROW_SIZE          (16)

#define INTERFACE_MENU_ICON_WIDTH               (46)
#define INTERFACE_MENU_ICON_HEIGHT              (32)

#define INTERFACE_ANIMATION_STEP                (48)

#define INTERFACE_ROUND_CORNER_RADIUS           (16)

#define INTERFACE_POSTER_HEIGHT                 (256)
#define INTERFACE_POSTER_TITLE_HEIGHT           (38)
#define INTERFACE_POSTER_PICTURE_WIDTH          (160)

/* Colors */

//36495e
// 4f6178
#define INTERFACE_BACKGROUND_RED                0x00
#define INTERFACE_BACKGROUND_GREEN              0x00
#define INTERFACE_BACKGROUND_BLUE               0x00
#define INTERFACE_BACKGROUND_ALPHA              0x88

// 2a4c79
#define INTERFACE_BORDER_RED                    0x88
#define INTERFACE_BORDER_GREEN                  0x88
#define INTERFACE_BORDER_BLUE                   0x88
#define INTERFACE_BORDER_ALPHA                  0xDD

#define INTERFACE_HIGHLIGHT_BORDER_RED          0x60
#define INTERFACE_HIGHLIGHT_BORDER_GREEN        0xDD
#define INTERFACE_HIGHLIGHT_BORDER_BLUE         0x60
#define INTERFACE_HIGHLIGHT_BORDER_ALPHA        0xFF

// orange
//#define INTERFACE_HIGHLIGHT_RECT_RED            0xE7
//#define INTERFACE_HIGHLIGHT_RECT_GREEN          0x78
//#define INTERFACE_HIGHLIGHT_RECT_BLUE           0x17
// blue
#define INTERFACE_HIGHLIGHT_RECT_RED            0x78
#define INTERFACE_HIGHLIGHT_RECT_GREEN          0x78
#define INTERFACE_HIGHLIGHT_RECT_BLUE           0xFF
#define INTERFACE_HIGHLIGHT_RECT_ALPHA          0x88

#define INTERFACE_BOOKMARK_RED                  0xFF
#define INTERFACE_BOOKMARK_GREEN                0xFF
#define INTERFACE_BOOKMARK_BLUE                 0xFF
#define INTERFACE_BOOKMARK_ALPHA                0xFF

// green
//#define INTERFACE_BOOKMARK_SELECTED_RED         0x60
//#define INTERFACE_BOOKMARK_SELECTED_GREEN       0xDD
//#define INTERFACE_BOOKMARK_SELECTED_BLUE        0x60
// orange
#define INTERFACE_BOOKMARK_SELECTED_RED         0xE7
#define INTERFACE_BOOKMARK_SELECTED_GREEN       0x78
#define INTERFACE_BOOKMARK_SELECTED_BLUE        0x17
#define INTERFACE_BOOKMARK_SELECTED_ALPHA       0xFF

#define INTERFACE_BOOKMARK_DISABLED_RED         0xcd
#define INTERFACE_BOOKMARK_DISABLED_GREEN       0xcd
#define INTERFACE_BOOKMARK_DISABLED_BLUE        0xcd
#define INTERFACE_BOOKMARK_DISABLED_ALPHA       0xFF

#define INTERFACE_THUMBNAIL_BACKGROUND_RED      INTERFACE_BACKGROUND_RED
#define INTERFACE_THUMBNAIL_BACKGROUND_GREEN    INTERFACE_BACKGROUND_GREEN
#define INTERFACE_THUMBNAIL_BACKGROUND_BLUE     INTERFACE_BACKGROUND_BLUE
#define INTERFACE_THUMBNAIL_BACKGROUND_ALPHA    INTERFACE_BACKGROUND_ALPHA

#define INTERFACE_PLAY_CONTROL_BORDER_RED       INTERFACE_BORDER_RED
#define INTERFACE_PLAY_CONTROL_BORDER_GREEN     INTERFACE_BORDER_GREEN
#define INTERFACE_PLAY_CONTROL_BORDER_BLUE      INTERFACE_BORDER_BLUE
#define INTERFACE_PLAY_CONTROL_BORDER_ALPHA     INTERFACE_BORDER_ALPHA

#define INTERFACE_PLAY_CONTROL_BACKGROUND_RED   INTERFACE_BACKGROUND_RED
#define INTERFACE_PLAY_CONTROL_BACKGROUND_GREEN INTERFACE_BACKGROUND_GREEN
#define INTERFACE_PLAY_CONTROL_BACKGROUND_BLUE  INTERFACE_BACKGROUND_BLUE
#define INTERFACE_PLAY_CONTROL_BACKGROUND_ALPHA INTERFACE_BACKGROUND_ALPHA

#define INTERFACE_MESSAGE_BOX_RED               INTERFACE_BACKGROUND_RED
#define INTERFACE_MESSAGE_BOX_GREEN             INTERFACE_BACKGROUND_GREEN
#define INTERFACE_MESSAGE_BOX_BLUE              INTERFACE_BACKGROUND_BLUE
#define INTERFACE_MESSAGE_BOX_ALPHA             0xFF

#define INTERFACE_MESSAGE_BOX_BORDER_RED        INTERFACE_BORDER_RED
#define INTERFACE_MESSAGE_BOX_BORDER_GREEN      INTERFACE_BORDER_GREEN
#define INTERFACE_MESSAGE_BOX_BORDER_BLUE       INTERFACE_BORDER_BLUE
#define INTERFACE_MESSAGE_BOX_BORDER_ALPHA      INTERFACE_BORDER_ALPHA

#define INTERFACE_SCROLLBAR_COLOR_RED           0xA0
#define INTERFACE_SCROLLBAR_COLOR_GREEN         0xA0
#define INTERFACE_SCROLLBAR_COLOR_BLUE          0xA0
#define INTERFACE_SCROLLBAR_COLOR_ALPHA         0xB0

#define INTERFACE_SCROLLBAR_COLOR_LT_RED        0xE0
#define INTERFACE_SCROLLBAR_COLOR_LT_GREEN      0xE0
#define INTERFACE_SCROLLBAR_COLOR_LT_BLUE       0xE0
#define INTERFACE_SCROLLBAR_COLOR_LT_ALPHA      0xD0

#define INTERFACE_SCROLLBAR_COLOR_DK_RED        0x30
#define INTERFACE_SCROLLBAR_COLOR_DK_GREEN      0x30
#define INTERFACE_SCROLLBAR_COLOR_DK_BLUE       0x30
#define INTERFACE_SCROLLBAR_COLOR_DK_ALPHA      0xD0

#define INTERFACE_SOUND_CONTROL_RED             0x00
#define INTERFACE_SOUND_CONTROL_GREEN           0xFF
#define INTERFACE_SOUND_CONTROL_BLUE            0x00
#define INTERFACE_SOUND_CONTROL_ALPHA           0xFF

/******************************************************************
* EXPORTED TYPEDEFS                                               *
*******************************************************************/

typedef enum
{
	interfaceAlignTop =                         0x01,
	interfaceAlignMiddle =                      0x02,
	interfaceAlignBottom =                      0x04,
	interfaceAlignLeft =                        0x08,
	interfaceAlignCenter =                      0x10,
	interfaceAlignRight =                       0x20
} interfaceAlign_t;

typedef enum
{
	interfaceBorderSideTop =                    1,
	interfaceBorderSideRight =                  2,
	interfaceBorderSideBottom =                 4,
	interfaceBorderSideLeft =                   8,
	interfaceBorderSideAll =                    1+2+4+8
} interfaceBorderSide_t;

typedef enum
{
	interfaceMenuList = 0,
	interfaceMenuCustom
} interfaceMenuType_t;

typedef enum
{
	interfaceListMenuNoThumbnail = 0,
	interfaceListMenuBigThumbnail,
	interfaceListMenuIconThumbnail,
	interfaceListMenuTypeCount
} interfaceListMenuType_t;

typedef enum
{
	interfaceMenuEntryCustom = 0,
	interfaceMenuEntryImage,
	interfaceMenuEntryText,
	interfaceMenuEntryHeading,
	interfaceMenuEntryEdit
} interfaceMenuEntryType_t;

typedef enum
{
	interfaceEditCustom = 0,
	interfaceEditString,
	interfaceEditNumber,
	interfaceEditIP,
	interfaceEditTime,
	interfaceEditDate,
	interfaceEditOnOff,
	interfaceEditSelect
} interfaceEditEntryType_t;

typedef enum
{
	menuDefaultActionShowMenu = 1,
	menuDefaultActionCount
} interfaceMenuActionType_t;

typedef enum
{
	interfaceCommandNone =                       0,
	interfaceCommandUp =                         DIKS_CURSOR_UP,
	interfaceCommandDown =                       DIKS_CURSOR_DOWN,
	interfaceCommandLeft =                       DIKS_CURSOR_LEFT,
	interfaceCommandRight =                      DIKS_CURSOR_RIGHT,
	interfaceCommandEnter =                      DIKS_ENTER,
	interfaceCommandOk =                         DIKS_OK,
	interfaceCommandToggleMenu =                 DIKS_SPACE,
#if !(defined STB225)
	interfaceCommandBack =                       DIKS_BACKSPACE,
#else
	interfaceCommandBack =                       DIKS_PAUSE, // this code sends button TEXT in top right corner of arrow buttons
#endif
	interfaceCommandExit =                       DIKS_ESCAPE,
	interfaceCommandMainMenu =                   DIKS_HOME,
	interfaceCommand1 =                          DIKS_1,
	interfaceCommand2 =                          DIKS_2,
	interfaceCommand3 =                          DIKS_3,
	interfaceCommand4 =                          DIKS_4,
	interfaceCommand5 =                          DIKS_5,
	interfaceCommand6 =                          DIKS_6,
	interfaceCommand7 =                          DIKS_7,
	interfaceCommand8 =                          DIKS_8,
	interfaceCommand9 =                          DIKS_9,
	interfaceCommand0 =                          DIKS_0,
	interfaceCommandVolumeUp =                   DIKS_PLUS_SIGN,
	interfaceCommandVolumeDown =                 DIKS_MINUS_SIGN,
	interfaceCommandVolumeMute =                 DIKS_ASTERISK,
	interfaceCommandPrevious =                   'z',
	interfaceCommandRewind =                     'x',
	interfaceCommandStop =                       'c',
	interfaceCommandPause =                      'v',
	interfaceCommandPlay =                       'b',
	interfaceCommandFastForward =                'n',
	interfaceCommandNext =                       'm',
	interfaceCommandRecord =                     DIKS_RECORD,
	interfaceCommandChangeMenuStyle =            DIKS_TAB,
	interfaceCommandRed =                        DIKS_F1,
	interfaceCommandGreen =                      DIKS_F2,
#if !(defined STB225)
	interfaceCommandYellow =                     DIKS_F3,
#else
	interfaceCommandYellow =                     61490, // this code sends blue button TIMER ander arrow buttons
#endif
	interfaceCommandBlue =                       DIKS_F4,
	interfaceCommandChannelUp =                  DIKS_CHANNEL_UP,
	interfaceCommandChannelDown =                DIKS_CHANNEL_DOWN,
//	interfaceCommandServices =                   'i',
	interfaceCommandServices =                   DIKS_INFO,
	interfaceCommandTV =                         DIKS_TV,
	interfaceCommandPhone =                      DIKS_PHONE,
	interfaceCommandWeb =                        DIKS_FAVORITES,
	interfaceCommandPageUp =                     DIKS_PAGE_UP,
	interfaceCommandPageDown =                   DIKS_PAGE_DOWN,
	interfaceCommandRefresh =                    DIKS_RESTART,
	interfaceCommandMic =                        DIKS_AUX,

	interfaceCommandCount
} interfaceCommand_t;

typedef struct
{
	int repeat;
	int source;
	interfaceCommand_t command;
	interfaceCommand_t original;
} interfaceCommandEvent_t, *pinterfaceCommandEvent_t;

typedef enum
{
	interfacePlayControlPlay =                   0x0001,
	interfacePlayControlPause =                  0x0002,
	interfacePlayControlStop =                   0x0004,
	interfacePlayControlRewind =                 0x0008,
	interfacePlayControlFastForward =            0x0010,
	interfacePlayControlPrevious =               0x0020,
	interfacePlayControlNext =                   0x0040,
	interfacePlayControlInfo =                   0x0080,
	interfacePlayControlSetPosition =            0x0100,
	interfacePlayControlAudioTracks =            0x0200,
	interfacePlayControlAddToPlaylist =          0x0400,
	interfacePlayControlMode =                   0x0800,
	interfacePlayControlTimeout =                0x1000,
	interfacePlayControlRecord =                 0x2000,
	interfacePlayControlAll =                   (0x2000 << 1)-1
} interfacePlayControlButton_t;

typedef enum
{
	interfaceSoundControlActionVolumeUp = 1,
	interfaceSoundControlActionVolumeDown,
	interfaceSoundControlActionVolumeMute,
	interfaceSoundControlActionVolumeHide
} interfaceSoundControlAction_t;

typedef enum
{
	interfaceAnimationNone = 0,
	interfaceAnimationVerticalCinema,
	interfaceAnimationVerticalPanorama,
	interfaceAnimationHorizontalPanorama,
	interfaceAnimationHorizontalSlide,
	interfaceAnimationHorizontalStripes,
	interfaceAnimationCount
} interfaceAnimation_t;

typedef enum
{
	inputFocusPlayControl = 0,
	inputFocusSlider,
	inputFocusSliderMoving,
	inputFocusSlideshow
} interfaceInputFocus_t;

typedef struct
{
	int R;
	int G;
	int B;
	int A;
} interfaceColor_t;

typedef struct
{
	int Enable;
	int R;
	int G;
	int B;
	int A;
	char image[PATH_MAX];
} interfaceBackground_t;

struct __interfaceMenu_t;
struct __interfaceEditEntry_t;

typedef int (*playControlProcessCommandFunction)(pinterfaceCommandEvent_t, void*);
typedef int (*menuProcessCommandFunction)(struct __interfaceMenu_t*, pinterfaceCommandEvent_t);
typedef void (*menuEventFunction)(struct __interfaceMenu_t*);
typedef int (*menuActionFunction)(struct __interfaceMenu_t*, void*);
typedef int (*menuEntryDisplayFunction)(struct __interfaceMenu_t*, DFBRectangle *, int);
typedef int (*playControlChannelCallback)(int, void*);
typedef void (*playControlDisplayFunction)();
typedef void (*playControlSetupFunction)(void*);
typedef int (*menuConfirmFunction)(struct __interfaceMenu_t*, pinterfaceCommandEvent_t, void*);
typedef int (*menuEnterTextFunction)(struct __interfaceMenu_t*, char *, void*);
typedef char *(*menuEnterTextFieldsFunction)(int, void*);
typedef int (*eventActionFunction)(void*);
typedef int (*playControlCallback)(interfacePlayControlButton_t, void*);
typedef int (*soundControlCallback)(interfaceSoundControlAction_t, void*);
typedef int (*editEntryProcessCommandFunction)(pinterfaceCommandEvent_t, struct __interfaceEditEntry_t*);

typedef struct
{
#ifdef WCHAR_SUPPORT
	wchar_t data[MAX_FIELD_PATTERN_LENGTH];
#else
	char    data[MAX_FIELD_PATTERN_LENGTH];
#endif
} interfaceEnterTextInfoSubField_t;

typedef struct
{
	char description[MENU_ENTRY_INFO_LENGTH];

	char value[MAX_FIELD_VALUE];
	char pattern[MAX_FIELD_PATTERN_LENGTH];
	interfaceEnterTextInfoSubField_t subPatterns[MAX_FIELD_PATTERNS];

	int currentPattern;
	int currentSymbol;

#ifdef WCHAR_SUPPORT
	wchar_t lastChar;
#else
	char lastChar;
#endif

	menuEnterTextFunction pCallback;
	void *pArg;

	stb810_inputMode inputMode;

} interfaceEnterTextInfo_t;

typedef struct
{
	interfaceMenuEntryType_t type;
	char                     info[MENU_ENTRY_INFO_LENGTH];
	menuActionFunction       pAction;
	menuActionFunction       pSelectedAction;
	menuActionFunction       pDeselectedAction; /**< if type is interfaceMenuEntryEdit, used as reset action */
	menuEntryDisplayFunction pDisplay;
	void                    *pArg; /**< if type is interfaceMenuEntryEdit, MUST point to interfaceEditEntry_t structure */
	int                      isSelectable;
	int                      infoReplacedCharPos;
	int                      infoReplacedChar;
	int                      thumbnail;
	char                     image[MENU_ENTRY_INFO_LENGTH];/** alternative thumbnail*/
} interfaceMenuEntry_t;

typedef struct
{
	int  selected;
	char value[3+3+3+3+1];
} interfaceEditIP_t;

typedef enum
{
	interfaceEditTimeCommon = 0, /**< Useful for time spaces */
	interfaceEditTime01,         /**< One hour or one minute */
	interfaceEditTime24          /**< Day time */
} interfaceEditTimeType_t;

typedef struct
{
	int  selected;
	char value[2+2+1];   /**< HHMM + trailing zero for functions like sprintf */
	interfaceEditTimeType_t type;
} interfaceEditTime_t;

typedef struct
{
	int  selected;
	char value[2+2+4+1]; /**< DDMMYYYY + trailing zero for functions like sprintf */
} interfaceEditDate_t;

typedef struct
{
	int  selected;
	int  count;
} interfaceEditSelect_t;

typedef struct __interfaceEditEntry_t
{
	void                    *pArg;
	editEntryProcessCommandFunction pCallback;
	int                      active;
	interfaceEditEntryType_t type;
	union {
		char                  text[MENU_ENTRY_INFO_LENGTH]; /**< String value */
		int                   checked;                      /**< On/off value */
		interfaceEditIP_t     ip;
		interfaceEditTime_t   time;
		interfaceEditDate_t   date;
		interfaceEditSelect_t select;
	} info;
} interfaceEditEntry_t;

typedef struct __interfaceMenu_t
{
	interfaceMenuType_t menuType;

	char name[MENU_ENTRY_INFO_LENGTH];

	int selectedItem;

	/*
	int clientX;
	int clientY;
	int clientWidth;
	int clientHeight;
	*/

	int thumbnailWidth;
	int thumbnailHeight;

	int menuEntryCount;
	interfaceMenuEntry_t menuEntry[MENU_MAX_ENTRIES];

	menuProcessCommandFunction processCommand;
	menuConfirmFunction pCustomKeysCallback;
	menuEventFunction displayMenu;
	menuEventFunction reinitializeMenu;

	menuActionFunction pActivatedAction;
	menuActionFunction pDeactivatedAction;

	int logo;
	int logoX; // when set to -1, displayed in menu title
	int logoY;
	int logoWidth;
	int logoHeight;

	int statusBarIcons[4]; // indices of statusbar hint icons

	void *pArg;

	struct __interfaceMenu_t *pParentMenu;
} interfaceMenu_t;

typedef struct
{
	interfaceMenu_t baseMenu;

	interfaceListMenuType_t listMenuType;

	int infoAreaX;
	int infoAreaY;
	int infoAreaWidth;
	int infoAreaHeight;

} interfaceListMenu_t;

typedef struct
{
	int showingLength;
	int length;
	char number[4];
	playControlChannelCallback pSet; // Should not stop current channel if new channel number is invalid
} interfaceChannelControl_t;

typedef struct
{
	int enabled;

	interfacePlayControlButton_t enabledButtons;

	interfacePlayControlButton_t activeButton;
	interfacePlayControlButton_t highlightedButton;

	void *pArg;
	playControlDisplayFunction pDisplay;
	playControlProcessCommandFunction pProcessCommand; // returns 0 when command has been processed by custom callback, non-zero otherwise
	playControlCallback pCallback;                // returns 0 when button has been processed by callback, non-zero otherwise
	playControlChannelCallback pChannelChange;    // 0 = next, 1 = previous
	eventActionFunction pAudioChange;             // called after successfull audio track change

	int positionX;
	int positionY;
	int width;
	int height;

	int visibleFlag;
	int showTimeout;
	int showOnStart;
	int showState; // when set to 1, current media description and status icon are displayed

	char description[MENU_ENTRY_INFO_LENGTH];
	int descriptionThumbnail;

	unsigned int sliderStart;
	unsigned int sliderEnd;
	unsigned int sliderPos;
	unsigned int sliderPointer;
	int alwaysShowSlider; // when set to 1, slider is visible even when play control is not
} interfacePlayControl_t;

typedef struct
{
	int enabled;

	interfacePlayControlButton_t highlightedButton;

	int positionX;
	int positionY;
	int width;
	int height;

	int visibleFlag;

} interfaceSlideshowControl_t;

typedef struct
{

	soundControlCallback pCallback;
	void *pArg;

	long minValue;
	long maxValue;
	long curValue;

	int muted;

	int visibleFlag;
} interfaceSoundControl_t;

typedef struct
{
	eventActionFunction pAction;
	void *pArg;
	time_t counter;
	struct timeval startTime;
} interfaceEvent_t;

typedef enum
{
	interfaceMessageBoxNone = 0,
	interfaceMessageBoxSimple,
	interfaceMessageBoxCallback,
	interfaceMessageBoxScrolling,
	interfaceMessageBoxPoster
} interfaceMessageBoxType_t;

typedef struct
{
	interfaceMessageBoxType_t type;
	int   icon;
	char  title[MENU_ENTRY_INFO_LENGTH];
	char  message[MAX_MESSAGE_BOX_LENGTH];
	int   offset;
	int   maxOffset;
	int   visibleLines;
	int   lineCount;
	interfaceColor_t titleColor;
	char *poster;
	/** If type is interfaceMessageBoxCallback then return value 0 closes message box.
	 * If type is interfaceMessageBoxScrolling then return value 0 mean command was processed by callback,
	 * non-zero mean to execute default action (scroll or close)
	 */
	menuConfirmFunction       pCallback;
	void *pArg;
} interfaceMessageBox_t;

typedef void (*sliderSetCallback)(long, void*);
typedef long (*sliderGetCallback)(void*);

typedef struct
{
	int minValue;
	int maxValue;
	int divisions;
	int width;
	int height;
	int hideDelay;
	sliderGetCallback getCallback;
	sliderSetCallback setCallback;
	menuConfirmFunction customKeyCallback;
	char textValue[MENU_ENTRY_INFO_LENGTH];
	void *pArg;
} interfaceSlider_t;

typedef struct
{
	char caption[MENU_ENTRY_INFO_LENGTH];
	int min;
	int max;
	int value;
	int steps;
} interfaceCustomSlider_t;

typedef struct
{
	int show;
	int pageNumber;
} interfaceTeletext_t;

typedef int (*customSliderFunction)(int, interfaceCustomSlider_t*, void*);

typedef struct
{
	int enable;
	int shift;
	int row;
	int cell;
#ifdef WCHAR_SUPPORT
	int altLayout;   // -1 = disabled, 0 = default, 1 = local
#endif
} keypadInfo_t;

typedef struct
{
	int screenWidth;
	int screenHeight;

	int clientX;
	int clientY;
	int clientWidth;
	int clientHeight;

	int highlightColor;

	int borderWidth;
	int paddingSize;
	int marginSize;
	int thumbnailSize;

	interfaceMenu_t *currentMenu; // MUST be not null

	int showMenu;
	int showLoadingAnimation;
	int showSliderControl;
	int showIncomingCall;
	int enableVoipIndication;

	keypadInfo_t keypad;
	int showLoading;

	int enableClock;
	int lockMenu;

	char cleanUpState;

	char notifyText[MENU_ENTRY_INFO_LENGTH];

	interfaceTeletext_t teletext;

	interfaceEvent_t event[MAX_EVENTS];
	interfaceMessageBox_t messageBox;
	interfaceAnimation_t animation;

	int eventCount;
#if (defined STB225)
	char enable3d;
	char mode3D;
	// 0 - no 3D at all
	// 1 - 3D video and interface not in 3D
#endif

	interfaceBackground_t background;

	customSliderFunction customSlider;
	int					 customSliderVisibleInMenu;
	void*				 customSliderArg;

	interfaceInputFocus_t inputFocus; // input focus in play control menu: slider, play control, slideshow
} interfaceInfo_t;

/*******************************************************************
* EXPORTED DATA                                                    *
********************************************************************/

extern interfaceInfo_t interfaceInfo;

extern interfacePlayControl_t interfacePlayControl;

extern interfaceSlideshowControl_t interfaceSlideshowControl;

extern interfaceChannelControl_t interfaceChannelControl;

extern const interfaceColor_t interface_colors[];

#ifdef WCHAR_SUPPORT
extern wchar_t keypad_local[8][16];

extern wchar_t keypad[KEYPAD_MAX_ROWS][KEYPAD_MAX_CELLS];
#endif

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

#ifdef __cplusplus
extern "C" {
#endif

//#ifdef STB225
//#define inline
//#endif

/* Initialization/deinitialization */

/* MUST be called after gfx_init() */
void interface_init(void);

void interface_destroy(void);

/* Tools */

int getMaxStringLength(char *string, int maxWidth);

int getMaxStringLengthForFont(IDirectFBFont *font, char *string, int maxWidth);

void smartLineTrim(char *string, int length);

/**
 *  @brief Pre-format text to fit in specified region using word wrap
 *
 *  @param[in]  text          Source text
 *  @param[in]  font          Font to be used for drawing
 *  @param[in]  maxWidth      Width of the region
 *  @param[in]  maxHeight     Height of the region
 *  @param[in]  dest_size     Size of formatted text buffer
 *  @param[out] dest          Formatted text
 *  @param[out] lineCount     Total line count of formatted text
 *  @param[out] visibleLines  Line count that fit specified region
 *
 *  @return Height of visible text
 */
int interface_formatTextWW(char *text, IDirectFBFont *font, int maxWidth, int maxHeight, int dest_size, char *dest, int *lineCount, int *visibleLines);

/* Interface primitives */

/**
 *  @brief Load icon from icon container file (which can contain many icons) and draw it on surface
 *
 *  @param[in]  pSurface      Destination surface
 *  @param[in]  path          Path to icon container file
 *  @param[in]  x             X coordinate on destination surface
 *  @param[in]  y             Y Coordinate on destination surface
 *  @param[in]  w             Width of icon (all icons in container must be of same size)
 *  @param[in]  h             Height of icon (all icons in container must be of same size)
 *  @param[in]  row           Row number of icon in container file
 *  @param[in]  cell          Cell number of icon in container file
 *  @param[in]  blend         Blitting flags (blending mode)
 *  @param[in]  align         Icon align relative to x and y (OR'ed vertical and horizontal align flags)
 *
 *  @return 0 on success
 */
int interface_drawIcon(IDirectFBSurface *pSurface, char *path, int x, int y, int w, int h, int row, int col, DFBSurfaceBlittingFlags blend, interfaceAlign_t align);

/**
 *  @brief Load and draw image
 *
 *  @param[in]  pSurface      Destination surface
 *  @param[in]  path          Path to image file
 *  @param[in]  x             X coordinate on destination surface
 *  @param[in]  y             Y coordinates on destination surface
 *  @param[in]  w             Width of image (treating of this parameter depends on stretch parameter value)
 *  @param[in]  h             Height of image (treating of this parameter depends on stretch parameter value)
 *  @param[in]  stretch       0 means that w and h are maximal dimensions of image (if image is larger then it'll be scaled down)
 *                            1 means that image must be scaled to match w and h
 *  @param[in]  clip          Clip rectangle (clipping is applied to image after it's scaled) (can be NULL)
 *  @param[in]  blend         Blitting flags (blending mode)
 *  @param[in]  align         Image align relative to x and y (OR'ed vertical and horizontal align flags)
 *  @param[out] rw            Resulting width of image to return to caller (can be NULL)
 *  @param[out] rh            Resulting height of image to return to caller (can be NULL)
*
 *  @return 0 on success
*/
int interface_drawImage(IDirectFBSurface *pSurface, char *path, int x, int y, int w, int h, int stretch, DFBRectangle *clip, DFBSurfaceBlittingFlags blend, interfaceAlign_t align, int *rw, int *rh);

/**
 *  @brief Draw border inside specified region
 *
 *  @param[in]  pSurface      Destination surface
 *  @param[in]  r
 *  @param[in]  g
 *  @param[in]  b
 *  @param[in]  a
 *  @param[in]  x             X coordinates on destination surface
 *  @param[in]  y             Y coordinates on destination surface
 *  @param[in]  w             Width of destination region
 *  @param[in]  h             Height of destination region
 *  @param[in]  border        Border width
 *  @param[in]  sides         Visible border sides
 */
void interface_drawInnerBorder(IDirectFBSurface *pSurface,
                               int r, int g, int b, int a,
                               int x, int y, int w, int h,
                               int border, interfaceBorderSide_t sides);

/**
 *  @brief Draw border surrounding specified region
 *
 *  @param[in]  pSurface      Destination surface
 *  @param[in]  r
 *  @param[in]  g
 *  @param[in]  b
 *  @param[in]  a
 *  @param[in]  x             X coordinates on destination surface
 *  @param[in]  y             Y coordinates on destination surface
 *  @param[in]  w             Width of destination region
 *  @param[in]  h             Height of destination region
 *  @param[in]  border        Border width
 *  @param[in]  sides         Visible border sides
 */
void interface_drawOuterBorder(IDirectFBSurface *pSurface,
                               int r, int g, int b, int a,
                               int x, int y, int w, int h,
                               int border, interfaceBorderSide_t sides);

/**
 *  @brief Draw vertical scrolling bar
 *
 *  @param[in]  pSurface      Destination surface
 *  @param[in]  x
 *  @param[in]  y
 *  @param[in]  w
 *  @param[in]  h
 *  @param[in]  lineCount     Total count of displaying lines
 *  @param[in]  visibleLines  Lines that fit destintation region
 *  @param[in]  lineOffset    Current scrolling position
*/
void interface_drawScrollingBar(IDirectFBSurface *pSurface, int x, int y, int w, int h, int lineCount, int visibleLines, int lineOffset);

/**
 *  @brief Draw text in specified region with smart word wrapping
 *
 *  @param[in]  font         Font to use
 *  @param[in]  r
 *  @param[in]  g
 *  @param[in]  b
 *  @param[in]  a
 *  @param[in]  x             X coordinates on destination surface
 *  @param[in]  y             Y coordinates on destination surface
 *  @param[in]  w             Width of destination region
 *  @param[in]  h             Height of destination region
 *  @param[in]  text          Text to draw
 *  @param[in]  align         
 *
 *  @return Height of visible text
 */
int interface_drawTextWW(IDirectFBFont *font, int r, int g, int b, int a, int x, int y, int w, int h, char *text, int align);

/**
 *  @brief Draw menu bookmark with specified text
 *
 *  @param[in]  pSurface      Destination surface
 *  @param[in]  pFont         Font to use
 *  @param[in]  x             Horizontal coordinate of bookmark
 *  @param[in]  y             Bottom of bookmark, top line of menu
 *  @param[in]  pText         Bookmark text
 *  @param[in]  selected      Draw text highlighted
 *  @param[out] endx          X coordinate of right border of drawn bookmark
 */
void interface_drawBookmark(IDirectFBSurface *pSurface, IDirectFBFont *pFont,
                            int x, int y, char *pText, int selected, int *endx);

void interface_processCommand(pinterfaceCommandEvent_t cmd);

/**
 *  @brief Draw menu if it is enabled with interface_showMenu
 *
 *  @param[in] flipFB         If set to 1 displays redrawn menu on screen
 */
void interface_displayMenu(int flipFB);

/**
 *  @brief Sets default menu values
 *
 *  @param[in]  pMenu         Pointer to interfaceMenu_t to be reinitiated
 */
void interface_menuReset(interfaceMenu_t *pMenu);

/**
 *  @brief Initiates interfaceMenu_t structure
 *
 *  @param[in]  pMenu               Pointer to interfaceMenu_t to be initiated
 *  @param[in]  type
 *  @param[in]  name
 *  @param[in]  logo                Index of menu logo in resource_thumbnails array
 *  @param[in]  statusIcons         int[4] containing statusbar hint icon indices for functional buttons
 *  @param[in]  pParent             Pointer to parent menu
 *  @param[in]  processCommand      Used to customize menu input
 *  @param[in]  displayCommand      Used to customize menu draw
 *  @param[in]  reinitializeCommand Used to set default menu values
 *  @param[in]  pActivatedAction    Called when user enters menu
 *  @param[in]  pDeactivatedAction  Called when user leaves menu
 *  @param[in]  pArg                Custom data associated with menu
 *
 *  @return Pointer to created menu
 *  @sa createListMenu()
 */
interfaceMenu_t *createBasicMenu( interfaceMenu_t *pMenu, interfaceMenuType_t type, char *name, int logo, int* statusIcons, interfaceMenu_t *pParent, menuProcessCommandFunction processCommand, menuEventFunction displayCommand, menuEventFunction reinitializeCommand, menuActionFunction pActivatedAction, menuActionFunction pDeactivatedAction, void *pArg);

/**
 *  @brief Initiates interfaceListMenu_t structure
 *
 *  @param[in]  pMenu               Pointer to interfaceMenu_t to be initiated
 *  @param[in]  name
 *  @param[in]  logo                Index of menu logo in resource_thumbnails array
 *  @param[in]  statusIcons         int[4] containing statusbar hint icon indices for functional buttons
 *  @param[in]  pParent             Pointer to parent menu
 *  @param[in]  viewType            List menu view type
 *  @param[in]  pActivatedAction    Called when user enters menu
 *  @param[in]  pDeactivatedAction  Called when user leaves menu. If return non-zero prevents leaving menu
 *  @param[in]  pArg                Custom data associated with menu
 *
 *  @return Pointer to created menu
 *  @sa createBasicMenu()
 */
 
interfaceListMenu_t *createListMenu(interfaceListMenu_t *pMenu, char *name, int logo, int* statusIcons, interfaceMenu_t *pParent, interfaceListMenuType_t viewType, menuActionFunction pActivatedAction, menuActionFunction pDeactivatedAction, void *pArg);

/**
 *  @brief Adds new entry to specified menu
 *
 *  @param[in]  pMenu               Pointer to interfaceMenu_t for entry to be added to
 *  @param[in]  type
 *  @param[in]  data                If type is interfaceMenuEntryText, then data is entry text
 *  @param[in]  dataSize            If type is interfaceMenuEntryText, then dataSize should be length of entry text with trailing zero
 *  @param[in]  isSelectable        If set to 1, menu entry can be highlighted and toggles callbacks
 *  @param[in]  pActivate           Called when user activates menu item
 *  @param[in]  pSelectedFunc       Called when this entry is selected in menu
 *  @param[in]  pDeselectedFunc     Called when selection moves from this entry
 *  @param[in]  pDisplay            Called on drawing menu. If NULL, interface_menuEntryDisplay is used
 *  @param[in]  pArg                Custom data, associated with menu
 *  @param[in]  thumbnail           Index of icon used for this entry
 *
 *  @return New pMenu entry count or -1 on error
 *  @sa interface_addMenuEntry()
 *  @sa interface_addMenuEntryDisabled()
 */
int interface_addMenuEntryCustom(interfaceMenu_t *pMenu, interfaceMenuEntryType_t type, const void *data, int dataSize, int isSelectable, menuActionFunction pActivate, menuActionFunction pSelectedFunc, menuActionFunction pDeselectedFunc, menuEntryDisplayFunction pDisplay, void *pArg, int thumbnail);

/**
 *  @brief Adds new selectable text entry to specified menu
 *
 *  @param[in]  pMenu               Pointer to interfaceMenu_t for entry to be added to
 *  @param[in]  text
 *  @param[in]  pActivate           Called when user activates menu item
 *  @param[in]  pArg                Custom data, associated with menu
 *  @param[in]  thumbnail           Index of icon used for this entry
 *
 *  @return New pMenu entry count or -1 on error
 *  @sa interface_addMenuEntryCustom()
 *  @sa interface_addMenuEntryDisabled()
 */
int interface_addMenuEntry(interfaceMenu_t *pMenu, const char *text, menuActionFunction pActivate, void *pArg, int thumbnail);

/**
 *  @brief Adds new disabled text entry to specified menu
 *
 *  @param[in]  pMenu               Pointer to interfaceMenu_t for entry to be added to
 *  @param[in]  text
 *  @param[in]  thumbnail           Index of icon used for this entry
 *
 *  @return New pMenu entry count or -1 on error
 *  @sa interface_addMenuEntry()
 *  @sa interface_addMenuEntryCustom()
 */
int interface_addMenuEntryDisabled(interfaceMenu_t *pMenu, const char *text, int thumbnail);

/**
 *  @brief Adds new edit date entry to specified menu
 *
 *  Date-specific fields os pEditEntry are not changed
 *
 *  @param[in]  pMenu               Pointer to interfaceMenu_t for entry to be added to
 *  @param[in]  text
 *  @param[in]  pActivate           Called when user finished editing
 *  @param[in]  pReset              Called when user canceled editing
 *  @param[in]  pArg                Custom data, associated with menu
 *  @param[in]  thumbnail           Index of icon used for this entry
 *  @param[out] pEditEntry          Edit entry associated with menu entry
 *
 *  @return New pMenu entry count or -1 on error
 *  @sa interface_addMenuEntryCustom()
 */
int interface_addEditEntryDate(interfaceMenu_t *pMenu, const char *text, menuActionFunction pActivate, menuActionFunction pReset, void *pArg, int thumbnail, interfaceEditEntry_t *pEditEntry);

/**
 *  @brief Adds new edit time entry to specified menu
 *
 *  Time-specific fields os pEditEntry are not changed
 *
 *  @param[in]  pMenu               Pointer to interfaceMenu_t for entry to be added to
 *  @param[in]  text
 *  @param[in]  pActivate           Called when user finished editing
 *  @param[in]  pReset              Called when user canceled editing
 *  @param[in]  pArg                Custom data, associated with menu
 *  @param[in]  thumbnail           Index of icon used for this entry
 *  @param[out] pEditEntry          Edit entry associated with menu entry
 *
 *  @return New pMenu entry count or -1 on error
 *  @sa interface_addMenuEntryCustom()
 */
int interface_addEditEntryTime(interfaceMenu_t *pMenu, const char *text, menuActionFunction pActivate, menuActionFunction pReset, void *pArg, int thumbnail, interfaceEditEntry_t *pEditEntry);

/**
 *  @brief Default list menu entry display function
 *  @param[in]  pMenu               Pointer to list menu, containing entry
 *  @param[in]  rect                List entry region to be drawed to
 *  @param[in]  entryIndex          Index of entry in pMenu
 *  @return 0 on success, -1 if unsupported menu or entry type
 */
int interface_menuEntryDisplay(interfaceMenu_t *pMenu, DFBRectangle *rect, int entryIndex);

int interface_setSelectedItem(interfaceMenu_t *pMenu, int index);

void interface_clearMenuEntries(interfaceMenu_t *pMenu);

void interface_showSplash(int x, int y, int w, int h, int animate, int showMenu);

/**
 *  @brief Used to change current menu. Calls pDectivatedAction of old menu, pActivatedAction of new menu and plays animation if menu changed
 *
 *  @param[in]  pMenu               Pointer to menu, which called the function
 *  @param[in]  pArg                Pointer to new menu
 *  @return Result of pActivatedAction of new menu or 0
 */
int interface_menuActionShowMenu(interfaceMenu_t *pMenu, void *pArg);

/**
 *  @brief Enables and configures play control.
 *
 *  @param[in]  pCallback           Custom input callback
 *  @param[in]  pArg                Custom data, associated with play control
 *  @param[in]  buttons             Enabled buttons of play control
 *  @param[in]  description         Text displayed on top of the screen
 *  @param[in]  thumbnail           Thumbnail dsplayed on top of the screen
 */
void interface_playControlSetup(playControlCallback pCallback, void *pArg, interfacePlayControlButton_t buttons, char *description, int thumbnail);

void interface_playControlSetProcessCommand(playControlProcessCommandFunction pProcessCommand);

void interface_playControlSetChannelCallbacks(playControlChannelCallback pChannelChange, playControlChannelCallback pSetChannelNumber);

void interface_playControlSetAudioCallback(eventActionFunction pAudioChange);

void interface_playControlSetDisplayFunction(playControlDisplayFunction pDisplay);

void interface_playControlSetButtons(interfacePlayControlButton_t buttons);

interfacePlayControlButton_t interface_playControlGetButtons();

/** Set play control visible and reset play control hide event
 *  @param[in] redraw If set to 1, redraw interface immediately
 */
void interface_playControlRefresh(int redraw);

void interface_playControlHide(int redraw);

void interface_playControlDisable(int redraw);

void interface_playControlReset();

void interface_channelNumberShow(int channelNumber);

/**
 *  @brief Used to toggle menu visibility on the screen or to switch to/from play control
 *
 *  @param[in]  show    1 - show menu, 0 - hide menu
 *  @param[in]  redraw  Redraw menu immediately
 */
void interface_showMenu(int show, int redraw);

void interface_playControlSelect(interfacePlayControlButton_t button);

void interface_playControlHighlight(interfacePlayControlButton_t button);

/** Default multiview process command function
 *  @param[in]  cmd     Command to process
 *  @param[in]  pArg    User data
 *  @return     0 if processed, 1 if command skipped
 */
int interface_multiviewProcessCommand(pinterfaceCommandEvent_t cmd, void *pArg);

/* Displays media description box on top of the screen */
void interface_showPlayState();

/**
 *  @brief Adds new event to interface event pool
 *
 *  @param[in]  pAction             Function to execute on timer
 *  @param[in]  pArg                Used to distinguish between events with same pAction.
 *  @param[in]  counter             Timeout in milliseconds to wait before executing pAction
 *  @param[in]  replaceSimilar      If set to 1 then if event with equal pAction & pArg already exists in pool, resets event's timer to counter instead of adding new event
 *
 *  @return 0 on success
 */
int s_interface_addEvent(eventActionFunction pAction, void *pArg, int counter, int replaceSimilar);

#ifdef EVENT_TRACE
#define interface_addEvent(a, b, c ,d) s_interface_addEvent(a, b, c, d); printf("s_interface_addEvent from %s (%s, %s)\n", __FUNCTION__, #a, #b);
#else
#define interface_addEvent s_interface_addEvent
#endif

int interface_removeEvent(eventActionFunction pAction, void *pArg);

/**
 *  @brief Displays box with text and optionally icon at specified position on the screen.
 *
 *  @param[in]  targetX
 *  @param[in]  targetY
 *  @param[in]  message
 *  @param[in]  icon
 *  @param[in]  fixedWidth          If non-zero used to limit width of message box (including borders). If fixedWidth<0, then left alignment is used
 *  @param[in]  resultingBox        Used to store screen position and size of displayed message box
 *  @param[in]  addHeight           Area of this height is reserved inside message box after text (used in slider control)
 *  @param[in]  icons               If not NULL, array of 4 indices of helper icons
 */
void interface_displayCustomTextBox( int targetX, int targetY, char *message, char *icon, int fixedWidth, DFBRectangle *resultingBox, int addHeight, int *icons );

void interface_displayTextBox(int targetX, int targetY, char *message, char *icon, int fixedWidth, DFBRectangle *resultingBox, int addHeight);

void interface_displayTextBoxColor( int targetX, int targetY, char *message, char *icon, int fixedWidth, DFBRectangle *resultingBox, int addHeight,
                                    int br, int bg, int bb, int ba,
                                    int tr, int tg, int tb, int ta);

void interface_displayCustomTextBoxColor( int targetX, int targetY, char *message, char *icon, int fixedWidth, DFBRectangle *resultingBox, int addHeight, int *icons,
                                          int br, int bg, int bb, int ba,
                                          int tr, int tg, int tb, int ta,
                                          int r, int g, int b, int a,
                                          IDirectFBFont *pFont);

void interface_displayScrollingTextBox( int x, int y, int w, int h, char *message, int lineOffset, int visibleLines, int lineCount, int icon);

void interface_displayCustomScrollingTextBox( int x, int y, int w, int h, char *message, int lineOffset, int visibleLines, int lineCount, int icon);

void interface_showLoading();

void interface_hideLoading();

void interface_showLoadingAnimation();

void interface_hideLoadingAnimation();

/**
 *  @brief Displays message box in the center of screen, which hides when any key is pressed
 *
 *  show*Box functions works in non-blocking mode, passing input to default or specified callback until interface_hideMessageBox is called
 *
 *  @param[in]  text                Text to display
 *  @param[in]  icon                Icon index or 0 if none
 *  @param[in]  hideDelay           If greater than zero, time in milliseconds after which message will be hidden automatically
 *  @sa interface_showScrollingBox()
 *  @sa interface_showConfirmationBox()
 *  @sa interface_showPosterBox()
 *  @sa interface_hideMessageBox()
 */
void interface_showMessageBox(char *text, int icon, int hideDelay);

/**
 *  @brief Displays message box with scrollable text and custom user input handler
 *
 *  @param[in]  text                Text to display
 *  @param[in]  icon                Icon index or 0 if none
 *  @param[in]  pCallback           User input handler
 *  @param[in]  pArg                Used as argument to pCallback
 *  @sa interface_showMessageBox()
 *  @sa interface_showConfirmationBox()
 *  @sa interface_showPosterBox()
 *  @sa interface_hideMessageBox()
 */
void interface_showScrollingBox(char *text, int icon, menuConfirmFunction pCallback, void *pArg);

/**
 *  @brief Displays message box with poster, scrollable text and custom user input handler
 *
 *  @param[in]  text                Text to display. Can be NULL, so only poster image is displayed
 *  @param[in]  title               Title of the poster box
 *  @param[in]  tr
 *  @param[in]  tg
 *  @param[in]  tb
 *  @param[in]  ta
 *  @param[in]  icon                Icon index or 0 if none
 *  @param[in]  poster              Url of poster image
 *  @param[in]  pCallback           User input handler
 *  @param[in]  pArg                Used as argument to pCallback
 *  @sa interface_showMessageBox()
 *  @sa interface_showConfirmationBox()
 *  @sa interface_showScrollingBox()
 *  @sa interface_hideMessageBox()
 */
void interface_showPosterBox(char *text, char *title, int tr, int tg, int tb, int ta, int icon, char *poster, menuConfirmFunction pCallback, void *pArg);

/**
 *  @brief Displays message box with custom user input handler
 *
 *  @param[in]  text                Text to display
 *  @param[in]  icon                Icon index or 0 if none
 *  @param[in]  pCallback           User input handler
 *  @param[in]  pArg                Used as argument to pCallback
 *  @sa interface_showMessageBox()
 *  @sa interface_showScrollingBox()
 *  @sa interface_showPosterBox()
 *  @sa interface_hideMessageBox()
 */
void interface_showConfirmationBox(char *text, int icon, menuConfirmFunction pCallback, void *pArg);

/**
 *  @brief Hide displayed message box
 *
 *  @sa interface_showMessageBox()
 *  @sa interface_showScrollingBox()
 *  @sa interface_showConfirmationBox()
 */
void interface_hideMessageBox();

void interface_soundControlSetup(soundControlCallback pAction, void *pArg, long min, long max, long cur);

void interface_soundControlRefresh(int redraw);

int  interface_soundControlProcessCommand(pinterfaceCommandEvent_t cmd);

void interface_soundControlSetValue(int value);

void interface_soundControlSetMute(int muteFlag);

void interface_sliderSetKeyCallback(menuConfirmFunction callback);

void interface_sliderSetHideDelay(int delay);

void interface_sliderSetText(char *text);

void interface_sliderSetMinValue(int minValue);

void interface_sliderSetMaxValue(int maxValue);

void interface_sliderSetCallbacks(sliderGetCallback getcallback, sliderSetCallback setcallback, void *pArg);

void interface_sliderSetDivisions(int pos);

void interface_sliderSetSize(int width, int height);

void interface_sliderShow(int bShowFlag, int bRedrawFlag);

void interface_notifyText(char *text, int bRedrawFlag);

void interface_setMenuLogo(interfaceMenu_t *pMenu, int logo, int x, int y, int w, int h);

/**
 *  @brief Displays input text dialog
 *
 *  @param[in]  pMenu               Menu which called the function
 *  @param[in]  description
 *  @param[in]  pattern             Simple pattern to match input
 *  @param[in]  pCallback           Callback to process user input
 *  @param[in]  pGetFields          Callback to fill pattern with default values
 *  @param[in]  inputMode           Digits only or text
 *  @param[in]  pArg                Used as argument to callback functions
 *
 *  @return Ignore it
 */
int interface_getText(interfaceMenu_t *pMenu, char *description, char *pattern, menuEnterTextFunction pCallback, menuEnterTextFieldsFunction pGetFields, stb810_inputMode inputMode, void *pArg);

/** Text input process command function.
 *  Used by interface_getText internally
 *  @param[in]  pMenu   Menu which called the function
 *  @param[in]  cmd     Command to process
 *  @param[in]  pArg    Pointer to interfaceEnterTextInfo_t structure
 *  @return     0 if processed, 1 if command skipped
 */
int interface_enterTextProcessCommand(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg);

/** Extract text value from interfaceEnterTextInfo_t structure
 *  Text is stored in field->value property
 *  @param[in]  field   Pointer to interfaceEnterTextInfo_t structure
 *  @return     0 on success
 */
int interface_enterTextGetValue( interfaceEnterTextInfo_t *field );

/** Format content of interfaceEnterTextInfo_t to be showed on screen
 *  @param[in]  field   Pointer to interfaceEnterTextInfo_t structure
 *  @param[in]  bufLength Output buffer size
 *  @param[out] buf     
 *  @return     0 on success
 */
int interface_enterTextShow( interfaceEnterTextInfo_t *field, const size_t bufLength, char *buf );

void interface_disableBackground();

void interface_setBackground(int r, int g, int b, int a, char *image);

void interface_playControlSlider(unsigned int start, unsigned int end, unsigned int pos);

int interface_playControlSliderIsEnabled();

void interface_playControlSliderEnable(int enable);

float interface_playControlSliderGetPosition();

void interface_playControlSetInputFocus(interfaceInputFocus_t newFocus);

void interface_playControlUpdateDescription(char* description);

void interface_playControlUpdateDescriptionThumbnail(char* description, int thumbnail);

void interface_setCustomKeysCallback(interfaceMenu_t *pMenu, menuConfirmFunction pCallback);

void interface_setCustomDisplayCallback(interfaceMenu_t *pMenu, menuEventFunction pCallback);

int  interface_getMenuEntryCount(interfaceMenu_t *pMenu);

int interface_getMenuEntryInfo(interfaceMenu_t *pMenu, int entryIndex, char* entryInfo, int entryInfoLength);

void* interface_getMenuEntryArg(interfaceMenu_t *pMenu, int entryIndex);

menuActionFunction interface_getMenuEntryAction(interfaceMenu_t *pMenu, int entryIndex);

void interface_setMenuName(interfaceMenu_t *pMenu, char* name, int nameLength);

int interface_getSelectedItem(interfaceMenu_t *pMenu);

int interface_isMenuEntrySelectable(interfaceMenu_t *pMenu, int index);

void interface_showSlideshowControl();

void interface_slideshowControlDisplay();

int interface_slideshowControlProcessCommand(pinterfaceCommandEvent_t cmd);

int interface_getTextBoxMaxLineCount();

void interface_flipSurface();

int interface_scrollingMessageBoxCallback(interfaceMenu_t* pMenu, pinterfaceCommandEvent_t cmd, void* pArg);

void interface_customSlider(customSliderFunction pFunction, void *pArg, int showOverMenu, int bRedrawFlag);

void interface_listMenuDisplay(interfaceMenu_t *pMenu);

int interface_listMenuProcessCommand(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd);

int interface_MenuDefaultProcessCommand(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd);

int interface_displayCustomSlider(customSliderFunction pCallback, void *pCallbackArg, int drawBox, int cx, int cy, int cw, IDirectFBFont *pFont);

void interface_drawRoundBox(int x, int y, int w, int h);

void interface_drawRoundBoxColor(int x, int y, int w, int h, int r, int g, int b, int a);

void interface_listMenuGetItemInfo(interfaceListMenu_t *pListMenu, int* itemHeight, int* maxVisibleItems);

int interface_symbolLookup( int num, int repeat, int *offset );

/** Draw current time in bottom right corner
 * @param[in] detached If true, draw rounded corners on top, if false - draw solid rectangle (clock will be part of standard list menu) 
 */
void interface_displayClock(int detached);

#ifdef STB225
#undef inline
#endif

#ifdef __cplusplus
}
#endif

/** @} interface */

#endif // __INTERFACE_H
