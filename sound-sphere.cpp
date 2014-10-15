//-----------------------------------------------------------------------------
// name: sound-sphere.cpp
// desc: a spherical audio visualizer
//
// author: Matt Horton (mattah@ccrma.stanford.edu)
//   date: fall 2014
//   uses: RtAudio by Gary Scavone
//-----------------------------------------------------------------------------
#include "RtAudio.h"
#include "chuck_fft.h"
#include "color.h"
#include <math.h>
#include <stdlib.h>
#include <iostream>
#include <cstring>
#include <sys/time.h>
#include <time.h>
using namespace std;

#ifdef __MACOSX_CORE__
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#endif

#ifdef __UNIX_JACK__
#include <GL/glut.h>
#endif




//-----------------------------------------------------------------------------
// function prototypes
//-----------------------------------------------------------------------------
void initGfx();
void idleFunc();
void displayFunc();
void reshapeFunc( GLsizei width, GLsizei height );
void keyboardFunc( unsigned char, int, int );
void specialFunc( int, int, int );
void mouseFunc( int button, int state, int x, int y );
void help();


// our datetype
#define SAMPLE float
// corresponding format for RtAudio
#define MY_FORMAT RTAUDIO_FLOAT32
// sample rate
#define MY_SRATE 44100
// number of channels
#define MY_CHANNELS 1
// for convenience
#define MY_PIE 3.14159265358979

// width and height
long g_width = 1024;
long g_height = 720;
long g_last_width = g_width;
long g_last_height = g_height;
// history length
int g_histSize = 255;
// refresh rate settings
long time_pre = 0;
int refresh_rate = 15000; //us
struct timeval timer;
// global buffer
SAMPLE * g_buffer = NULL;
SAMPLE * g_freq_buffer = NULL;
complex * g_cbuff = NULL;
complex ** g_cbuff_buff = NULL;
SAMPLE * g_avg_buff = NULL;
long g_bufferSize;
// flags
bool g_rotate = false;
bool g_circle = false;
bool g_sphere = false;
bool g_window_on = false;
bool g_waterfall = false;
bool g_party = false;
bool g_noBug = true;
bool g_avMax = false;
#ifdef __UNIX_JACK__
GLboolean g_fullscreen = gl_FALSE;
#else
GLboolean g_fullscreen = FALSE;
#endif
// radius for circle
float g_radius_factor = 1.0f;
float g_radius = 1.0f;
float g_radius_base = 1.0f;
// window
SAMPLE * g_window = NULL;
// history counter
int g_histCount = 0;
int g_maxCount = 0;
// max val
float g_maxVal = 0.0f;
// left/right rotation
float yrot = 3.0f;




//-----------------------------------------------------------------------------
// name: callme()
// desc: audio callback
//-----------------------------------------------------------------------------
int callme( void * outputBuffer, void * inputBuffer, unsigned int numFrames,
            double streamTime, RtAudioStreamStatus status, void * data )
{
    // cast!
    SAMPLE * input = (SAMPLE *)inputBuffer;
    SAMPLE * output = (SAMPLE *)outputBuffer;
    SAMPLE avg = 0.0f, sum = 0.0f;
    
    // fill
    for( int i = 0; i < numFrames; i++ )
    {
        // assume mono
        g_buffer[i] = input[i];
        // zero output
        output[i] = 0;
    }
    
    // apply window
    apply_window(g_buffer, g_window, (unsigned long) g_bufferSize);
    
    // copy g_buffer to g_freq_buffer
    memcpy( g_freq_buffer, g_buffer, sizeof(SAMPLE)*g_bufferSize);
    
    // fft
    rfft(g_freq_buffer, g_bufferSize/2, FFT_FORWARD);
    
    // Get the complex buffer for this round set up
    g_cbuff = (complex *) g_freq_buffer;
    
    g_maxVal = 0.0f; //reset maxVal
    
    // Store this complex buffer in the history buffer
    // Indices are modulo g_histSize. This creates a rolling
    // store of size g_histSize
    for (int j = 0; j < g_bufferSize/2; j++) {
        g_cbuff_buff[g_histCount][j].re = 0;
        g_cbuff_buff[g_histCount][j].im = 0;
        g_cbuff_buff[g_histCount][j] = g_cbuff[j];
        
        // get new maxVal
        if (cmp_abs(g_cbuff[j]) > g_maxVal) {
            g_maxVal = cmp_abs(g_cbuff[j]);
            g_avg_buff[g_histCount] = g_maxVal;
        }
    }
    
    g_histCount = (g_histCount + 1) % g_histSize;
    if ( g_histCount > g_maxCount ) g_maxCount = g_histCount;
    
    return 0;
}

