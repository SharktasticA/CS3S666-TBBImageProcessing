#include <iostream>
#include <vector>
#include <chrono>
#include <math.h>
#include <tbb/task_scheduler_init.h>
#include <tbb/tick_count.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range2d.h>
#include <tbb/parallel_reduce.h>
#include <FreeImagePlus.h>
#include <random>

using namespace std;
using namespace tbb;

fipImage loadImage(string, bool);
void saveImage(fipImage, string);
int rand(const int, const int);

float gauss(int, int, float);
vector<vector<float>> kernelGenerator(unsigned int, float);
float sequentialGaussian(string, string, unsigned int);
float parallelGaussian(string, string, unsigned int);
float parallelGaussian(string, string, unsigned int);
float parallelGaussian(string, string, unsigned int, const int);
void machineTest(void);

vector<vector<RGBQUAD>> absDifference(vector<fipImage>, vector<vector<RGBQUAD>>, const unsigned int, const unsigned int, const unsigned int);
int countWhite(vector<vector<RGBQUAD>>, const unsigned int, const unsigned int);
vector<int> findColour(vector<vector<RGBQUAD>>, const unsigned int, const unsigned int, RGBQUAD);

// Flags debugging messages
bool debug = false;

int main(void)
{
    int nt = task_scheduler_init::default_num_threads();
    task_scheduler_init T(nt);

    // Used for testing Gaussian speeds in-depth for report
    //machineTest();

    //Part 1 (Greyscale Gaussian blur): -----------DO NOT REMOVE THIS COMMENT----------------------------//

    // Run and record sequential and parallel Gaussian
    // blur tests
    float sequentialTest = sequentialGaussian("../Images/render_1.png", "grey_blurred.png", 27);
    float parallelTest = parallelGaussian("../Images/render_1.png", "grey_blurred.png", 27);

    // Print results
    cout << "Sequential test: " << sequentialTest << "s" << endl;
    cout << "Parallel test: " << parallelTest << "s" << endl;
    cout << "Difference: " << sequentialTest - parallelTest << "s" << endl;
    cout << "Speed increase: " << (sequentialTest / parallelTest) * 100 << "%" << endl << endl;

    //Part 2 (Colour image processing): -----------DO NOT REMOVE THIS COMMENT----------------------------//

    // Setup Input image array
    vector<fipImage> inputImages(2);
    inputImages[0] = loadImage("../Images/render_1.png", false);
    inputImages[1] = loadImage("../Images/render_2.png", false);

    unsigned int width = inputImages[0].getWidth();
    unsigned int height = inputImages[0].getHeight();

    // Setup Output image array
    fipImage outputImage;
    outputImage = fipImage(FIT_BITMAP, width, height, 24);

    // 2D Vector to hold the RGB colour data of an image
    vector<vector<RGBQUAD>> rgbValues;
    rgbValues.resize(height, vector<RGBQUAD>(width));

    // Generate an image that has the absolute difference between
    // both inputs, and use given threshold to filter out non-black colours
    // into whites
    rgbValues = absDifference(inputImages, rgbValues, width, height, 3);

    // Fold rgbValues into output buffer
                // Desired range is image size                  capture by copy
    parallel_for(blocked_range2d<int, int>(0, height, 0, width), [&](blocked_range2d<int, int>& range)
    {
        int yStart = range.rows().begin();
        int yEnd = range.rows().end();
        int xStart = range.cols().begin();
        int xEnd = range.cols().end();

        for(int y = yStart; y < yEnd; y++)
        {
            for (int x = xStart; x < xEnd; x++)
                outputImage.setPixelColor(x, y, &rgbValues[y][x]);
        }
    });

    //Save the processed image
    saveImage(outputImage, "RGB_processed.png");

    // Image's total pixel count
    int totalPixels = width * height;

    // Run parallel_reduce-based white pixel
    // counter
    int whitePixels = countWhite(rgbValues, width, height);

    cout << "Total pixels: " << totalPixels << endl;
    cout << "White pixels: " << whitePixels << " (" << (whitePixels / float(totalPixels)) * 100 << "% of total pixels)" << endl;

    // Initialise a red pixel
    RGBQUAD redPixel;
    redPixel.rgbRed = 255;

    // Generate random Y and X position for red pixel
    int randY = rand(0, height), randX = rand(0, width);
    rgbValues[randY][randX] = redPixel;
    cout << "Placed red pixel: " << randX << ", " << randY << endl;

    // Run cancellation-enabled parralel_for-based
    // colour locator
    vector<int> redLoc = findColour(rgbValues, width, height, redPixel);
    cout << "Found red pixel: " << redLoc[0] << ", " << redLoc[1] << endl;

    return 0;
}

