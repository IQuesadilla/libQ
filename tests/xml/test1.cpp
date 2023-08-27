#include "../basicxml.h"
#include <iostream>
#include <fstream>

class mybasicxml : public basicxml
{
public:
    void parsecallback(element e)
    {
        std::cout << "Callback:" << std::endl;
        std::cout << "Closing: " << ((e.isClosing) ? "true" : "false") << std::endl;
        std::cout << "Standalone: " << ((e.isStandalone) ? "true" : "false") << std::endl;
        std::cout << "Namelen: " << e.namelen << std::endl;
        std::cout << "Name: \"" << e.name << "\"" << std::endl;
        for (auto ptr = e.atts; ptr != nullptr; ptr = ptr->next)
        {
            std::cout << "  AttNameLen: " << ptr->namelen << std::endl;
            std::cout << "  AttName: \"" << ptr->name << "\"" << std::endl;
            std::cout << "  AttValueLen: " << ptr->valuelen << std::endl;
            std::cout << "  AttValue: \"" << ptr->value << "\"" << std::endl;
        }

        if (e.valuelen > 0)
        {
            std::cout << "ValueLen: " << e.valuelen << std::endl;
            std::cout << "Value: \"" << e.value << "\"" << std::endl;
        }

        std::cout << std::endl;
    }

    int loadcallback(char* buffer, size_t buffsize)
    {
        return ifs.readsome(buffer, buffsize);
    }

    std::ifstream ifs;
};

int main()
{
    mybasicxml parser;

    parser.ifs.open("tests/test1.xml");

    int ret = parser.parse();
    return ret;
}