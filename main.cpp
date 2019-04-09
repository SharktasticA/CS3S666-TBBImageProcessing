#include <iostream>
#include <vector>
//Thread building blocks library
#include <tbb/task_scheduler_init.h>
//Free Image library
#include <FreeImagePlus.h>

using namespace std;
using namespace tbb;

int main()
{
    int nt = task_scheduler_init::default_num_threads();
    task_scheduler_init T(nt);

    //Part 1 (Greyscale Gaussian blur): -----------DO NOT REMOVE THIS COMMENT----------------------------//




    //Part 2 (Colour image processing): -----------DO NOT REMOVE THIS COMMENT----------------------------//

    // Setup Input image array
    fipImage inputImage;
    inputImage.load("../Images/render_1.png");

    unsigned int width = inputImage.getWidth();
    unsigned int height = inputImage.getHeight();

    // Setup Output image array
    fipImage outputImage;
    outputImage = fipImage(FIT_BITMAP, width, height, 24);

    //2D Vector to hold the RGB colour data of an image
    vector<vector<RGBQUAD>> rgbValues;
    rgbValues.resize(height, vector<RGBQUAD>(width));


    RGBQUAD rgb;  //FreeImage structure to hold RGB values of a single pixel

    //Extract colour data from image and store it as individual RGBQUAD elements for every pixel
    for(int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            inputImage.getPixelColor(x, y, &rgb); //Extract pixel(x,y) colour data and place it in rgb

            rgbValues[y][x].rgbRed = rgb.rgbRed;
            rgbValues[y][x].rgbGreen = rgb.rgbGreen;
            rgbValues[y][x].rgbBlue = rgb.rgbBlue;
        }
    }

    //Place the pixel colour values into output image
    for(int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            outputImage.setPixelColor(x, y, &rgbValues[y][x]);
        }
    }

    //Save the processed image
    outputImage.save("RGB_processed.png");

    return 0;
}