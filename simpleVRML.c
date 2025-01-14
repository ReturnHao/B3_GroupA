/*
 *	simpleVRML.c
 *
 *	Demonstration of ARToolKit with models rendered in VRML.
 *
 *  Press '?' while running for help on available key commands.
 *
 *	Copyright (c) 2002 Mark Billinghurst (MB) grof@hitl.washington.edu
 *	Copyright (c) 2004 Raphael Grasset (RG) raphael.grasset@hitlabnz.org.
 *	Copyright (c) 2004-2006 Philip Lamb (PRL) phil@eden.net.nz. 
 *	
 *	Rev		Date		Who		Changes
 *	1.0.0	????-??-??	MB		Original from ARToolKit
 *  1.0.1   2004-10-29  RG		Fix for ARToolKit 2.69.
 *  1.0.2   2004-11-30  PRL     Various fixes.
 *
 */
/*
 * 
 * This file is part of ARToolKit.
 * 
 * ARToolKit is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * ARToolKit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with ARToolKit; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 */


// ============================================================================
//	Includes
// ============================================================================

#ifdef _WIN32
#  include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#  include <GLUT/glut.h>
#else
#  include <GL/glut.h>
#endif

#include <png.h>

#include <AR/config.h>
#include <AR/video.h>
#include <AR/param.h>			// arParamDisp()
#include <AR/ar.h>
#include <AR/gsub_lite.h>
#include <AR/arvrml.h>

#include "object.h"

// ============================================================================
//	Constants
// ============================================================================

#define VIEW_SCALEFACTOR		0.025		// 1.0 ARToolKit unit becomes 0.025 of my OpenGL units.
#define VIEW_SCALEFACTOR_1		1.0			// 1.0 ARToolKit unit becomes 1.0 of my OpenGL units.
#define VIEW_SCALEFACTOR_4		4.0			// 1.0 ARToolKit unit becomes 4.0 of my OpenGL units.
#define VIEW_DISTANCE_MIN		4.0			// Objects closer to the camera than this will not be displayed.
#define VIEW_DISTANCE_MAX		4000.0		// Objects further away from the camera than this will not be displayed.


// ============================================================================
//	Global variables
// ============================================================================

// Preferences.
static int prefWindowed = TRUE;
static int prefWidth = 640;					// Fullscreen mode width.
static int prefHeight = 480;				// Fullscreen mode height.
static int prefDepth = 32;					// Fullscreen mode bit depth.
static int prefRefresh = 0;					// Fullscreen mode refresh rate. Set to 0 to use default rate.

// Image acquisition.
static ARUint8		*gARTImage = NULL;

// Marker detection.
static int			gARTThreshhold = 100;
static long			gCallCountMarkerDetect = 0;

// Transformation matrix retrieval.
static int			gPatt_found = FALSE;	// At least one marker.

// Drawing.
static ARParam		gARTCparam;
static ARGL_CONTEXT_SETTINGS_REF gArglSettings = NULL;

// Object Data.
static ObjectData_T	*gObjectData;
static int			gObjectDataCount;

// *** Changed ***
static int Cnt[31];
static int Disp[31][31];
static ARMarkerInfo Marker_store[31][31];

// ============================================================================
//	Functions
// ============================================================================

static int setupCamera(const char *cparam_name, char *vconf, ARParam *cparam)
{	
    ARParam			wparam;
	int				xsize, ysize;

    // Open the video path.
    if (arVideoOpen(vconf) < 0) {
    	fprintf(stderr, "setupCamera(): Unable to open connection to camera.\n");
    	return (FALSE);
	}
	
    // Find the size of the window.
    if (arVideoInqSize(&xsize, &ysize) < 0) return (FALSE);
    fprintf(stdout, "Camera image size (x,y) = (%d,%d)\n", xsize, ysize);
	
	// Load the camera parameters, resize for the window and init.
    if (arParamLoad(cparam_name, 1, &wparam) < 0) {
		fprintf(stderr, "setupCamera(): Error loading parameter file %s for camera.\n", cparam_name);
        return (FALSE);
    }
    arParamChangeSize(&wparam, xsize, ysize, cparam);
    fprintf(stdout, "*** Camera Parameter ***\n");
    arParamDisp(cparam);
	
    arInitCparam(cparam);

	if (arVideoCapStart() != 0) {
    	fprintf(stderr, "setupCamera(): Unable to begin camera data capture.\n");
		return (FALSE);		
	}
	
	return (TRUE);
}

