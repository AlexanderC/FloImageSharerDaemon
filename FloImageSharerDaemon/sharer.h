//
//  sharer.h
//  FloImageSharer
//
//  Created by AlexanderC on 1/30/14.
//  Copyright (c) 2014 AlexanderC. All rights reserved.
//

#ifndef __FloImageSharer__sharer__
#define __FloImageSharer__sharer__

#ifdef __APPLE__
    #include <TargetConditionals.h>
    #define FLO_ON_MAC (TARGET_OS_MAC)
    #define FLO_ON_WIN 0
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
    #define FLO_ON_MAC 0
    #define FLO_ON_WIN 1
#else
    #define FLO_ON_MAC 0
    #define FLO_ON_WIN 0
#endif

#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <curl/curl.h>

#if FLO_ON_MAC
    #include <CoreServices/CoreServices.h>
#elif FLO_ON_WIN
    #include <windows.h>
    #include <stdlib.h>
    #include <tchar.h>
    #include <map>
#endif


using namespace std;

class Sharer {
protected:
    vector<char*> pathsRaw;
    
#if FLO_ON_MAC
    CFArrayRef pathsToWatch;
    FSEventStreamRef stream;
    CFTimeInterval latency; 
    FSEventStreamContext* context;
#elif FLO_ON_WIN
    map<char*, HANDLE> dwChangeHandles;
#endif
    bool started = false;
    bool binded = false;
    queue<string> sendQueue;
    char* serviceUrl;
    
public:
    Sharer(vector<char*> paths, char* url);
    ~Sharer();
    
public:
    void bind();
    void unbind();
    void start();
    void stop();
    void debugStream();
    void flushQueue();
    void flushQueueWhileNotEmpty();
    void pushAndFlush(string file);
#if FLO_ON_WIN
    void updateListener();
#endif
    
protected:
    void sendFileIfImage(string file);
    void showNotification(string title, string text);
    
    struct MatchPathSeparator
    {
        bool operator()(char ch) const
        {
#if FLO_ON_WIN
            return ch == '\';
#else
            return ch == '/';
#endif
        }
    };
    
    template <size_t N>
    bool in_array(const string needle, string (haystack)[N])
    {
        return find(haystack, haystack + N, needle) != haystack + N;
    }
};

#if FLO_ON_MAC
void manageChange(ConstFSEventStreamRef streamRef,
                 void *clientCallBackInfo,
                 size_t numEvents,
                 void *eventPaths,
                 const FSEventStreamEventFlags eventFlags[],
                 const FSEventStreamEventId eventIds[]);
#endif

#endif /* defined(__FloImageSharer__sharer__) */
