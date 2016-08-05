# Purpose #

These source files are used to program the fan controller.

# Compiling #

To compile the software, you will need to have MPLAB X and MPLAB XC16 compiler installed on your system.
Within MPLAB X:

 1. Create a standalone project.
 2. Select the device - PIC24FV16KM202
 3. Select your debugger/programmer
 4. Add all *.c and *.s files to the project "Source Files"
 5. Add all *.h files to the project "Header Files"
 6. Compile

In xc16-gcc settings, be sure to include the directory with the header files or you will have trouble building:

 1. File -> Project Properties
 2. Click "xc16-gcc" on the left pane
 3. In the right pane, click the drop-down at the top and select "Preprocessing and messages"
 4. Under "C include dirs", be sure that the location for your header files is specified if it is outside
 of the project

# How to Flash #

To program the fan controller, you will need the hardware necessary to program a Microchip board.
I prefer to use an [ICD3](http://www.microchip.com/Developmenttools/ProductDetails.aspx?PartNO=DV164035),
but a [PICkit 3](http://www.microchip.com/Developmenttools/ProductDetails.aspx?PartNO=PG164130) should
work just as well.

The programming header on the board is compatible with the PICkit3 connector with no modification.  A
0.1" harness with the following pinout would have to be made for an ICD3:

 1. ~MCLR/Vpp
 2. Vdd
 3. Vss
 4. PGD
 5. PGC
 
Connect to the target and hit "Make and Program Device Main Project" button (looks like a green down arrow).

# Where to Next? #

Visit [for(embed)](http://www.forembed.com) for more projects and details!