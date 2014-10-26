/*
 * Program description: Convolves an image by a specified kernal.
 *
 * Author: Austin Brennan - awbrenn@g.clemson.edu
 *
 * Date: October 13, 2014
 *
 * Other information: Check README for more information
 *
 */

#include <cstdlib>
#include <iostream>
#include <vector>
#include <cmath>
#include <float.h>
#include <OpenImageIO/imageio.h>

#ifdef __APPLE__
#  include <GLUT/glut.h>
#else
#  include <GL/glut.h>
#endif

using namespace std;
OIIO_NAMESPACE_USING

// Struct Declaration
struct pixel {
    float r, g, b, a;
};

// Global Variables
float GAMMA;
float ** LD_map; // display luminance map
float ** LW_map; // world luminance map
float ** LW_map_low_pass; // world luminace with applied filter
float ** LW_map_high_pass; // subtract LW low pass from log(LW)
float LW_low_pass_min_value;
float LW_low_pass_max_value;
int IMAGE_HEIGHT;
int IMAGE_WIDTH;
pixel **PIXMAP;
pixel **COPY_PIXMAP;
char *OUTPUT_FILE = NULL;
int FILTER_SIZE = 15;
float **FILTER;
string FLAG_OPTIONS[2] = {"-g", "-c"};


// Image Class
class Image {
public:
    int height;
    int width;
    pixel ** pixmap;

    Image(int, int);
};

// Constructore with pixmap size
Image::Image(int x, int y) {
    width = x;
    height = y;
    pixmap = new pixel*[height];
    pixmap[0] = new pixel[width * height];

    for (int i = 1; i < height; i++)
        pixmap[i] = pixmap[i - 1] + width;
}

/* Handles errors
 * input	- message is printed to stdout, if kill is true end program
 * output	- None
 * side effect - prints error message and kills program if kill flag is set
 */
void handleError (string message, bool kill) {
    cout << "ERROR: " << message << "\n";
    if (kill)
        exit(0);
}

void initializeLuminanceMap(float ** &luminance_map) {
    luminance_map = new float*[IMAGE_HEIGHT];
    luminance_map[0] = new float[IMAGE_WIDTH * IMAGE_HEIGHT];


    for (int i = 1; i < IMAGE_HEIGHT; i++)
        luminance_map[i] = luminance_map[i - 1] + IMAGE_WIDTH;
}

/* Converts pixels from vector to pixel pointers
 * input		- vector of pixels, number of channels
 * output		- None
 * side effect	- saves pixel data from vector to PIXMAP
 */
Image convertVectorToImage (vector<unsigned char> vector_pixels, int channels) {
    Image image(IMAGE_WIDTH, IMAGE_HEIGHT);

    int i = 0;
    if (channels == 3) {
        for (int row = image.height-1; row >= 0; row--)
            for (int col = 0; col < image.width; col++) {
                image.pixmap[row][col].r = (float) vector_pixels[i++] / 255.0;
                image.pixmap[row][col].g = (float) vector_pixels[i++] / 255.0;
                image.pixmap[row][col].b = (float) vector_pixels[i++] / 255.0;
                image.pixmap[row][col].a = 1.0;
            }
    }
    else if (channels == 4) {
        for (int row = image.height-1; row >= 0; row--)
            for (int col = 0; col < image.width; col++) {
                image.pixmap[row][col].r = (float) vector_pixels[i++] / 255.0;
                image.pixmap[row][col].g = (float) vector_pixels[i++] / 255.0;
                image.pixmap[row][col].b = (float) vector_pixels[i++] / 255.0;
                image.pixmap[row][col].a = (float) vector_pixels[i++] / 255.0;
            }
    }
    else
        handleError ("Could not convert image.. sorry", 1);

    return image;
}



/* Flips image verticaly
 *
 */