static int setupMarkersObjects(char *objectDataFilename)
{	
	// Load in the object data - trained markers and associated bitmap files.
    if ((gObjectData = read_VRMLdata(objectDataFilename, &gObjectDataCount)) == NULL) {
        fprintf(stderr, "setupMarkersObjects(): read_VRMLdata returned error !!\n");
        return (FALSE);
    }

    printf("Object count = %d\n", gObjectDataCount);
	
	return (TRUE);
}

// Report state of ARToolKit global variables arFittingMode,
// arImageProcMode, arglDrawMode, arTemplateMatchingMode, arMatchingPCAMode.
static void debugReportMode(void)
{
	if(arFittingMode == AR_FITTING_TO_INPUT ) {
		fprintf(stderr, "FittingMode (Z): INPUT IMAGE\n");
	} else {
		fprintf(stderr, "FittingMode (Z): COMPENSATED IMAGE\n");
	}
	
	if( arImageProcMode == AR_IMAGE_PROC_IN_FULL ) {
		fprintf(stderr, "ProcMode (X)   : FULL IMAGE\n");
	} else {
		fprintf(stderr, "ProcMode (X)   : HALF IMAGE\n");
	}
	
	if (arglDrawModeGet(gArglSettings) == AR_DRAW_BY_GL_DRAW_PIXELS) {
		fprintf(stderr, "DrawMode (C)   : GL_DRAW_PIXELS\n");
	} else if (arglTexmapModeGet(gArglSettings) == AR_DRAW_TEXTURE_FULL_IMAGE) {
		fprintf(stderr, "DrawMode (C)   : TEXTURE MAPPING (FULL RESOLUTION)\n");
	} else {
		fprintf(stderr, "DrawMode (C)   : TEXTURE MAPPING (HALF RESOLUTION)\n");
	}
		
	if( arTemplateMatchingMode == AR_TEMPLATE_MATCHING_COLOR ) {
		fprintf(stderr, "TemplateMatchingMode (M)   : Color Template\n");
	} else {
		fprintf(stderr, "TemplateMatchingMode (M)   : BW Template\n");
	}
	
	if( arMatchingPCAMode == AR_MATCHING_WITHOUT_PCA ) {
		fprintf(stderr, "MatchingPCAMode (P)   : Without PCA\n");
	} else {
		fprintf(stderr, "MatchingPCAMode (P)   : With PCA\n");
	}
}


static void Quit(void)
{
	arglCleanup(gArglSettings);
	arVideoCapStop();
	arVideoClose();
#ifdef _WIN32
	CoUninitialize();
#endif
	exit(0);
}

static void Keyboard(unsigned char key, int x, int y)
{
	int mode;
	switch (key) {
		case 0x1B:						// Quit.
		case 'Q':
		case 'q':
			Quit();
			break;
		case 'C':
		case 'c':
			mode = arglDrawModeGet(gArglSettings);
			if (mode == AR_DRAW_BY_GL_DRAW_PIXELS) {
				arglDrawModeSet(gArglSettings, AR_DRAW_BY_TEXTURE_MAPPING);
				arglTexmapModeSet(gArglSettings, AR_DRAW_TEXTURE_FULL_IMAGE);
			} else {
				mode = arglTexmapModeGet(gArglSettings);
				if (mode == AR_DRAW_TEXTURE_FULL_IMAGE)	arglTexmapModeSet(gArglSettings, AR_DRAW_TEXTURE_HALF_IMAGE);
				else arglDrawModeSet(gArglSettings, AR_DRAW_BY_GL_DRAW_PIXELS);
			}
			fprintf(stderr, "*** Camera - %f (frame/sec)\n", (double)gCallCountMarkerDetect/arUtilTimer());
			gCallCountMarkerDetect = 0;
			arUtilTimerReset();
			debugReportMode();
			break;
		case '?':
		case '/':
			printf("Keys:\n");
			printf(" q or [esc]    Quit demo.\n");
			printf(" c             Change arglDrawMode and arglTexmapMode.\n");
			printf(" ? or /        Show this help.\n");
			printf("\nAdditionally, the ARVideo library supplied the following help text:\n");
			arVideoDispOption();
			break;
		case 'a':
				//偍帋偟/////////////////////*
/*	if((fp = fopen("Wrl/aaaaa.dat", "w")) == NULL){
		printf("FILE OPEN ERROR\n");
		exit(-1);
	}
	fprintf(fp, "aaaaa.wrl\n%f 0.0 0.0\t\t# Translation\n0.0 0.0 0.0 0.0\t\t# Rotation\n1.0 1.0 1.0\t\t# Scale", x);

	fclose(fp);
*/
	/////////////////////////////
			break;
		default:
			break;
	}
}