//-----------------------------------------------------------------------------
// Name: drawCircle( )
// Desc: draws a circle
//-----------------------------------------------------------------------------
void drawCircle(complex * cbuff) {
    float radius, angle, x, y, xrot = 0.0f, zrot = 0.0f;
    
    glBegin(GL_LINE_LOOP);
    
    for(int i =0; i < (g_bufferSize/2); i++){
        angle = 2 * M_PI * i / (g_bufferSize/2);
        radius = g_radius_factor * g_radius + g_radius_base;
        
        if (cmp_abs(cbuff[i]) <= 1) {
            x = (10*pow(cmp_abs(cbuff[i]), .5)+radius)*cos(angle);
            y = (10*pow(cmp_abs(cbuff[i]), .5)+radius)*sin(angle);
        } else {
            x = radius*cos(angle);
            y = radius*sin(angle);
        }
        
        glVertex2f(x,y);
    }

    glEnd();
}

//-----------------------------------------------------------------------------
// Name: drawSquare( )
// Desc: draws a square
//-----------------------------------------------------------------------------
void drawSquare() {
    glBegin(GL_QUADS); // Start drawing a quad primitive
    glVertex3f(-1.0f, -1.0f, -1.0f); // The bottom left corner
    glVertex3f(-1.0f, 1.0f, -1.0f); // The top left corner
    glVertex3f(1.0f, 1.0f, -0.2f); // The top right corner
    glVertex3f(1.0f, -1.0f, -0.2f); // The bottom right corner
    glEnd();
}

//-----------------------------------------------------------------------------
// Name: drawWindow( )
// Desc: draws a square
//-----------------------------------------------------------------------------
void drawWindow() {
    // step through and plot the waveform
    GLfloat x = -5;
    
    // increment
    GLfloat xinc = ::fabs(2*x / (g_bufferSize));

    
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < g_bufferSize; i++) {
        glVertex2f(x, g_window[i]);
        x += xinc;
    }
    glEnd();
}

//-----------------------------------------------------------------------------
// Name: help( )
// Desc: print usage
//-----------------------------------------------------------------------------
void help()
{
    cerr << "----------------------------------------------------" << endl;
    cerr << "sound-sphere (v1.0)" << endl;
    cerr << "Matt Horton" << endl;
    cerr << "http://ccrma.stanford.edu/~mattah/256a/sound-sphere/" << endl;
    cerr << "----------------------------------------------------" << endl;
    cerr << " All modifier keys can be used in their capital form" << endl;
    cerr << endl;
    cerr << "'h' - print this help message" << endl;
    cerr << "'m' - toggle fullscreen" << endl;
    cerr << "'q' - quit visualization" << endl;
    cerr << "'c' - show/hide circular signal spectrum (will hide sphere if shown)" << endl;
    cerr << "'s' - show/hide spherical signal spectrum" << endl;
    cerr << "'f' - toggle drawing of historical spectra" << endl;
    cerr << "'w' - show/hide time-domain window visualization" << endl;
    cerr << "'p' - toggle party mode" << endl;
    cerr << "'a' - toggle max averaging in party mode. Makes color change more smoothly." << endl;
    cerr << "'b' - toggle buggy...er...awesome mode" << endl;
    cerr << "'r' - toggle rotation" << endl;
    cerr << endl;
    cerr << "radius controls:" << endl;
    cerr << "Press or hold the up and down keys to increase or " << endl;
    cerr << "decrease (respectively) the radius of the sphere or circle." << endl;
    cerr << endl;
    cerr << "rotation controls:" << endl;
    cerr << "Press or hold the left and right keys to rotate about the y axis." << endl;
    cerr << "----------------------------------------------------" << endl;
}