pixel ** flipImageVertical(pixel **pixmap_vertical_flip) {
    for (int row = IMAGE_HEIGHT-1; row >= 0; row--)
        for (int col = 0; col < IMAGE_WIDTH; col++) {
            pixmap_vertical_flip[(IMAGE_HEIGHT-1)-row][col] = PIXMAP[row][col];
        }

    return pixmap_vertical_flip;
}

/* Reads image specified in argv[1]
 * input		- the input file name
 * output		- None
 */
Image readImage (string filename) {
    ImageInput *in = ImageInput::open(filename);
    if (!in)
        handleError("Could not open input file", true);
    const ImageSpec &spec = in->spec();
    IMAGE_WIDTH = spec.width;
    IMAGE_HEIGHT = spec.height;
    int channels = spec.nchannels;
    if(channels < 3 || channels > 4)
        handleError("Application supports 3 or 4 channel images only", 1);
    vector<unsigned char> pixels (IMAGE_WIDTH*IMAGE_HEIGHT*channels);
    in->read_image (TypeDesc::UINT8, &pixels[0]);
    in->close ();
    delete in;


    return convertVectorToImage(pixels, channels);
}

/* Write image to specified file
 * input		- pixel array, width of display window, height of display window
 * output		- None
 * side effect	- writes image to a file
 */
void writeImage(unsigned char *glut_display_map, int window_width, int window_height) {
    const char *filename = OUTPUT_FILE;
    const int xres = window_width, yres = window_height;
    const int channels = 4; // RGBA
    int scanlinesize = xres * channels;
    ImageOutput *out = ImageOutput::create (filename);
    if (! out) {
        handleError("Could not create output file", false);
        return;
    }
    ImageSpec spec (xres, yres, channels, TypeDesc::UINT8);
    out->open (filename, spec);
    out->write_image (TypeDesc::UINT8, glut_display_map+(window_height-1)*scanlinesize, AutoStride, -scanlinesize, AutoStride);
    out->close ();
    delete out;
    cout << "SUCCESS: Image successfully written to " << OUTPUT_FILE << "\n";
}

/* Draw Image to opengl display
 * input		- None
 * output		- None
 * side effect	- draws image to opengl display window
 */
void drawImage() {
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glRasterPos2i(0,0);
    glDrawPixels(IMAGE_WIDTH, IMAGE_HEIGHT, GL_RGBA, GL_FLOAT, PIXMAP[0]);
    glFlush();
}

void initializeFilterMap(float ** &filter_map) {
    filter_map = new float*[FILTER_SIZE];
    filter_map[0] = new float[FILTER_SIZE*FILTER_SIZE];

    for (int i = 1; i < FILTER_SIZE; i++)
        filter_map[i] = filter_map[i - 1] + FILTER_SIZE;
}


/*  Adds together the values of a filter
    Used for calculating the pixel value of the center of a kernal
 */
float sumFilterMapValues(float ** &filter) {
    float sum_of_filter_values = 0.0;

    for (int row = 0; row < FILTER_SIZE; row++)
        for (int col = 0; col < FILTER_SIZE; col++) {
            sum_of_filter_values += filter[row][col];
        }

    return sum_of_filter_values;
}


// Calculates the kernal values on a specified channel
void calculateFilterMap(float ** &filter_map, int filter_range, float **LW_map, int LW_map_row, int LW_map_col) {
    int offset_LW_map_row, offset_LW_map_col;

    for (int filt_row = 0; filt_row < FILTER_SIZE; filt_row++) {
        offset_LW_map_row = LW_map_row + filt_row - filter_range;
        for (int filt_col = 0; filt_col < FILTER_SIZE; filt_col++) {
            offset_LW_map_col = LW_map_col + filt_col - filter_range;

            if (offset_LW_map_row < 0 or offset_LW_map_row >= IMAGE_HEIGHT or offset_LW_map_col < 0 or offset_LW_map_col >= IMAGE_WIDTH)
                filter_map[filt_row][filt_col] = 0.0;
            else
                filter_map[filt_row][filt_col] = LW_map[offset_LW_map_row][offset_LW_map_col] * FILTER[filt_row][filt_col];
        }
    }
}