static void Idle(void)
{
	static int ms_prev;
	int ms;
	float s_elapsed;
	ARUint8 *image;

	ARMarkerInfo    *marker_info;					// Pointer to array holding the details of detected markers.
    int             marker_num;						// Count of number of markers detected.
    int             i, j, k, l;
	
	// Find out how long since Idle() last ran.
	ms = glutGet(GLUT_ELAPSED_TIME);
	s_elapsed = (float)(ms - ms_prev) * 0.001;
	if (s_elapsed < 0.01f) return; // Don't update more often than 100 Hz.
	ms_prev = ms;
	
	// Update drawing.
	arVrmlTimerUpdate();
	
	// Grab a video frame.
	if ((image = arVideoGetImage()) != NULL) {
		gARTImage = image;	// Save the fetched image.
		gPatt_found = FALSE;	// Invalidate any previous detected markers.
		
		gCallCountMarkerDetect++; // Increment ARToolKit FPS counter.
		
		// Detect the markers in the video frame.
		if (arDetectMarker(gARTImage, gARTThreshhold, &marker_info, &marker_num) < 0) {
			exit(-1);
		}
				
		// Check for object visibility.

		for (i = 0; i < gObjectDataCount; i++) {
		
			// Check through the marker_info array for highest confidence
			// visible marker matching our object's pattern.
			//k =  -1;
			Cnt[i] = 0;  // *** Changed ***

			for (j = 0; j < marker_num; j++) {
				if (marker_info[j].id == gObjectData[i].id) {
					/*
					if( k == -1 ) k = j; // First marker detected.
					else if (marker_info[k].cf < marker_info[j].cf) k = j; // Higher confidence marker detected.
					*/

					//if( Cnt[i] == 0 ) Disp[i][0] = j, Cnt[i]++; // First marker detected.
					//else if (marker_info[Disp[i][0]].cf < marker_info[j].cf) Disp[i][0] = j; // Higher confidence marker detected.

					Disp[i][Cnt[i]++] = j; // *** Changed ***
					//printf("Coverting %d %d\n", i, j);
				}
			}
			
			if (Cnt[i] != 0)
			{
				// Get the transformation between the marker and the real camera.
				//fprintf(stderr, "Saw object %d.\n", i);
				/*
				if (gObjectData[i].visible == 0)
				{
					arGetTransMat(&marker_info[k], gObjectData[i].marker_center, gObjectData[i].marker_width, gObjectData[i].trans);
				}
				else
				{
					arGetTransMatCont(&marker_info[k], gObjectData[i].trans, gObjectData[i].marker_center, gObjectData[i].marker_width, gObjectData[i].trans);
				}
				*/
				
				for (l = 0; l < Cnt[i]; l++) Marker_store[i][l] = marker_info[Disp[i][l]];

				gObjectData[i].visible = 1;
				gPatt_found = TRUE;
			} 
			else 
			{
				gObjectData[i].visible = 0;
			}
		}
		
		// Tell GLUT to update the display.
		glutPostRedisplay();
	}
}

//
//	This function is called on events when the visibility of the
//	GLUT window changes (including when it first becomes visible).
//
static void Visibility(int visible)
{
	if (visible == GLUT_VISIBLE) {
		glutIdleFunc(Idle);
	} else {
		glutIdleFunc(NULL);
	}
}