//-----------------------------------------------------------------------------
// name: main()
// desc: entry point
//-----------------------------------------------------------------------------
int main( int argc, char ** argv )
{
    // instantiate RtAudio object
    RtAudio audio;
    // variables
    unsigned int bufferBytes = 0;
    // frame size
    unsigned int bufferFrames = 512;
    
    // check for audio devices
    if( audio.getDeviceCount() < 1 )
    {
        // nopes
        cout << "no audio devices found!" << endl;
        exit( 1 );
    }

    // initialize GLUT
    glutInit( &argc, argv );
    // init gfx
    initGfx();

    // let RtAudio print messages to stderr.
    audio.showWarnings( true );

    // set input and output parameters
    RtAudio::StreamParameters iParams, oParams;
    iParams.deviceId = audio.getDefaultInputDevice();
    iParams.nChannels = MY_CHANNELS;
    iParams.firstChannel = 0;
    oParams.deviceId = audio.getDefaultOutputDevice();
    oParams.nChannels = MY_CHANNELS;
    oParams.firstChannel = 0;
    
    // create stream options
    RtAudio::StreamOptions options;

    // go for it
    try {
        // open a stream
        audio.openStream( &oParams, &iParams, MY_FORMAT, MY_SRATE, &bufferFrames, &callme, (void *)&bufferBytes, &options );
    }
    catch( RtError& e )
    {
        // error!
        cout << e.getMessage() << endl;
        exit( 1 );
    }

    // compute
    bufferBytes = bufferFrames * MY_CHANNELS * sizeof(SAMPLE);
    // allocate global buffer
    g_bufferSize = bufferFrames;
    g_buffer = new SAMPLE[g_bufferSize];
    g_freq_buffer = new SAMPLE[g_bufferSize];
    memset( g_buffer, 0, sizeof(SAMPLE)*g_bufferSize );
    memset(g_freq_buffer, 0, sizeof(SAMPLE)*g_bufferSize);
    g_window = new SAMPLE[g_bufferSize];
    g_avg_buff = new SAMPLE[g_histSize];
    
    g_cbuff = new complex [g_bufferSize/2];
    for (int i = 0; i < (g_bufferSize/2); i++) {
        g_cbuff[i].re = 0.0f;
        g_cbuff[i].im = 0.0f;
    }
    
    g_cbuff_buff = new complex *[g_histSize];
    for (int i = 0; i < g_histSize; i++){
        g_cbuff_buff[i] = new complex [g_bufferSize/2];
        for (int j= 0; j < (g_bufferSize/2); j++) {
            g_cbuff_buff[i][j].re = 0;
            g_cbuff_buff[i][j].re = 0;
        }
    }
    

    
    hanning(g_window, (unsigned long)g_bufferSize);
    
    // print help
    help();
    
    // go for it
    try {
        // start stream
        audio.startStream();

        // let GLUT handle the current thread from here
        glutMainLoop();
        
        // stop the stream.
        audio.stopStream();
    }
    catch( RtError& e )
    {
        // print error message
        cout << e.getMessage() << endl;
        goto cleanup;
    }
    
cleanup:
    // close if open
    if( audio.isStreamOpen() )
        audio.closeStream();
    
    delete g_buffer, g_cbuff, g_window, g_freq_buffer, g_cbuff_buff;
    
    // done
    return 0;
}




//-----------------------------------------------------------------------------
// Name: reshapeFunc( )
// Desc: called when window size changes
//-----------------------------------------------------------------------------
void initGfx()
{
    // double buffer, use rgb color, enable depth buffer
    glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH );
    // initialize the window size
    glutInitWindowSize( g_width, g_height );
    // set the window postion
    glutInitWindowPosition( 100, 100 );
    // create the window
    glutCreateWindow( "sound-sphere" );
    
    // set the idle function - called when idle
    glutIdleFunc( idleFunc );
    // set the display function - called when redrawing
    glutDisplayFunc( displayFunc );
    // set the reshape function - called when client area changes
    glutReshapeFunc( reshapeFunc );
    // set the keyboard function - called on keyboard events
    glutKeyboardFunc( keyboardFunc );
    // set the special keyboard function - called on special keyboard events
    glutSpecialFunc( specialFunc );
    // set the mouse function - called on mouse stuff
    glutMouseFunc( mouseFunc );
    
    // set clear color
    glClearColor( 0, 0, 0, 1 );
    // enable color material
    glEnable( GL_COLOR_MATERIAL );
    // enable depth test
    glEnable( GL_DEPTH_TEST );
}