// convolves a pixmap with FILTER
void convolveWorldLuminance() {

    int filter_range;
    float **filter_map;

    filter_range = FILTER_SIZE / 2;

    initializeFilterMap(filter_map);

    for (int row = 0; row < IMAGE_HEIGHT; row++)
        for (int col = 0; col < IMAGE_WIDTH; col++) {
            calculateFilterMap(filter_map, filter_range, LW_map_low_pass, row, col);

            LW_map_low_pass[row][col] = sumFilterMapValues(filter_map);
        }
}

void initializePixmap(pixel ** &pixmap) {
    pixmap = new pixel*[IMAGE_HEIGHT];
    pixmap[0] = new pixel[IMAGE_WIDTH * IMAGE_HEIGHT];

    for (int i = 1; i < IMAGE_HEIGHT; i++)
        pixmap[i] = pixmap[i - 1] + IMAGE_WIDTH;
}


// Restores the image to it's original
void restoreOriginalImage() {
    pixel ** temp_pixmap;

    initializePixmap(temp_pixmap);

    // copy current pixmap
    for (int row = 0; row < IMAGE_HEIGHT; row++)
        for (int col = 0; col < IMAGE_WIDTH; col++) {
            temp_pixmap[row][col] = PIXMAP[row][col];
        }

    for (int row = 0; row < IMAGE_HEIGHT; row++)
        for (int col = 0; col < IMAGE_WIDTH; col++) {
            PIXMAP[row][col] = COPY_PIXMAP[row][col];
        }

    for (int row = 0; row < IMAGE_HEIGHT; row++)
        for (int col = 0; col < IMAGE_WIDTH; col++) {
            COPY_PIXMAP[row][col] = temp_pixmap[row][col];
        }

    drawImage();
    delete temp_pixmap;
}


/* Key press handler
 * input	- Handled by opengl, because this is a callback function.
 * output	- None
 */
void handleKey(unsigned char key, int x, int y) {

    if (key == 'w') {
        if(OUTPUT_FILE != NULL) {
            int window_width = glutGet(GLUT_WINDOW_WIDTH), window_height = glutGet(GLUT_WINDOW_HEIGHT);
            unsigned char glut_display_map[window_width*window_height*4];
            glReadPixels(0,0, window_width, window_height, GL_RGBA, GL_UNSIGNED_BYTE, glut_display_map);
            writeImage(glut_display_map, window_width, window_height);
        }
        else
            handleError("Cannot write to file. Specify filename in third argument to write to file.", false);
    }
    else if (key == 'q' || key == 'Q') {
        cout << "\nProgram Terminated." << endl;
        exit(0);
    }
    else if (key == 's' || key == 'S') {
        restoreOriginalImage();
    }
}


/* Initialize opengl
 * input	- command line arguments
 * output	- none
 */
void openGlInit(int argc, char* argv[]) {
    // start up the glut utilities
    glutInit(&argc, argv);

    // create the graphics window, giving width, height, and title text
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGBA);
    glutInitWindowSize(IMAGE_WIDTH, IMAGE_HEIGHT);
    glutCreateWindow("Tonemap Result");

    // set up the callback routines to be called when glutMainLoop() detects
    // an event
    glutDisplayFunc(drawImage);		  		// display callback
    glutKeyboardFunc(handleKey);	  		// keyboard callback
    // glutMouseFunc(handleClick);		// click callback

    // define the drawing coordinate system on the viewport
    // lower left is (0, 0), upper right is (WIDTH, HEIGHT)
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, IMAGE_WIDTH, 0, IMAGE_HEIGHT);

    // specify window clear (background) color to be opaque white
    glClearColor(1, 1, 1, 0);

    // Routine that loops forever looking for events. It calls the registered
    // callback routine to handle each event that is detected
    glutMainLoop();
}


