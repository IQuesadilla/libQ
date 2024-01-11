#include "xml.h"

#include <iostream>


basicxml::basicxml()
{
    buffersize = 20;
}

void yayaya() {}

int basicxml::parse()
{
    int strbuffsize = 300;
    char strbuff[strbuffsize];
    // Would cause issues is attributes were too long
    element e;
    char *attstring = nullptr;
    int attslen = 0;

    flags.isInsideBrackets = false;
    flags.lastIteration = false;
    flags.doCallback = false;
    flags.LastCharWasSlash = false;
    flags.findingComment = 0;
    flags.isComment = 0;

    int strbuffit = 0, LastWhitespace = 0;
    while (!flags.lastIteration)
    {
        char buffer[buffersize];
        int size = loadcallback(buffer,buffersize);
        if (size < buffersize)
        {
            flags.lastIteration = true;
        }

        for (char *c = buffer; c != &buffer[size]; ++c)
        {
            if (flags.isInsideBrackets)
            {
                if (*c == '>')
                {
                    if (flags.isLoadingName)
                    {
                        e.atts = nullptr;
                    }

                    if (flags.LastCharWasSlash)
                    {
                        e.isClosing = false;
                        e.isStandalone = true;
                    }

                    flags.isLoadingName = false;
                    flags.isLoadingAtts = false;
                    flags.isInsideBrackets = false;
                }

                else if (*c == '/' || *c == '?')
                {
                    if ( flags.isLoadingName )
                    {
                        e.isClosing = true;
                    }
                    flags.LastCharWasSlash = true;
                }

                else if ( whitespace(*c) && flags.isLoadingName )
                {
                    flags.isLoadingName = false;
                    flags.LastCharWasSlash = false;
                }

                else
                {
                    flags.LastCharWasSlash = false;
                    if ( !flags.isLoadingName && !flags.isLoadingAtts )
                    {
                        strbuff[strbuffit++] = '\0';
                        attstring = &strbuff[strbuffit];
                        flags.isLoadingAtts = true;
                    }

                    strbuff[strbuffit++] = *c;

                    if (flags.isLoadingName)
                        ++e.namelen;
                    else if (flags.isLoadingAtts)
                        ++attslen;
                }
            }
            else // !flags.isInsideBrackets
            {
                if ( !flags.isComment )
                {
                    if ( *c == '<'  && flags.findingComment == 0 )
                    {
                        flags.findingComment = 1;
                    }

                    else if( flags.findingComment > 0 )
                    {
                        if (*c == '!' && flags.findingComment == 1)
                        {
                            flags.findingComment = 2;
                        }

                        else if (*c == '-' && flags.findingComment == 2)
                        {
                            flags.findingComment = 3;
                        }

                        else if (*c == '-' && flags.findingComment == 3)
                        {
                            flags.isComment = true;
                        }

                        else
                        {
                            if (flags.doCallback)
                            {
                                if ( e.isClosing )
                                    e.valuelen = 0;
                                
                                strbuff[strbuffit] = '\0';

                                run_parsecallback(&e,attstring,attslen);
                            }

                            flags.isInsideBrackets = true;
                            flags.isLoadingVal = false;
                            flags.isLoadingName = true;
                            flags.isLoadingAtts = false;
                            flags.doCallback = true;

                            e.name = strbuff;
                            e.namelen = 0;
                            e.value = nullptr;
                            e.valuelen = 0;
                            e.isClosing = false;
                            e.isStandalone = false;
                            e.isFirst = true;
                            strbuffit = 0;
                            attstring = nullptr;
                            attslen = 0;
                            LastWhitespace = 0;
                            flags.findingComment = 0;
                            --c;
                        }
                    }
                    else if ( !whitespace(*c) )
                    {
                        if (strbuffit > strbuffsize - 2)
                        {
                            int distance = strbuffit - LastWhitespace;
                            if (distance < 10) e.valuelen -= distance;
                            else distance = 0;

                            char prevchar = strbuff[strbuffit-distance];
                            strbuff[strbuffit-distance] = '\0';
                            run_parsecallback(&e,attstring,attslen);

                            strbuffit = e.namelen+1;
                            e.value = &strbuff[strbuffit];

                            strbuff[strbuffit++] = prevchar;
                            for (int it = 1; it < distance; it++)
                                strbuff[strbuffit++] = strbuff[LastWhitespace + it];

                            e.valuelen = distance;
                            e.atts = nullptr;
                            attstring = nullptr;
                            attslen = 0;
                            LastWhitespace = 0;
                        }

                        if (e.value == nullptr)
                        {
                            if (e.atts == nullptr)
                                strbuff[strbuffit++] = '\0';

                            e.value = &strbuff[strbuffit];
                        }
                        else
                        {
                            if (flags.isLoadingVal == false)
                            {
                                strbuff[strbuffit++] = ' ';
                                LastWhitespace = strbuffit;
                                ++e.valuelen;
                            }
                        }

                        strbuff[strbuffit++] = *c;
                        ++e.valuelen;

                        flags.isLoadingVal = true;
                    }

                    else
                    {
                        flags.isLoadingVal = false;
                    }
                }

                else // flags.isComment
                {
                    if ( *c == '-' && flags.findingComment == 3 )
                    {
                        flags.findingComment = 2;
                    }

                    else if ( *c == '-' && flags.findingComment == 2 )
                    {
                        flags.findingComment = 1;
                    }

                    else if ( *c == '>' && flags.findingComment == 1 )
                    {
                        flags.findingComment = 0;
                        flags.isComment = false;
                    }
                }
            }
        }
    }
    
    strbuff[strbuffit] = '\0';
    e.valuelen = 0;
    run_parsecallback(&e,attstring,attslen);

    yayaya();

    return 0;
}

void basicxml::parse_attributes(attribute *a, char *attstring, int length, element e)
{
    int i = 0;
    attribute newa;
    bool lastit = true;

    for (; i < length; ++i)
    {
        if ( !whitespace(attstring[i]) )
        {
            newa.name = &attstring[i];
            lastit = false;
            break;
        }
    } ++i;

    if (lastit)
    {
        parsecallback(e);
        return;
    }

    for (; i < length; ++i )
    {
        if ( attstring[i] == '=' )
        {
            newa.namelen = &attstring[i] - newa.name;
            attstring[i] = '\0';
            break;
        }
    } ++i;

    for (; i < length; ++i )
    {
        if ( attstring[i] == '\"' )
        {
            newa.value = &attstring[i+1];
            break;
        }
    } ++i;

    for (; i < length; ++i )
    {
        if ( attstring[i] == '\"' )
        {
            newa.valuelen = &attstring[i] - newa.value;
            attstring[i] = '\0';
            break;
        }
    } ++i;

    newa.next = nullptr;

    if (a == nullptr)
    {
        e.atts = &newa;
    }
    else
    {
        a->next = &newa;
    }

    parse_attributes(&newa,&attstring[i],length - i, e);
}

void basicxml::run_parsecallback(element *e, char *attstring, int attslen)
{
    char null = '\0';
    if (e->valuelen == 0)
    {
        e->value = &null;
    }

    if ( attstring == nullptr )
    {
        parsecallback(*e);
    }
    else
    {
        parse_attributes(nullptr,attstring,attslen,*e);
    }
    e->isFirst = false;
}

bool basicxml::whitespace(char c)
{
    if (
            c == ' '  ||
            c == '\r' ||
            c == '\n'
        )
        return true;
    return false;
}