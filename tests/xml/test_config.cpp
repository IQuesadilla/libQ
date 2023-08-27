#include "../basicxml.h"
#include <iostream>
#include <fstream>

class mybasicxml : public basicxml
{
public:
    void parsecallback(element e)
    {
        std::string name = e.name;

        if ( !e.isClosing )
        {
            if ( loadingConfig )
            {
                if ( name == "showname" )
                {
                    for (auto ptr = e.atts; ptr != nullptr; ptr = ptr->next)
                    {
                        if ( std::string(ptr->name) == "enable" )
                        {
                            showName = std::stoi(ptr->value);
                        }
                    }
                }
                else if ( name == "showage" )
                {
                    for (auto ptr = e.atts; ptr != nullptr; ptr = ptr->next)
                    {
                        if ( std::string(e.atts->name) == "enable" )
                        {
                            showAge = std::stoi(e.atts->value);
                        }
                    }
                }
            }

            else if ( loadingData )
            {
                if ( name == "name" )
                {
                    myname = e.value;
                }
                else if ( name == "age" )
                {
                    myage = e.value;
                }
            }

            if ( name == "config" )
            {
                loadingConfig = true;
            }
            else if ( name == "data" )
            {
                loadingData = true;
            }
        }

        else
        {
            if ( name == "config" )
            {
                loadingConfig = false;

            }
            else if ( name == "data" )
            {
                loadingData = false;
            }
        }
    }

    int loadcallback(char* buffer, size_t buffsize)
    {
        int s = ifs.readsome(buffer, buffsize);
        return s;
    }

    mybasicxml()
    {
    loadingConfig = false;
    loadingData = false;

    showName = false;
    showAge = false;

    }

    std::ifstream ifs;
    bool loadingConfig;
    bool loadingData;

    bool showName;
    bool showAge;

    std::string myname;
    std::string myage;
};

int main()
{
    mybasicxml parser;

    parser.ifs.open("tests/test_config.xml");

    parser.parse();

    if ( parser.showName )
    {
        std::cout << "Hi! My name is " << parser.myname << ". ";
    }

    if ( parser.showAge )
    {
        std::cout << "I am " << parser.myage << " years old.";
    }
    
    std::cout << std::endl;

    return 0;
}