/*  Finds the maximum of two floating point numbers.
    If the numbers are equal maximum returns a.
 */
float maximum(float a, float b) {
    if (a < b)
        return b;
    else if (a > b)
        return a;

    return a;
}


/*  Calculates a scale factor.
    Divides the filter by the calculated scale factor.
 */
void normalizeFilter() {
    float scale_factor, filter_value, sum_of_negative_values = 0, sum_of_positive_values = 0;

    for (int row = 0; row < FILTER_SIZE; row++)
        for (int col = 0; col < FILTER_SIZE; col++) {
            filter_value = FILTER[row][col];
            if (filter_value < 0)
                sum_of_negative_values += -1.0 * filter_value; // multiply by -1.0 for absolute value
            else if (filter_value > 0)
                sum_of_positive_values += filter_value;
        }
    scale_factor = maximum(sum_of_positive_values, sum_of_negative_values);
    if (scale_factor != 0)
        scale_factor = 1.0 / scale_factor;
    else
        scale_factor = 1.0;

    for (int row = 0; row < FILTER_SIZE; row++)
        for (int col = 0; col < FILTER_SIZE; col++) {
            FILTER[row][col] = scale_factor * FILTER[row][col];
        }
}


// Flips a kernal both horizontally and vertically
void flipFilterXandY() {
    float **temp_filter;

    initializeFilterMap(temp_filter);

    for (int row = 0; row < FILTER_SIZE; row++)
        for (int col = 0; col < FILTER_SIZE; col++) {
            temp_filter[row][col] = FILTER[(FILTER_SIZE-1)-row][(FILTER_SIZE-1)-col];
        }

    for (int row = 0; row < FILTER_SIZE; row++)
        for (int col = 0; col < FILTER_SIZE; col++) {
            FILTER[row][col] = temp_filter[row][col];
        }

    delete temp_filter;
}

void getWorldLuminance(pixel ** &pixmap) {
    pixel pixel;

    for (int row = 0; row < IMAGE_HEIGHT; row++)
        for (int col = 0; col < IMAGE_WIDTH; col++) {
            pixel = pixmap[row][col];
            LW_map[row][col] = (1.0/61.0)*(20.0*pixel.r + 40.0*pixel.g + pixel.b);
        }
}

float logWithCorrectionForLogOfZero(float value) {
    if (value <= 0) {
        value = FLT_MIN;
    }
    return log(value);
}

void getDisplayLuminance() {
    for (int row = 0; row < IMAGE_HEIGHT; row++)
        for (int col = 0; col < IMAGE_WIDTH; col++) {
            LD_map[row][col] = exp(GAMMA* logWithCorrectionForLogOfZero(LW_map[row][col]));
        }
}

void getDisplayLuminanceWithConvolutionOnLW() {
    for (int row = 0; row < IMAGE_HEIGHT; row++)
        for (int col = 0; col < IMAGE_WIDTH; col++) {
            LW_map_low_pass[row][col] = logWithCorrectionForLogOfZero(LW_map[row][col]);
        }
    convolveWorldLuminance();

    for (int row = 0; row < IMAGE_HEIGHT; row++)
        for (int col = 0; col < IMAGE_WIDTH; col++) {
            LW_map_high_pass[row][col] = logWithCorrectionForLogOfZero(LW_map[row][col]) - LW_map_low_pass[row][col];
        }

    for (int row = 0; row < IMAGE_HEIGHT; row++)
        for (int col = 0; col < IMAGE_WIDTH; col++) {
            LD_map[row][col] = exp(GAMMA * LW_map_low_pass[row][col] + LW_map_high_pass[row][col]);
        }
}

float LDOverLW(float LD, float LW) {
    if (LW <= 0) {
        LW = FLT_MIN;
    }
    return LD / LW;
}

