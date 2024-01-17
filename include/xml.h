#ifndef LIBQ_XML_H
#define LIBQ_XML_H
#pragma once
//typedef unsigned long int;
//typedef unsigned int uint;

struct attribute
{
    char *name;
    int namelen;
    char *value;
    int valuelen;

    attribute *next;
};

struct element
{
    char *name;
    int namelen;
    char *value;
    int valuelen;

    bool isClosing;
    bool isStandalone;
    bool isFirst;

    attribute *atts;
};

class basicxml
{
public:
    basicxml();
    ~basicxml(){};

    int parse();

    int buffersize;

private:
    struct
    {
        bool lastIteration : 1;
        bool isInsideBrackets : 1;
        bool isLoadingName : 1;
        bool isLoadingAtts : 1;
        bool isLoadingVal : 1;
        bool doCallback : 1;
        bool isComment : 1;
        bool LastCharWasSlash : 1;
        unsigned int findingComment : 2;
    } flags;

    virtual int loadcallback(char *, int) = 0;
    virtual void parsecallback(element) = 0;

    void parse_attributes(attribute *a, char *attstring, int length, element e);

    void run_parsecallback(element *e, char *attstring, int attslen);
    bool whitespace(char c);
};

#endif