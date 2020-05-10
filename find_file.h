#pragma once

#include <cassert>
#include <cstdio>
#include <cstring>

#include <chrono>
#include <iostream>
#include <stack>


#define STRING_BUILDER_INITIAL_CAPACITY 256

struct string_builder {
    char* String;
    int Length;
    int Capacity;
};

void StringBuilderInit(string_builder* Builder, const char* String, int Length)
{
    Builder->Capacity = STRING_BUILDER_INITIAL_CAPACITY;
    while (Builder->Capacity < Length + 1)
    {
        Builder->Capacity *= 2;
    }

    Builder->String = (char*)malloc(Builder->Capacity);
    Builder->Length = Length;

    for (int I = 0; I < Length; I++)
    {
        Builder->String[I] = String[I];
    }
    Builder->String[Length] = 0;
}

void StringBuilderDrop(string_builder* Builder, int NumChars)
{
    Builder->Length -= NumChars;
    Builder->String[Builder->Length] = 0;
}

void StringBuilderCopy(string_builder* Dst, string_builder* Src)
{
    if (Dst->Capacity < Src->Length + 1)
    {
        while (Dst->Capacity < Src->Length + 1)
        {
            Dst->Capacity *= 2;
        }
        Dst->String = (char*)realloc(Dst->String, Dst->Capacity);
    }
    Dst->Length = Src->Length;
    for (int I = 0; I < Src->Length; I++)
    {
        Dst->String[I] = Src->String[I];
    }
    Dst->String[Dst->Length] = 0;
}

void StringBuilderAppend(string_builder* Builder, const char* String, int Length) 
{
    int NewLength = Builder->Length + Length;
    if (Builder->Capacity < NewLength + 1)
    {
        while (Builder->Capacity < NewLength + 1)
        {
            Builder->Capacity *= 2;
        }        
        Builder->String = (char*)realloc(Builder->String, Builder->Capacity);
    }

    for (int I = 0; I < Length; I++)
    {
        Builder->String[Builder->Length + I] = String[I];
    }
    Builder->Length = NewLength;
    Builder->String[Builder->Length] = 0;
}

void StringBuilderClear(string_builder* Builder)
{
    Builder->Length = 0;
    Builder->String[0] = 0;
}

#if defined(_WIN32)
#include <windows.h>
struct recursive_directory_iterator
{
    string_builder Path;

    HANDLE CurrentHandle;
    WIN32_FIND_DATAA FindData;

    std::stack<int> CharsAdded;
    std::stack<HANDLE> Handles;
};

bool RecursiveDirectoryIteratorNext(recursive_directory_iterator* Iterator)
{
    if (Iterator->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        if (strcmp(Iterator->FindData.cFileName, ".") != 0 && strcmp(Iterator->FindData.cFileName, "..") != 0)
        {
            int Length = strlen(Iterator->FindData.cFileName);
            StringBuilderAppend(&Iterator->Path, Iterator->FindData.cFileName, Length);
            StringBuilderAppend(&Iterator->Path, "\\*", 2);

            HANDLE NextFindHandle = FindFirstFileA(Iterator->Path.String, &Iterator->FindData);
            StringBuilderDrop(&Iterator->Path, 1);
            if (NextFindHandle != INVALID_HANDLE_VALUE)
            {
                Iterator->Handles.push(Iterator->CurrentHandle);
                Iterator->CharsAdded.push(Length + 1);
                Iterator->CurrentHandle = NextFindHandle;
                return true;
            }
            else
            {
                StringBuilderDrop(&Iterator->Path, Length + 1);
            }
        }
    }

    BOOL NextFile = FindNextFileA(Iterator->CurrentHandle, &Iterator->FindData);
    while (!NextFile)
    {
        if (Iterator->CharsAdded.size() == 0)
        {
            return false;
        }

        StringBuilderDrop(&Iterator->Path, Iterator->CharsAdded.top());
        Iterator->CharsAdded.pop();

        FindClose(Iterator->CurrentHandle);
        Iterator->CurrentHandle = Iterator->Handles.top();
        Iterator->Handles.pop();

        NextFile = FindNextFileA(Iterator->CurrentHandle, &Iterator->FindData);
    }
    return NextFile;
}