GLuint CreateTextureFromPng(const char* filename)
{
	unsigned char header[8];    
	int k;  
	GLuint textureID;
	int width, height; 
	png_byte color_type; 
	png_byte bit_depth;
 
	png_structp png_ptr;
	png_infop info_ptr;
	int number_of_passes;
	png_bytep * row_pointers;
	int row,col,pos; 
	GLubyte *rgba;
 
	FILE *fp = fopen(filename,"rb");
	if (!fp) { fclose(fp); return 0; }
    
    fread(header, 1, 8, fp);
	if (png_sig_cmp(header,0,8)) { fclose(fp); return 0; }
   
	png_ptr=png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL); 
	if (!png_ptr) { fclose(fp); return 0; }
	
	info_ptr=png_create_info_struct(png_ptr);
 
	if(!info_ptr)
	{
		png_destroy_read_struct(&png_ptr,(png_infopp)NULL,(png_infopp)NULL);
		fclose(fp);
		return 0;
	}

    if (setjmp(png_jmpbuf(png_ptr)))
	{
		png_destroy_read_struct(&png_ptr,(png_infopp)NULL,(png_infopp)NULL);
        fclose(fp);
        return 0;
    }
	
	png_init_io(png_ptr,fp);

    png_set_sig_bytes(png_ptr, 8);
	
    png_read_info(png_ptr, info_ptr);
	
    width = png_get_image_width(png_ptr, info_ptr);
    height = png_get_image_height(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);
	
	// if (color_type == PNG_COLOR_TYPE_RGB_ALPHA)
 
        // png_set_swap_alpha(png_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	
    number_of_passes = png_set_interlace_handling(png_ptr);
	
    png_read_update_info(png_ptr, info_ptr);
 
    if (setjmp(png_jmpbuf(png_ptr))) { fclose(fp); return 0; }
    
	rgba=(GLubyte*)malloc(width * height * 4);

    row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);
 
	for (k = 0; k < height; k++) row_pointers[k] = NULL;
         
	for (k=0; k<height; k++)
		//row_pointers[k] = (png_byte*) malloc(png_get_rowbytes(png_ptr,info_ptr));
		row_pointers[k] = (png_bytep)png_malloc(png_ptr, png_get_rowbytes(png_ptr, info_ptr));

 	png_read_image(png_ptr, row_pointers);
 
    pos = (width * height * 4) - (4 * width);
    for (row = 0; row < height; row++)
    {
       for (col = 0; col < (4 * width); col += 4)
       {
		   rgba[pos++] = row_pointers[row][col];        // red
		   rgba[pos++] = row_pointers[row][col + 1];    // green
		   rgba[pos++] = row_pointers[row][col + 2];    // blue
		   rgba[pos++] = row_pointers[row][col + 3];    // alpha
	   }
	   pos = (pos - (width * 4) * 2);
    }

    glEnable(GL_TEXTURE_2D);
 
    glGenTextures(1,&textureID);

    glBindTexture(GL_TEXTURE_2D,textureID);
 
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
 
    //glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
 
	free(row_pointers); 
	free(rgba);
	fclose(fp);
    return textureID;
}

//
//	This function is called when the
//	GLUT window is resized.
//
static void Reshape(int w, int h)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, (GLsizei) w, (GLsizei) h);
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Call through to anyone else who needs to know about window sizing here.
	glOrtho(-100, 100, -100, 100, -100, 100);
}

static void Game_Over()
{
	if (Cnt[0] == 2) printf("Player 2 Win !");
	if (Cnt[1] == 2) printf("Player 1 Win !");
}

//
// This function is called when the window needs redrawing.
//

