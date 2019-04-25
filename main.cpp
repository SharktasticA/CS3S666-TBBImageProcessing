#include <iostream>
#include <vector>
#include <chrono>
#include <math.h>
#include <tbb/task_scheduler_init.h>
#include <tbb/tick_count.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range2d.h>
#include <FreeImagePlus.h>

using namespace std;
using namespace tbb;

// Flags debugging messages
bool debug = false;

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

// Calculates the 2-dimensional Gaussian distribution for
// the given kernel position
// Returns: evaluated Gaussian distribution
// Parameters:
    // (x) kernel's x-position
    // (y) kernel's y-position
    // (sigma) Gaussian standard deviation
float gauss(int x, int y, float sigma)
{
    return 1 / (2 * M_PI * pow(sigma, 2)) * exp(-(pow(x, 2) + pow(y, 2)) / (2 * pow(sigma, 2)));
}

// Generates a Gaussian filter matrix (kernel) that can be
// used to blur an image with
// Returns: 2-dimensional float vector containing a kernel
// of Gaussian distribution values
// Parameters:
    // (size) desired kernel's width/height size
    // (sigma) Gaussian standard deviation
vector<vector<float>> kernelGenerator(unsigned int size, float sigma)
{
    // Ensures
    int remainder = size % 2;
    if (remainder == 0)
    {
        size += 1;
        if (debug) cout << "Kernel size corrected from " << size-1 << " to " << size << endl;
    }

    float sum = 0.0;

    vector<vector<float>> kernel(size, vector<float>(size, 0));

    if (debug) cout << "Kernel gen: " << endl;
    for (int x = 0; x < size; x++)
    {
        for (int y = 0; y < size; y++)
        {
            sum += kernel[x][y] = gauss(x - int(size / 2), y - int(size / 2), sigma);
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

// Sequentially applies Gaussian blur to an image
// Returns: time elapsed to complete the process
// Parameters:
    // (inPath) relative file path to input image
    // (outPath) relative file path for desired output image
    // (kernelSize) sampling kernel size (controls blur strength)
float sequentialGaussian(string inPath, string outPath, unsigned int kernelSize)
{
    fipImage iImg = loadImage(inPath);
    const int width = iImg.getWidth();
    const int height = iImg.getHeight();

    fipImage oImg = fipImage(FIT_FLOAT, width, height, 24);
    float* inPixels = (float*)iImg.accessPixels();
    float* outPixels = (float*)oImg.accessPixels();

    vector<vector<float>> kernel = kernelGenerator(kernelSize, kernelSize);
    kernelSize = kernel.size();
    int kernelHalf = kernelSize / 2;

    auto start = tick_count::now();
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            if (kernelSize == 1) outPixels[y * width + x] += kernel[0][0] * inPixels[y * width + x];
            else
            {
                for (int j = -kernelHalf; j <= kernelHalf; j++)
                {
                    for (int i = -kernelHalf; i <= kernelHalf; i++)
                    {
                        if (((y + j) > 0 && (x + i) > 0) && ((y + j) < height && (x + i) < width))
                            outPixels[y * width + x] += kernel[i + kernelHalf][j + kernelHalf] * inPixels[(y + j) * width + (x + i)];
                    }
                }
            }
        }
    }

    auto finish = tick_count::now();
    saveImage(oImg, outPath);
    return (finish - start).seconds();
}

// Parallel applies Gaussian blur to an image
// Returns: time elapsed to complete the process
// Parameters:
    // (inPath) relative file path to input image
    // (outPath) relative file path for desired output image
    // (kernelSize) sampling kernel size (controls blur strength)
float parallelGaussian(string inPath, string outPath, unsigned int kernelSize)
{
    fipImage iImg = loadImage(inPath);
    const int width = iImg.getWidth();
    const int height = iImg.getHeight();

    fipImage oImg = fipImage(FIT_FLOAT, width, height, 24);
    float* inPixels = (float*)iImg.accessPixels();
    float* outPixels = (float*)oImg.accessPixels();

    vector<vector<float>> kernel = kernelGenerator(kernelSize, kernelSize);
    kernelSize = kernel.size();
    int kernelHalf = kernelSize / 2;

    auto start = tick_count::now();
    tbb::parallel_for(blocked_range2d<int, int>(0, height, 0, 0, width, 0), [=](const blocked_range2d<int, int>& range)
    {
        int yStart = range.rows().begin();
        int yEnd = range.rows().end();
        int xStart = range.cols().begin();
        int xEnd = range.cols().end();

        for (int y = yStart; y != yEnd; y++)
        {
            for (int x = xStart; x != xEnd; x++)
            {
                if (kernelSize == 1) outPixels[y * width + x] += kernel[0][0] * inPixels[y * width + x];
                else
                {
                    for (int j = -kernelHalf; j <= kernelHalf; j++)
                    {
                        for (int i = -kernelHalf; i <= kernelHalf; i++)
                        {
                            if (((y + j) > 0 && (x + i) > 0) && ((y + j) < height && (x + i) < width))
                                outPixels[y * width + x] += kernel[i + kernelHalf][j + kernelHalf] * inPixels[(y + j) * width + (x + i)];
                        }
                    }
                }
            }
        }
    });

    auto finish = tick_count::now();
    saveImage(oImg, outPath);
    return (finish - start).seconds();
}

// Parallel applies Gaussian blur to an image with
// custom grain size
// Returns: time elapsed to complete the process
// Parameters:
    // (inPath) relative file path to input image
    // (outPath) relative file path for desired output image
    // (kernelSize) sampling kernel size (controls blur strength)
    // (grain) allows custom chunk size to be specified
float parallelGaussian(string inPath, string outPath, unsigned int kernelSize, int grain)
{
    fipImage iImg = loadImage(inPath);
    const int width = iImg.getWidth();
    const int height = iImg.getHeight();

    fipImage oImg = fipImage(FIT_FLOAT, width, height, 24);
    float* inPixels = (float*)iImg.accessPixels();
    float* outPixels = (float*)oImg.accessPixels();

    vector<vector<float>> kernel = kernelGenerator(kernelSize, kernelSize);
    kernelSize = kernel.size();
    int kernelHalf = kernelSize / 2;

    auto start = tick_count::now();

    tbb::parallel_for(blocked_range2d<int, int>(0, height, grain, 0, width, grain), [=](const blocked_range2d<int, int>& range)
    {
        int yStart = range.rows().begin();
        int yEnd = range.rows().end();
        int xStart = range.cols().begin();
        int xEnd = range.cols().end();

        for (int y = yStart; y != yEnd; y++)
        {
            for (int x = xStart; x != xEnd; x++)
            {
                if (kernelSize == 1) outPixels[y * width + x] += kernel[0][0] * inPixels[y * width + x];
                else
                {
                    for (int j = -kernelHalf; j <= kernelHalf; j++)
                    {
                        for (int i = -kernelHalf; i <= kernelHalf; i++)
                        {
                            if (((y + j) > 0 && (x + i) > 0) && ((y + j) < height && (x + i) < width))
                                outPixels[y * width + x] += kernel[i + kernelHalf][j + kernelHalf] * inPixels[(y + j) * width + (x + i)];
                        }
                    }
                }
            }
        }
    }, simple_partitioner());

    auto finish = tick_count::now();
    saveImage(oImg, outPath);
    return (finish - start).seconds();
}

// Test driver program used from obtaining test
// results for different machines for the report.
void machineTest()
{
    // Sequential tests
    cout << "Sequential, 1x1 kernel: " << sequentialGaussian("../Images/thinkpads.png", "../Images/thinkpads_sequential_1.png", 1) << "s" << endl;
    cout << "Sequential, 3x3 kernel: " << sequentialGaussian("../Images/thinkpads.png", "../Images/thinkpads_sequential_3.png", 3) << "s" << endl;
    cout << "Sequential, 9x9 kernel: " << sequentialGaussian("../Images/thinkpads.png", "../Images/thinkpads_sequential_9.png", 9) << "s" << endl;
    cout << "Sequential, 27x27 kernel: " << sequentialGaussian("../Images/thinkpads.png", "../Images/thinkpads_sequential_27.png", 27) << "s" << endl;
    cout << "Sequential, 81x81 kernel: " << sequentialGaussian("../Images/thinkpads.png", "../Images/thinkpads_sequential_81.png", 81) << "s" << endl;

    // Parallel auto chunk tests
    cout << "Parallel, 1x1 kernel, auto chunk: " << parallelGaussian("../Images/thinkpads.png", "../Images/thinkpads_parallel_1.png", 1) << "s" << endl;
    cout << "Parallel, 3x3 kernel, auto chunk: " << parallelGaussian("../Images/thinkpads.png", "../Images/thinkpads_parallel_3.png", 3) << "s" << endl;
    cout << "Parallel, 9x9 kernel, auto chunk: " << parallelGaussian("../Images/thinkpads.png", "../Images/thinkpads_parallel_9.png", 9) << "s" << endl;
    cout << "Parallel, 27x27 kernel, auto chunk: " << parallelGaussian("../Images/thinkpads.png", "../Images/thinkpads_parallel_27.png", 27) << "s" << endl;
    cout << "Parallel, 81x81 kernel, auto chunk: " << parallelGaussian("../Images/thinkpads.png", "../Images/thinkpads_parallel_81.png", 81) << "s" << endl;

    // Parallel 256 grain tests
    cout << "Parallel, 1x1 kernel, 256 grain: " << parallelGaussian("../Images/thinkpads.png", "../Images/thinkpads_parallel_1_256.png", 1, 256) << "s" << endl;
    cout << "Parallel, 3x3 kernel, 256 grain: " << parallelGaussian("../Images/thinkpads.png", "../Images/thinkpads_parallel_3_256.png", 3, 256) << "s" << endl;
    cout << "Parallel, 9x9 kernel, 256 grain: " << parallelGaussian("../Images/thinkpads.png", "../Images/thinkpads_parallel_9_256.png", 9, 256) << "s" << endl;
    cout << "Parallel, 27x27 kernel, 256 grain: " << parallelGaussian("../Images/thinkpads.png", "../Images/thinkpads_parallel_27_256.png", 27, 256) << "s" << endl;
    cout << "Parallel, 81x81 kernel, 256 grain: " << parallelGaussian("../Images/thinkpads.png", "../Images/thinkpads_parallel_81_256.png", 81, 256) << "s" << endl;

    // Parallel 2048 grain tests
    cout << "Parallel, 1x1 kernel, 2048 grain: " << parallelGaussian("../Images/thinkpads.png", "../Images/thinkpads_parallel_1_2048.png", 1, 2048) << "s" << endl;
    cout << "Parallel, 3x3 kernel, 2048 grain: " << parallelGaussian("../Images/thinkpads.png", "../Images/thinkpads_parallel_3_2048.png", 3, 2048) << "s" << endl;
    cout << "Parallel, 9x9 kernel, 2048 grain: " << parallelGaussian("../Images/thinkpads.png", "../Images/thinkpads_parallel_9_2048.png", 9, 2048) << "s" << endl;
    cout << "Parallel, 27x27 kernel, 2048 grain: " << parallelGaussian("../Images/thinkpads.png", "../Images/thinkpads_parallel_27_2048.png", 27, 2048) << "s" << endl;
    cout << "Parallel, 81x81 kernel, 2048 grain: " << parallelGaussian("../Images/thinkpads.png", "../Images/thinkpads_parallel_81_2048.png", 81, 2048) << "s" << endl;
}

int main()
{
    int nt = task_scheduler_init::default_num_threads();
    task_scheduler_init T(nt);

    //Part 1 (Greyscale Gaussian blur): -----------DO NOT REMOVE THIS COMMENT----------------------------//
    machineTest();
    //float sequentialTest = sequentialGaussian("../Images/render_1.png", "../Images/grey_blurred.png", 3);
    //float parallelTest = parallelisedGaussian("../Images/render_1.png", "../Images/grey_blurred.png", 3);

    //cout << "Sequential test: " << sequentialTest << "s" << endl;
    //cout << "Parallel test: " << parallelTest << "s" << endl;
    //cout << "Speed increase: " << (sequentialTest / parallelTest) * 100 << "%" << endl;

    return 0;
    //Part 2 (Colour image processing): -----------DO NOT REMOVE THIS COMMENT----------------------------//

    // Setup Input image array
    fipImage inputImage;
    inputImage.load("../Images/thinkpads.png");

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