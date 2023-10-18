#include "cfifo.h"
#include <iostream>

const char *mystring = "Hello, this is a string that is longer than 4 characters.";

int main()
{
    char buffer[4];
    cfifo myfifo(buffer,4);

    int length = strlen(mystring);
    for (int i = 0; i < length;)
    {
        while ( myfifo.putch(&mystring[i]) && i < length ) ++i;

        char temp;
        while ( myfifo.getch(&temp) )
        {
            std::cout << temp;
        }
    }
    std::cout << std::endl;
}