static void Display(void)
{
	int i, j, k, l;
	double num = 0.0;
    GLdouble p[16];
	GLdouble m[16];
	
	// Select correct buffer for this context.
	glDrawBuffer(GL_BACK);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the buffers for new frame.
	
	arglDispImage(gARTImage, &gARTCparam, 1.0, gArglSettings);	// zoom = 1.0.
	arVideoCapNext();
	gARTImage = NULL; // Image data is no longer valid after calling arVideoCapNext().
	glPushMatrix();

	if (gPatt_found) {
		// Projection transformation.
		arglCameraFrustumRH(&gARTCparam, VIEW_DISTANCE_MIN, VIEW_DISTANCE_MAX, p);
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixd(p);
		glMatrixMode(GL_MODELVIEW);
		
		// Viewing transformation.
		glLoadIdentity();
		// Lighting and geometry that moves with the camera should go here.
		// (I.e. must be specified before viewing transformations.)
		//none
		
		// All other lighting and geometry goes here.
		// Calculate the camera position for each object and draw it.
		for (i = 0; i < gObjectDataCount; i++) {
			if ((gObjectData[i].visible != 0) && (gObjectData[i].vrml_id >= 0)) {
				for (j = 0; j < Cnt[i]; j++)
			    {
					arGetTransMatCont(&Marker_store[i][j], gObjectData[i].trans, gObjectData[i].marker_center, gObjectData[i].marker_width, gObjectData[i].trans);

					arglCameraViewRH(gObjectData[i].trans, m, VIEW_SCALEFACTOR_4);
					glLoadMatrixd(m);

					arVrmlDraw(gObjectData[i].vrml_id);
				}
			}			
		}

		//Game_Over();
	} // gPatt_found
	// Any 2D overlays go here.
	// Display win.
	if ((Cnt[0] == 2) || (Cnt[1] == 2))
	{
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, CreateTextureFromPng("Win.png"));
		
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f); glVertex2f(-60.0f, -60.0f); 
		glTexCoord2f(1.0f, 0.0f); glVertex2f(60.0f, -60.0f); 
		glTexCoord2f(1.0f, 1.0f); glVertex2f(60.0f, 60.0f); 
		glTexCoord2f(0.0f, 1.0f); glVertex2f(-60.0f, 60.0f); 
		glEnd();
	
		glDisable(GL_TEXTURE_2D);
	}
	glutSwapBuffers();
}

int main(int argc, char** argv)
{
	int i;
	char glutGamemode[32];
	const char *cparam_name = 
		"Data/camera_para.dat";
#ifdef _WIN32
	char			*vconf = "Data\\WDM_camera_flipV.xml";
#else
	char			*vconf = "";
#endif
	char objectDataFilename[] = "Data/object_data_vrml";
	
	// ----------------------------------------------------------------------------
	// Library inits.
	//

	glutInit(&argc, argv);

	// ----------------------------------------------------------------------------
	// Hardware setup.
	//

	if (!setupCamera(cparam_name, vconf, &gARTCparam)) {
		fprintf(stderr, "main(): Unable to set up AR camera.\n");
		exit(-1);
	}
	
#ifdef _WIN32
	CoInitialize(NULL);
#endif

	// ----------------------------------------------------------------------------
	// Library setup.
	//

	// Set up GL context(s) for OpenGL to draw into.
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	if (!prefWindowed) {	
		if (prefRefresh) sprintf(glutGamemode, "%ix%i:%i@%i", prefWidth, prefHeight, prefDepth, prefRefresh);
		else sprintf(glutGamemode, "%ix%i:%i", prefWidth, prefHeight, prefDepth);
		glutGameModeString(glutGamemode);
		glutEnterGameMode();
	} else {
		glutInitWindowSize(gARTCparam.xsize, gARTCparam.ysize);
		glutCreateWindow(argv[0]);
	}

	// Setup argl library for current context.
	if ((gArglSettings = arglSetupForCurrentContext()) == NULL) {
		fprintf(stderr, "main(): arglSetupForCurrentContext() returned error.\n");
		exit(-1);
	}
	debugReportMode();
	arUtilTimerReset();

	if (!setupMarkersObjects(objectDataFilename)) {
		fprintf(stderr, "main(): Unable to set up AR objects and markers.\n");
		Quit();
	}
	// Test render all the VRML objects.
    fprintf(stdout, "Pre-rendering the VRML objects...");
	fflush(stdout);
	glEnable(GL_TEXTURE_2D);

	for (i = 0; i < gObjectDataCount; i++) {
		arVrmlDraw(gObjectData[i].vrml_id);
	}

	glDisable(GL_TEXTURE_2D);
	fprintf(stdout, " done\n");

	// Register GLUT event-handling callbacks.
	// NB: Idle() is registered by Visibility.
	glutDisplayFunc(Display);
	glutReshapeFunc(Reshape);
	glutVisibilityFunc(Visibility);
	glutKeyboardFunc(Keyboard);
	
	glutMainLoop();
	return (0);
}