//-----------------------------------------------------------------------------
// Name: reshapeFunc( )
// Desc: called when window size changes
//-----------------------------------------------------------------------------
void reshapeFunc( GLsizei w, GLsizei h )
{
    // save the new window size
    g_width = w; g_height = h;
    // map the view port to the client area
    glViewport( 0, 0, w, h );
    // set the matrix mode to project
    glMatrixMode( GL_PROJECTION );
    // load the identity matrix
    glLoadIdentity( );
    // create the viewing frustum
    gluPerspective( 45.0, (GLfloat) w / (GLfloat) h, 1.0, 300.0 );
    // set the matrix mode to modelview
    glMatrixMode( GL_MODELVIEW );
    // load the identity matrix
    glLoadIdentity( );
    // position the view point
    gluLookAt( 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f );
}




//-----------------------------------------------------------------------------
// Name: keyboardFunc( )
// Desc: key event
//-----------------------------------------------------------------------------
void keyboardFunc( unsigned char key, int x, int y )
{
    switch( key )
    {
        case 'A':
        case 'a':
            g_avMax = !g_avMax;
            break;
        case 'Q':
        case 'q':
            exit(1);
            break;
        case 'C':
        case 'c':
            g_circle = !g_circle;
            g_sphere = false;
            break;
        case 'S':
        case 's':
            g_sphere = !g_sphere;
            if (!g_circle) g_circle = !g_circle;
            break;
        case 'R':
        case 'r':
            g_rotate = !g_rotate;
            glRotatef(0, 0, 1, 0);
            break;
        case 'W':
        case 'w':
            g_window_on = !g_window_on;
            break;
        case 'F':
        case 'f':
            g_waterfall = !g_waterfall;
            break;
        case 'P':
        case 'p':
            g_party = !g_party;
            break;
        case 'H':
        case 'h':
            help();
            break;
        case 'B':
        case 'b':
            g_noBug = !g_noBug;
            break;
        case 'M':
        case 'm': // toggle fullscreen
        {
            // check fullscreen
            if( !g_fullscreen )
            {
                g_last_width = g_width;
                g_last_height = g_height;
                glutFullScreen();
            }
            else
                glutReshapeWindow( g_last_width, g_last_height );
            
            // toggle variable value
            g_fullscreen = !g_fullscreen;
        }
    }
    
    glutPostRedisplay( );
}

//-----------------------------------------------------------------------------
// Name: specialFunc( )
// Desc: special key event
//-----------------------------------------------------------------------------
void specialFunc(int key, int x, int y) {
    if (key == GLUT_KEY_UP) {
        if (g_radius_base < 2.5f) {
            g_radius_base += .05f;
        }
    } else if (key == GLUT_KEY_DOWN) {
        if (g_radius_base > 0.0f) {
            g_radius_base -= .05f;
        }
    } else if (key == GLUT_KEY_RIGHT) {
        //yrot += .1;
        glRotatef( yrot, 0, 1, 0 );
    } else if (key == GLUT_KEY_LEFT) {
        //yrot -= .1;
        glRotatef( yrot, 0, -1, 0);
    }
}



//-----------------------------------------------------------------------------
// Name: mouseFunc( )
// Desc: handles mouse stuff
//-----------------------------------------------------------------------------
void mouseFunc( int button, int state, int x, int y )
{
    if( button == GLUT_LEFT_BUTTON )
    {
        // when left mouse button is down
        if( state == GLUT_DOWN )
        {
        }
        else
        {
        }
    }
    else if ( button == GLUT_RIGHT_BUTTON )
    {
        // when right mouse button down
        if( state == GLUT_DOWN )
        {
        }
        else
        {
        }
    }
    else
    {
    }
    
    glutPostRedisplay( );
}




//-----------------------------------------------------------------------------
// Name: idleFunc( )
// Desc: callback from GLUT
//-----------------------------------------------------------------------------
void idleFunc( )
{
    // render the scene
    glutPostRedisplay( );
}