void calculateGammaCorrectedImage(pixel ** &image_pixmap) {
    getWorldLuminance(image_pixmap);
    getDisplayLuminance();

    for (int row = 0; row < IMAGE_HEIGHT; row++)
        for (int col = 0; col < IMAGE_WIDTH; col++) {
            image_pixmap[row][col].r = LDOverLW(LD_map[row][col],  LW_map[row][col]) * image_pixmap[row][col].r;
            image_pixmap[row][col].g = LDOverLW(LD_map[row][col],  LW_map[row][col]) * image_pixmap[row][col].g;
            image_pixmap[row][col].b = LDOverLW(LD_map[row][col],  LW_map[row][col]) * image_pixmap[row][col].b;
        }

}

void calculateGammaCorrecedImageWithConvolution(pixel ** &image_pixmap) {
    getWorldLuminance(image_pixmap);
    getDisplayLuminanceWithConvolutionOnLW();

    for (int row = 0; row < IMAGE_HEIGHT; row++)
        for (int col = 0; col < IMAGE_WIDTH; col++) {
            image_pixmap[row][col].r = LDOverLW(LD_map[row][col],  LW_map[row][col]) * image_pixmap[row][col].r;
            image_pixmap[row][col].g = LDOverLW(LD_map[row][col],  LW_map[row][col]) * image_pixmap[row][col].g;
            image_pixmap[row][col].b = LDOverLW(LD_map[row][col],  LW_map[row][col]) * image_pixmap[row][col].b;
        }
}

int main(int argc, char *argv[]) {
    if (argc != 4 and argc != 5)
        handleError("Proper use:\n$> tonemap ['-c' or '-g' need to use one] gamma_value input.img\n"
                "$> tonemap ['-c' or '-g' need to use one] gamma_value input.img output.img\n"
                "supported input image formats: .exr .hdr\n"
                "supported output image formats: .exr .hdr .png. jpg .tiff and possibly more", 1);
    if (argc == 5) // specified output file
        OUTPUT_FILE = argv[4];

    initializeFilterMap(FILTER);

    // set all filter values to 1
    for (int row = 0; row < FILTER_SIZE; row++)
        for (int col = 0; col < FILTER_SIZE; col++) {
            FILTER[row][col] = 1;
        }

    normalizeFilter();
    flipFilterXandY();

    bool with_convolution;

    if (FLAG_OPTIONS[0].compare(argv[1]))
        with_convolution = true;
    else if (FLAG_OPTIONS[1].compare(argv[1]))
        with_convolution = false;
    else
        handleError("\nInvalid flag options\nValid flag options are: [-g] for simple tonemap and [-c]"
                "which convovles the log-space luminance into a blurred channel and sharpened channel S,"
                " and recomposes them as gamma * B + S\n", 1);

    GAMMA = atof(argv[2]);

    Image image = readImage(argv[3]);

    initializePixmap(COPY_PIXMAP);
    for (int row = 0; row < IMAGE_HEIGHT; row++)
        for (int col = 0; col < IMAGE_WIDTH; col++) {
            COPY_PIXMAP[row][col] = image.pixmap[row][col];
        }

    if (with_convolution) {
        initializeLuminanceMap(LW_map);
        initializeLuminanceMap(LD_map);
        initializeLuminanceMap(LW_map_low_pass);
        initializeLuminanceMap(LW_map_high_pass);
        calculateGammaCorrecedImageWithConvolution(image.pixmap);
        delete LD_map;
        delete LW_map;
        delete LW_map_low_pass;
        delete LW_map_high_pass;
    }
    else  {
        initializeLuminanceMap(LW_map);
        initializeLuminanceMap(LD_map);
        calculateGammaCorrectedImage(image.pixmap);
        delete LD_map;
        delete LW_map;
    }

    PIXMAP = image.pixmap;



    openGlInit(argc, argv);
}