void RecursiveDirectoryIteratorStart(recursive_directory_iterator* Iterator, const char* Path, int Length)
{
    StringBuilderInit(&Iterator->Path, Path, Length);
    StringBuilderAppend(&Iterator->Path, "\\*", 2);

    Iterator->CurrentHandle = FindFirstFileA(Iterator->Path.String, &Iterator->FindData);

    StringBuilderDrop(&Iterator->Path, 1);
}
#elif defined(__linux__)
#include <dirent.h>
struct recursive_directory_iterator
{
    string_builder Path;

    DIR* CurrentDir;
    struct dirent* CurrentEntry;

    std::stack<int> CharsAdded;
    std::stack<DIR*> Dirs;
};

bool RecursiveDirectoryIteratorIsDir(recursive_directory_iterator* Iterator)
{
    return Iterator->CurrentEntry->d_type == DT_DIR;
}

const char* RecursiveDirectoryIteratorFileName(recursive_directory_iterator* Iterator)
{
    return Iterator->CurrentEntry->d_name;
}

bool RecursiveDirectoryIteratorNext(recursive_directory_iterator* Iterator)
{
    const char* FileName = RecursiveDirectoryIteratorFileName(Iterator);
    if (RecursiveDirectoryIteratorIsDir(Iterator))
    {
        
        if (strcmp(FileName, ".") != 0 && strcmp(FileName, "..") != 0)
        {
            int Length = strlen(FileName);
            StringBuilderAppend(&Iterator->Path, FileName, Length);

            DIR* NextDir = opendir(Iterator->Path.String);
            if (NextDir)
            {
                StringBuilderAppend(&Iterator->Path, "/", 1);
                Iterator->Dirs.push(Iterator->CurrentDir);
                Iterator->CharsAdded.push(Length + 1);
                Iterator->CurrentDir = NextDir;
                Iterator->CurrentEntry = readdir(Iterator->CurrentDir);
                return true;
            }
            else
            {
                StringBuilderDrop(&Iterator->Path, Length + 1);
            }
        }
    }

    struct dirent* NextEntry = readdir(Iterator->CurrentDir);
    while (!NextEntry)
    {
        if (Iterator->CharsAdded.size() == 0)
        {
            return false;
        }

        StringBuilderDrop(&Iterator->Path, Iterator->CharsAdded.top());
        Iterator->CharsAdded.pop();

        closedir(Iterator->CurrentDir);
        Iterator->CurrentDir = Iterator->Dirs.top();
        Iterator->Dirs.pop();

        NextEntry = readdir(Iterator->CurrentDir);
    }

    Iterator->CurrentEntry = NextEntry;

    return NextEntry != NULL;
}

void RecursiveDirectoryIteratorStart(recursive_directory_iterator* Iterator, const char* Path, int Length)
{
    StringBuilderInit(&Iterator->Path, Path, Length);
    StringBuilderAppend(&Iterator->Path, "/", 1);

    Iterator->CurrentDir = opendir(Iterator->Path.String);
    Iterator->CurrentEntry = readdir(Iterator->CurrentDir);
}

#endif



bool GlobMatch(const char* GlobExpr, const char* String)
{
    int GlobLen = strlen(GlobExpr);
    int StringLen = strlen(String);
    int Gx = 0;
    int Sx = 0;

    int RestartGx = 0;
    int RestartSx = 0;

    while (Gx < GlobLen || Sx < StringLen)
    {
        if (Gx < GlobLen)
        {
            char C = GlobExpr[Gx];
            if (C == '?') 
            {
                if (Sx < StringLen)
                {
                    Gx++;
                    Sx++;
                    continue;
                }
            }
            else if (C == '*')
            {
                // First assume the star matches nothing restart at the current indices
                // if that doesn't workout
                RestartGx = Gx;
                RestartSx = Sx + 1;
                Gx++;
                continue;
            }
            else
            {
                if (Sx < StringLen && String[Sx] == C)
                {
                    Gx++;
                    Sx++;
                    continue;
                }
            }
        }

        if (0 < RestartSx && RestartSx <= StringLen)
        {
            Gx = RestartGx;
            Sx = RestartSx;
            continue;
        }
        return false;
    }
    return true;
}