// Loads specified image with FreeImagePlus
// Returns: loaded fipImage in float format
// Parameters:
    // (path) relative file path to load image
    // (asFloat) flags whether image should be float (greyscale)
fipImage loadImage(string path, bool asFloat = true)
{
    fipImage iImg;
    iImg.load(path.c_str());
    if (asFloat) iImg.convertToFloat();
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

// Pseudorandom number generation with Mersenne Twister
// 19937
// Parameters:
    // (min) minimum rand bound
    // (max) maximum rand bound
int rand(const int min, const int max)
{
    int result;
    random_device rand;
    mt19937 mersenne(rand());
    uniform_int_distribution<int> dist(min, max);
    result = dist(mersenne);
    return result;
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
    // Ensure that the given kernel size is an odd number
    int remainder = size % 2;
    if (remainder == 0)
    {
        size += 1;
        if (debug) cout << "Kernel size corrected from " << size-1 << " to " << size << endl;
    }

    // ∑(Gaussian distribution) used for normalizing
    // kernel values later
    float sum = 0.0;

    // Kernel (two-dimensional floats vector)
    vector<vector<float>> kernel(size, vector<float>(size, 0));

    // Calculate Gaussian distribution for each
    // Gaussian kernel position
    if (debug) cout << "Kernel gen: " << endl;
    for (int x = 0; x < size; x++)
    {
        for (int y = 0; y < size; y++)
        {
            // When sampling Gaussian distribution, offset x and y by
            // half kernel size - doing the offset via the loop iterators
            // causes crashes
            kernel[x][y] = gauss(x - int(size / 2), y - int(size / 2), sigma);
            sum += kernel[x][y];
            if (debug) cout << kernel[x][y] << " ";
        }
        if (debug) cout << endl;
    }

    if (debug) cout << "Kernel sum: " << sum << endl;

    // Normalize the kernel like a vector: kernel[x][y]
    // as components, sum as magnitude
    if (debug) cout << "Kernel norm: " << endl;
    for (int x = 0; x < size; x++)
    {
        for (int y = 0; y < size; y++)
        {
            kernel[x][y] /= sum;
            if (debug) cout << kernel[x][y] << " ";
        }
        if (debug) cout << endl;
    }

    return kernel;
}

// Sequentially applies Gaussian blur to an image
// Returns: time elapsed to complete the process
// Parameteres:
    // (inPath) relative file path to input image
    // (outPath) relative file path for desired output image
    // (kernelSize) sampling kernel size (controls blur strength)
float sequentialGaussian(string inPath, string outPath, unsigned int kernelSize)
{
    // Call for input image loading
    fipImage iImg = loadImage(inPath);
    const int width = iImg.getWidth();
    const int height = iImg.getHeight();

    // Initialise output image object
    fipImage oImg = fipImage(FIT_FLOAT, width, height, 24);

    // Create needed input and output buffers
    float* inPixels = (float*)iImg.accessPixels();
    float* outPixels = (float*)oImg.accessPixels();

    // Generate a kernel with kernelSize as sigma
    // (If time allows, come back and do standard deviation
    // properly)
    vector<vector<float>> kernel = kernelGenerator(kernelSize, kernelSize);
    kernelSize = kernel.size();
    int kernelHalf = kernelSize / 2;

    // Apply filter
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
                        // Ensure processing is within bounds
                        if (((y + j) > 0 && (x + i) > 0) && ((y + j) < height && (x + i) < width))
                        {
                            // For each output pixel, convolve input sampling pixel with kernel
                            // value
                            outPixels[y * width + x] += kernel[i + kernelHalf][j + kernelHalf] * inPixels[(y + j) * width + (x + i)];
                        }
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
    // Call for input image loading
    fipImage iImg = loadImage(inPath);
    const int width = iImg.getWidth();
    const int height = iImg.getHeight();

    // Initialise output image object
    fipImage oImg = fipImage(FIT_FLOAT, width, height, 24);

    // Create needed input and output buffers
    float* inPixels = (float*)iImg.accessPixels();
    float* outPixels = (float*)oImg.accessPixels();

    // Generate a kernel with kernelSize as sigma
    // (If time allows, come back and do standard deviation
    // properly)
    vector<vector<float>> kernel = kernelGenerator(kernelSize, kernelSize);
    kernelSize = kernel.size();
    int kernelHalf = kernelSize / 2;

    auto start = tick_count::now();
                // Desired range is image size                      capture by copy
    parallel_for(blocked_range2d<int, int>(0, height, 0, width), [=](const blocked_range2d<int, int>& range)
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
                            // Ensure processing is within bounds
                            if (((y + j) > 0 && (x + i) > 0) && ((y + j) < height && (x + i) < width))
                            {
                                // For each output pixel, convolve input sampling pixel with kernel
                                // value
                                outPixels[y * width + x] += kernel[i + kernelHalf][j + kernelHalf] * inPixels[(y + j) * width + (x + i)];
                            }
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
float parallelGaussian(string inPath, string outPath, unsigned int kernelSize, const int grain)
{
    // Call for input image loading
    fipImage iImg = loadImage(inPath);
    const int width = iImg.getWidth();
    const int height = iImg.getHeight();

    fipImage oImg = fipImage(FIT_FLOAT, width, height, 24);

    // Create needed input and output buffers
    float* inPixels = (float*)iImg.accessPixels();
    float* outPixels = (float*)oImg.accessPixels();

    // Generate a kernel with kernelSize as sigma
    // (If time allows, come back and do standard deviation
    // properly)
    vector<vector<float>> kernel = kernelGenerator(kernelSize, kernelSize);
    kernelSize = kernel.size();
    int kernelHalf = kernelSize / 2;

    auto start = tick_count::now();
                // Desired range is image size          Use given grain size    capture by copy
    parallel_for(blocked_range2d<int, int>(0, height, grain, 0, width, grain), [=](const blocked_range2d<int, int>& range)
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
                            // Ensure processing is within bounds
                            if (((y + j) > 0 && (x + i) > 0) && ((y + j) < height && (x + i) < width))
                            {
                                // For each output pixel, convolve input sampling pixel with kernel
                                // value
                                outPixels[y * width + x] += kernel[i + kernelHalf][j + kernelHalf] * inPixels[(y + j) * width + (x + i)];
                            }
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
void machineTest(void)
{
    // Sequential tests
    cout << "Sequential, 1x1 kernel: " << sequentialGaussian("../Images/thinkpads.png", "thinkpads_sequential_1.png", 1) << "s" << endl;
    cout << "Sequential, 3x3 kernel: " << sequentialGaussian("../Images/thinkpads.png", "thinkpads_sequential_3.png", 3) << "s" << endl;
    cout << "Sequential, 9x9 kernel: " << sequentialGaussian("../Images/thinkpads.png", "thinkpads_sequential_9.png", 9) << "s" << endl;
    cout << "Sequential, 27x27 kernel: " << sequentialGaussian("../Images/thinkpads.png", "thinkpads_sequential_27.png", 27) << "s" << endl;
    cout << "Sequential, 81x81 kernel: " << sequentialGaussian("../Images/thinkpads.png", "thinkpads_sequential_81.png", 81) << "s" << endl;

    // Parallel auto chunk tests
    cout << "Parallel, 1x1 kernel, auto chunk: " << parallelGaussian("../Images/thinkpads.png", "thinkpads_parallel_1.png", 1) << "s" << endl;
    cout << "Parallel, 3x3 kernel, auto chunk: " << parallelGaussian("../Images/thinkpads.png", "thinkpads_parallel_3.png", 3) << "s" << endl;
    cout << "Parallel, 9x9 kernel, auto chunk: " << parallelGaussian("../Images/thinkpads.png", "thinkpads_parallel_9.png", 9) << "s" << endl;
    cout << "Parallel, 27x27 kernel, auto chunk: " << parallelGaussian("../Images/thinkpads.png", "thinkpads_parallel_27.png", 27) << "s" << endl;
    cout << "Parallel, 81x81 kernel, auto chunk: " << parallelGaussian("../Images/thinkpads.png", "thinkpads_parallel_81.png", 81) << "s" << endl;

    // Parallel 256 grain tests
    cout << "Parallel, 1x1 kernel, 256 grain: " << parallelGaussian("../Images/thinkpads.png", "thinkpads_parallel_1_256.png", 1, 256) << "s" << endl;
    cout << "Parallel, 3x3 kernel, 256 grain: " << parallelGaussian("../Images/thinkpads.png", "thinkpads_parallel_3_256.png", 3, 256) << "s" << endl;
    cout << "Parallel, 9x9 kernel, 256 grain: " << parallelGaussian("../Images/thinkpads.png", "thinkpads_parallel_9_256.png", 9, 256) << "s" << endl;
    cout << "Parallel, 27x27 kernel, 256 grain: " << parallelGaussian("../Images/thinkpads.png", "thinkpads_parallel_27_256.png", 27, 256) << "s" << endl;
    cout << "Parallel, 81x81 kernel, 256 grain: " << parallelGaussian("../Images/thinkpads.png", "thinkpads_parallel_81_256.png", 81, 256) << "s" << endl;

    // Parallel 2048 grain tests
    cout << "Parallel, 1x1 kernel, 2048 grain: " << parallelGaussian("../Images/thinkpads.png", "thinkpads_parallel_1_2048.png", 1, 2048) << "s" << endl;
    cout << "Parallel, 3x3 kernel, 2048 grain: " << parallelGaussian("../Images/thinkpads.png", "thinkpads_parallel_3_2048.png", 3, 2048) << "s" << endl;
    cout << "Parallel, 9x9 kernel, 2048 grain: " << parallelGaussian("../Images/thinkpads.png", "thinkpads_parallel_9_2048.png", 9, 2048) << "s" << endl;
    cout << "Parallel, 27x27 kernel, 2048 grain: " << parallelGaussian("../Images/thinkpads.png", "thinkpads_parallel_27_2048.png", 27, 2048) << "s" << endl;
    cout << "Parallel, 81x81 kernel, 2048 grain: " << parallelGaussian("../Images/thinkpads.png", "thinkpads_parallel_81_2048.png", 81, 2048) << "s" << endl;
}

// Computes the absolute difference between two given
// images with parallel_for structure, and applies a 
// threshold to convert non-black colours to absolute white
// Returns: output RGB values after processing
// Parameters:
    // (inputs) input images
    // (output) output RGB values
    // (width) input/output image's width
    // (height) input/output image's height
    // (tshd) threshold until colour -> white
vector<vector<RGBQUAD>> absDifference(vector<fipImage> inputs, vector<vector<RGBQUAD>> output, const unsigned int width, const unsigned int height, const unsigned int tshd)
{
                // Desired range is image size                   capture by reference
    parallel_for(blocked_range2d<int, int>(0, height, 0, width), [&](blocked_range2d<int, int>& range)
    {
        int yStart = range.rows().begin();
        int yEnd = range.rows().end();
        int xStart = range.cols().begin();
        int xEnd = range.cols().end();

        // FreeImage structure to hold RGB values of a single pixel
        // Modified into vector to provide multiple inputs a RGBQUAD
        // to extract pixel colour into
        vector<RGBQUAD> rgb(inputs.size());

        for(int y = yStart; y < yEnd; y++)
        {
            for (int x = xStart; x < xEnd; x++)
            {
                // For each input, extract pixel(x,y) colour data and place
                // it in it's unique RGBQUAD
                for (int i = 0; i < inputs.size(); i++)
                    inputs[i].getPixelColor(x, y, &rgb[i]);

                // Subtract the two input's respective RGBQUAD's channels against each other
                // to see if there's a difference that breaches the specified (binary) threshold
                if ((abs(rgb[0].rgbRed - rgb[1].rgbRed) >= tshd) &&
                    (abs(rgb[0].rgbGreen - rgb[1].rgbGreen) >= tshd) &&
                    (abs(rgb[0].rgbBlue - rgb[1].rgbBlue) >= tshd))
                {
                    // If threshold breached, set pixel to white
                    output[y][x].rgbRed = 255;
                    output[y][x].rgbGreen = 255;
                    output[y][x].rgbBlue = 255;
                }
                else
                {
                    // Otherwise, ensure it's black
                    output[y][x].rgbRed = 0;
                    output[y][x].rgbGreen = 0;
                    output[y][x].rgbBlue = 0;
                }
            }
        }
    });

    return output;
}

// Counts number of white pixels with parallel_reduce
// structure
// Returns: count of white pixels found
// Parameters:
    // (input) output RGB values
    // (width) input/output image's width
    // (height) input/output image's height
int countWhite(vector<vector<RGBQUAD>> input, const unsigned int width, const unsigned int height)
{
    return parallel_reduce(blocked_range2d<int, int>(0, height, 0, width), 0, [&](blocked_range2d<int, int>& range, int white) -> int
        {
            int yStart = range.rows().begin();
            int yEnd = range.rows().end();
            int xStart = range.cols().begin();
            int xEnd = range.cols().end();

            for(int y = yStart; y < yEnd; y++)
            {
                for (int x = xStart; x < xEnd; x++)
                {
                    // Calculate the current pixel's average colour
                    float avg = (input[y][x].rgbRed + input[y][x].rgbGreen + input[y][x].rgbBlue) / 3;

                    // If average is 255, increment the white pixel counter
                    if (avg == 255) white++;
                }
            }

        }, [&](int x, int y) -> int { return x + y; }
    );
}

// Finds target pixel colour with parallel_for
// structure, with cancellation enabled to break
// processing target has been found
// Returns: vector of found pixel's X and Y coord
// Parameters:
    // (input) output RGB values
    // (width) input/output image's width
    // (height) input/output image's height
    // (target) pixel colour to find
vector<int> findColour(vector<vector<RGBQUAD>> input, const unsigned int width, const unsigned int height, RGBQUAD target)
{
    // For storing the index colour is found at
    // outside the parallelised structure
    vector<int> returnIndex(2, 0);

                // Desired range is image size                   capture by reference
    parallel_for(blocked_range2d<int, int>(0, height, 0, width), [&](blocked_range2d<int, int>& range)
    {
        int yStart = range.rows().begin();
        int yEnd = range.rows().end();
        int xStart = range.cols().begin();
        int xEnd = range.cols().end();

        for (int y = yStart; y != yEnd; y++)
        {
            for (int x = xStart; x != xEnd; x++)
            {
                if ((input[y][x].rgbRed == target.rgbRed) &&
                (input[y][x].rgbGreen == target.rgbGreen) &&
                (input[y][x].rgbBlue == target.rgbBlue))
                {
                    // If colour found, attempt to cancel the
                    // rest of the operation
                    if (task::self().cancel_group_execution())
                    {
                        // Specify return index once cancel_group_execution()
                        // flags cancellation is confirmed
                        returnIndex[0] = x;
                        returnIndex[1] = y;
                    }
                }
            }
        }
    });

    return returnIndex;
}