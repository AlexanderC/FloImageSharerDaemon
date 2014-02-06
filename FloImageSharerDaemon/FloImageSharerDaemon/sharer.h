//
//  sharer.h
//  FloImageSharer
//
//  Created by AlexanderC on 1/30/14.
//  Copyright (c) 2014 AlexanderC. All rights reserved.
//

#ifndef __FloImageSharer__sharer__
#define __FloImageSharer__sharer__

#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <CoreServices/CoreServices.h>
#include <curl/curl.h>

using namespace std;

class Sharer {
protected:
    vector<char*> pathsRaw;
    CFArrayRef pathsToWatch;
    FSEventStreamRef stream;
    CFTimeInterval latency; 
    FSEventStreamContext* context;
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
    
protected:
    void sendFileIfImage(string file);
    void showNotification(string title, string text);
    
    struct MatchPathSeparator
    {
        bool operator()(char ch) const
        {
            return ch == '/';
        }
    };
    
    template<size_t N>
    bool in_array( const string needle, string (haystack)[N] )
    {
        return std::find(haystack, haystack + N, needle) != haystack + N;
    }
};

void manageChange(ConstFSEventStreamRef streamRef,
                 void *clientCallBackInfo,
                 size_t numEvents,
                 void *eventPaths,
                 const FSEventStreamEventFlags eventFlags[],
                 const FSEventStreamEventId eventIds[]);

#endif /* defined(__FloImageSharer__sharer__) */
