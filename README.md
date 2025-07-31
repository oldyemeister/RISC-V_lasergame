# RISC-V_lasergame
A laser reflection puzzle game implemented on DE1-SoC FPGA board with VGA output and PS/2 keyboard control. The inspiration of this game originates from a hacking minigame of the Doomsday heist in GTAOL

## Online version using CPUlator

How to play:

1. Visit https://cpulator.01xz.net/?sys=rv32-de1soc
   
2. Select C as the language and paste Laser_Puzzle_CPUlator.c into the editor

3. Press Compile and Load, the compilation should take 5-10 seconds

4. When compilation is over, press the continue button in the top row

5. Scroll down in the devices section and find the column "PS/2 keyboard or mouse", click on the "Type here" box
   <img width="868" height="310" alt="image" src="https://github.com/user-attachments/assets/33b21b5c-2351-45b4-8567-7aeafecb1584" />

6. Scroll up to find "VGA pixel buffer" and start playing!

Arrow keys：

← rotate selected mirror counter clockwise

→ rotate selected mirror clockwise

↑ select the previous mirror

↓ select the next mirror

When the game fails/passes, press any arrow key to reset the game.

##FPGA Version

Firmware requirements: De1-Soc board, VGA monitor, PS/2 keyboard

1. Connect the VGA monitor and PS/2 keyboard

2. Download the FPGA_Version folder and set Laser_Puzzle_FPGA.c as the top-level module in your monitor program

3. Compile and run!