//-----------------------------------------------------------------------------
// Name: displayFunc( )
// Desc: callback function invoked to draw the client area
//-----------------------------------------------------------------------------
void displayFunc( )
{
    // local state
    static GLfloat zrot = 0.0f, c = 0.0f, xrot = 0.0f, breathe = 0.0f, breathe_angle = 0.0f, circ_rot = 0.0f, avg_max = 0.0f;
    complex * cbuff = new complex[g_bufferSize/2];
    SAMPLE * avg_buff;
    
    
    // enforce refresh rate
    long time_diff = 0;
    if (time_pre > 0){
        gettimeofday(&timer, NULL);
        long time_post = (long)(timer.tv_sec*1000000+timer.tv_usec);
        time_diff =  time_post - time_pre;
        
        if(time_diff < refresh_rate)usleep(refresh_rate - time_diff);
        
        gettimeofday(&timer, NULL);
        time_post = (long)(timer.tv_sec*1000000+timer.tv_usec);
        time_diff =  time_post - time_pre;
    }
    gettimeofday(&timer, NULL);
    time_pre = (long)(timer.tv_sec*1000000+timer.tv_usec);
    
    // clear the color and depth buffers
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    
    // line width
    glLineWidth( 2.0 );
    // step through and plot the waveform
    GLfloat x = -5;
    // increment
    GLfloat xinc = ::fabs(2*x / (g_bufferSize));
    
    // push the matrix
    glPushMatrix();
    
    // rotation
    if (g_rotate) {
        glRotatef( zrot, 0, 0, 1 );
        zrot += .1;
    } else {
        zrot = 0.0f;
    }

    // set the color
    // glColor3f( (sin(c)+1)/2, (sin(c*2)+1)/2, (sin(c+.5)+1)/2 );
    glColor3f( 1, 1, 1 );
    
    if (g_window_on) drawWindow();

    // go
    glBegin( GL_LINE_STRIP );
    // loop through global buffer
    for( int i = 0; i < g_bufferSize; i++ )
    {
        // set the next vertex
        glVertex2f( x, 5*g_buffer[i] );
        // increment x
        x += xinc;
    }
    // done
    glEnd();
    
    
    if (g_party) {
        Color color = {};
        // Average the max values over the history of max values
        if (g_avMax) {
            for (int i = 0; i < g_histSize; i++) {
                avg_max += g_avg_buff[i];
            }
            avg_max = avg_max / g_histSize;
            color = colorSpectrum((double)(avg_max*100.0));
        } else { // Use only the current max value
            color = colorSpectrum((double)(g_maxVal*100.0));
        }
    
        glColor3f(color.R, color.G, color.B);
    } else {
        glColor3f( (sin(c)+1)/2, (sin(c*2)+1)/2, (sin(c+.5)+1)/2 );
    }
    
    // Set up breathing circle
    breathe_angle = 2 * M_PI * breathe / (g_bufferSize/2);
    g_radius = .5*sin(breathe_angle)+1;
    
    
    // rotation
    if (g_rotate) {
        glRotatef( xrot, -1, 0, 0 );
        xrot += .0246;
        circ_rot += .5;
    } else {
        xrot = 0.0f;
        circ_rot = 0.0f;
    }
    
    memcpy( cbuff, g_cbuff, sizeof(complex)*(g_bufferSize/2));
    
    if (g_sphere && g_circle) {
        if (g_waterfall) {
            for (int spectrum = 0; spectrum < g_maxCount; spectrum++){
                
                glRotatef( circ_rot, 1, 0, 0 );
                circ_rot += 0.0123;
                drawCircle(g_cbuff_buff[spectrum]);
            }
        } else {
            for (int i = 0; i < 128; i++) {
                glRotatef( circ_rot, 1, 0, 0 );
                circ_rot += 0.049; // 2*pi/128
                if(g_noBug) {
                    drawCircle(cbuff);
                } else {
                    drawCircle(g_cbuff);
                }
            }
            for (int i = 0; i < (g_bufferSize/2); i++) {
                g_cbuff[i].re = 0.0f;
                g_cbuff[i].im = 0.0f;
            }
        }
    } else if (g_circle) {
        glRotatef( circ_rot, 1, 0, 0 );
        drawCircle(cbuff);
    }
    
    // pop
    glPopMatrix();
    
    // increment color
    c += .01;
    
    // increment breathing counter
    breathe += .5;
    
    // flush!
    glFlush( );
    // swap the double buffer
    glutSwapBuffers( );
}
