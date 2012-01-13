/*

Elecard STB Application
Copyright (C) 2011  "Элекард-ЦТП"

*/

#if !defined(__STB225_H)
	#define __STB225_H


/*******************
* INCLUDE FILES    *
********************/


/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

#ifdef __cplusplus
extern "C" {
#endif


/**
 *   @brief Function used to change maping frame buffers to output screen
 *
 *   @param  path          I     Path to device (e.g /dev/fb[0|1]) that will be mapping
 *   @param  dest_x        I     X location of mapping device on output screen
 *   @param  dest_y        I     Y location of mapping device on output screen
 *   @param  dest_width    I     Width of mapping device on output screen
 *   @param  dest_height   I     Height of mapping device on output screen
 *
 *   @retval not 0 if error occure
 *           0 if not.
 */
int32_t Stb225ChangeDestRect(char *path, uint32_t dest_x, uint32_t dest_y, uint32_t dest_width, uint32_t dest_height);

/**
 *   @brief Function used to initialize ir receiver and leds on stb830(stb225)
 *
 *   @retval not 0 if error occure
 *           0 if not.
 */
int32_t Stb225initIR(void);


#ifdef __cplusplus
}
#endif


#endif //#if !defined(__STB225_H)
