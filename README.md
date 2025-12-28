
C/C++ Register Based Virtual Machine
Everything is contained in this one file
written completely from scratch, including all the string parsing

Video explaining the basic VM outline and the tests: 
https://www.youtube.com/watch?v=Pp9-9z29x30

This is the third language I've written (previous 2 were created based off the book 'Crafting Interpreters' by Robert Nystrom)
I wanted to explore VM performance and design my own architecture from scratch.
Eventually this will power a hidden magic system in the game I'm creating, also from scratch.
The player will be able to write simple instructions that execute spells for various effects in game.

# COMPILING

To build it, copy to a .cpp file, such as 'vmtest.cpp' , and run the windows developer console,
navigate to the direction its saved in, and compile it
such as doing: 
'cl vmtest.cpp' OR 'clang vmtest.cpp' (clang will output to a.exe)
'./vmtest'


# WHAT HAPPENS
What will happen, is all the tests will run after another, printing all the operations and test results
Once those run, you will enter the repl mode, where you can type in your own commands to the console
So run the program, see the initial printf dump, and then type:

# REPL EXAMPLE (press enter after every line you type into the console)

LOAD $0 #1  
LOAD $1 #2
ADD $0 $1       (number stored in register 1 is added to register 2)
/registers      (this prints the values stored in every registe, register 0 should now hold 3 (1+2=3))
/program        (prints the hex representation of all the instructions so far)
/clear          (clears the registers and instructions)
/quit           (quits out of the application)

