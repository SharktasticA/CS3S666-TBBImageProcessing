#include <iostream>
#include <vector>
#include <chrono>
#include <math.h>
//Thread building blocks library
#include <tbb/task_scheduler_init.h>
#include <tbb/tick_count.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range2d.h>
//Free Image library
#include <FreeImagePlus.h>

using namespace std;
using namespace tbb;

bool debug = false;
float sigma = 2;

// Loads specified image with FreeImagePlus
// Returns: loaded fipImage in float format
// Parameters:
    // (path) relative file path to load image
fipImage loadImage(string path)
{
    fipImage iImg;
    iImg.load(path.c_str());
    iImg.convertToFloat();
    if (debug) cout << "Opened " << path << endl;
    return iImg;
}

// Saves specified image with FreeImagePlus
// Parameters:
    // (oImg) given FreeImagePlus image to save
    // (path) relative file path to save image
void saveImage(fipImage oImg, string path)
{
    oImg.convertToType(FREE_IMAGE_TYPE::FIT_BITMAP);
    oImg.convertTo24Bits();
    if (debug) cout << "Saved " << path << endl;
    oImg.save(path.c_str());
}

//
float gaussian2D(int x, int y, float sigma)
{
    return 1 / (2 * M_PI * pow(sigma, 2)) * exp(-(pow(x, 2) + pow(y, 2)) / (2 * pow(sigma, 2)));
}

//
vector<vector<float>> kernelGenerator(unsigned int size)
{
    float sum = 0.0;

    vector<vector<float>> kernel(size, vector<float>(size, 0));

    if (debug) cout << "Kernel gen: " << endl;
    for (int x = 0; x < size; x++)
    {
        for (int y = 0; y < size; y++)
        {
            sum += kernel[x][y] = gaussian2D(x - int(size / 2), y - int(size / 2), sigma);
            if (debug) cout << x << ":" << y << ": " << kernel[x][y] << endl;
        }
    }

    if (debug) cout << "Kernel sum: " << sum << endl;
    if (debug) cout << "Kernel norm: " << endl;
    for (int x = 0; x < size; x++)
    {
        for (int y = 0; y < size; y++)
        {
            kernel[x][y] /= sum;
            if (debug) cout << x << ":" << y << ": " << kernel[x][y] << endl;
        }
    }

    return kernel;
}

// Applies a Gaussian blur to an image sequentially
// Returns: time elapsed to complete this function
// Parameters:
    // (inPath) relative file path to input image
    // (outPath) relative file path for desired output image
float sequentialGaussian(string inPath, string outPath, unsigned int kernelSize = 5)
{
    auto start = tick_count::now();
    fipImage iImg = loadImage(inPath);
    const int width = iImg.getWidth();
    const int height = iImg.getHeight();


    fipImage oImg = fipImage(FIT_FLOAT, width, height, 24);
    float* inPixels = (float*)iImg.accessPixels();
    float* outPixels = (float*)oImg.accessPixels();

    vector<vector<float>> kernel = kernelGenerator(kernelSize);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            for (int j = 0; j < kernelSize; j++)
            {
                for (int i = 0; i < kernelSize; i++)
                {
                    if (((y + j) < height && (y + j) >= 0) && ((x + i) < width && (x + i) >= 0))
                        outPixels[y * width + x] += kernel[j][i] * inPixels[(y + j) * width + (x + i)];
                }
            }
        }
    }

    saveImage(oImg, outPath);
    auto finish = tick_count::now();
    return (finish - start).seconds();
}

// Applies a Gaussian blur to an image parallelised
// Returns: time elapsed to complete this function
// Parameters:
// (inPath) relative file path to input image
// (outPath) relative file path for desired output image
float parallelisedGaussian(string inPath, string outPath, unsigned int kernelSize = 5)
{
    auto start = tick_count::now();
    fipImage iImg = loadImage(inPath);
    const int width = iImg.getWidth();
    const int height = iImg.getHeight();

    fipImage oImg = fipImage(FIT_FLOAT, width, height, 24);
    float* inPixels = (float*)iImg.accessPixels();
    float* outPixels = (float*)oImg.accessPixels();

    vector<vector<float>> kernel = kernelGenerator(kernelSize);

    tbb::parallel_for(blocked_range2d<uint64_t, uint64_t>(0, height, 8, 0, width, width>>2), [=](const blocked_range2d<uint64_t, uint64_t>& range)
    {
        auto yStart = range.rows().begin();
        auto yEnd = range.rows().end();
        auto xStart = range.cols().begin();
        auto xEnd = range.cols().end();

        for (int y = yStart; y != yEnd; y++)
        {
            for (int x = xStart; x != xEnd; x++)
            {
                for (int j = 0; j < kernelSize; j++)
                {
                    for (int i = 0; i < kernelSize; i++)
                    {
                        if (((y + j) < height && (y + j) >= 0) && ((x + i) < width && (x + i) >= 0))
                            outPixels[y * width + x] += kernel[j][i] * inPixels[(y + j) * width + (x + i)];
                    }
                }
            }
        }
    });

    saveImage(oImg, outPath);
    auto finish = tick_count::now();
    return (finish - start).seconds();
}

int main()
{
    int nt = task_scheduler_init::default_num_threads();
    task_scheduler_init T(nt);

    //Part 1 (Greyscale Gaussian blur): -----------DO NOT REMOVE THIS COMMENT----------------------------//
    cout << "Sequential, 3x3 kernel: " << sequentialGaussian("../Images/thinkpads.png", "../Images/thinkpads_sequential_3.png", 3) << endl;
    cout << "Sequential, 9x9 kernel: " << sequentialGaussian("../Images/thinkpads.png", "../Images/thinkpads_sequential_9.png", 9) << endl;
    cout << "Sequential, 21x21 kernel: " << sequentialGaussian("../Images/thinkpads.png", "../Images/thinkpads_sequential_21.png", 21) << endl;
    cout << "Parallelised, 3x3 kernel: " << parallelisedGaussian("../Images/thinkpads.png", "../Images/thinkpads_parallelised_3.png", 3) << endl;
    cout << "Parallelised, 9x9 kernel: " << parallelisedGaussian("../Images/thinkpads.png", "../Images/thinkpads_parallelised_9.png", 9) << endl;
    cout << "Parallelised, 21x21 kernel: " << parallelisedGaussian("../Images/thinkpads.png", "../Images/thinkpads_parallelised_21.png", 21) << endl;


    return 0;
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