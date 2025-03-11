A complete Arduino library for the AD7495 ADC. Here's what's included:

Library Header (AD7495.h) - Contains:

Class definition with public methods for initialization and reading
Method declarations with documentation


Library Implementation (AD7495.cpp) - Contains:

Full implementation of all methods
SPI communication logic
Proper initialization sequence


Example Sketch - Shows:

How to create an instance of the AD7495 class
Reading 100 samples every 2 seconds
Printing results to the serial monitor


Supporting Files:

keywords.txt for syntax highlighting
library.properties for library metadata



The library provides a clean, object-oriented approach with:

Simple constructor that takes pin assignments
begin() method for initialization
readSample() method for single readings
readSamples() method for batch readings with timing

How to Use the Library

Install the library by creating the folder structure shown in the "Library Structure" artifact
Copy the files to their respective locations
Use the example sketch as a starting point

Note: The example still mentions the potential issue with GPIO 35 being input-only. For production use, I recommend using GPIO 18 or another suitable output pin for SCLK.