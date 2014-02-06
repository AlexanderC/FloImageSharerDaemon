//
//  sharer.cpp
//  FloImageSharer
//
//  Created by AlexanderC on 1/30/14.
//  Copyright (c) 2014 AlexanderC. All rights reserved.
//

#include "sharer.h"

Sharer::Sharer (vector<char*> paths, char* url) {
    this->pathsRaw = paths;
    this->serviceUrl = url;
    
    this->latency = 1.0; /* Latency in seconds */
    
    FSEventStreamContext callbackInfo;
    callbackInfo.version = 0;
    callbackInfo.info = (void*) this;
    callbackInfo.retain = NULL;
    callbackInfo.release = NULL;
    callbackInfo.copyDescription = NULL;
    
    this->context = &callbackInfo;
    
    CFStringRef workingPaths[this->pathsRaw.size()];
    int i = 0;
    
    for(auto &pathRaw : this->pathsRaw) {
        workingPaths[i] = CFStringCreateWithCString(kCFAllocatorDefault, pathRaw, kCFStringEncodingUTF8);
        i++;
    }
    i = 0;
    
    this->pathsToWatch = CFArrayCreate(NULL, (const void **) &workingPaths, this->pathsRaw.size(), NULL);
    
    /*
     FSEventStreamCreate(
     CFAllocatorRef             allocator,
     FSEventStreamCallback      callback,
     FSEventStreamContext *     context,
     CFArrayRef                 pathsToWatch,
     FSEventStreamEventId       sinceWhen,
     CFTimeInterval             latency,
     FSEventStreamCreateFlags   flags)                           __OSX_AVAILABLE_STARTING(__MAC_10_5, __IPHONE_NA);
     */
    this->stream = FSEventStreamCreate(
                                       NULL,
                                       &manageChange,
                                       this->context,
                                       this->pathsToWatch,
                                       kFSEventStreamEventIdSinceNow, /* Or a previous event ID */
                                       this->latency,
                                       kFSEventStreamCreateFlagFileEvents
                                       );
}

Sharer::~Sharer() {
    // flush all remaining items
    this->flushQueueWhileNotEmpty();
    
    this->stop();
    this->unbind();
    FSEventStreamInvalidate(this->stream);
    FSEventStreamRelease(this->stream);
    this->pathsRaw.clear();
}

void Sharer::bind() {
    if(false == this->binded) {
        FSEventStreamScheduleWithRunLoop(this->stream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
        this->binded = true;
    }
}

void Sharer::unbind() {
    if(true == this->binded) {
        FSEventStreamUnscheduleFromRunLoop(this->stream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
        this->binded = false;
    }
}

void Sharer::start() {
    if(false == this->started) {
        // do not allow running loop without to bind
        if(false == this->binded) {
            throw "Normally you have to call Sharer::bind() before calling this.";
        }
        
        FSEventStreamStart(this->stream);
        this->started = true;
        
        CFRunLoopRun();
    }
}

void Sharer::stop() {
    if(true == this->started) {
        FSEventStreamStop(this->stream);
        this->started = false;
    }
}

void Sharer::debugStream() {
    FSEventStreamShow(this->stream);
}


void Sharer::sendFileIfImage(string file) {
    unsigned long lastDot = file.find_last_of(".");
    
    if(lastDot != string::npos) {
        const string extension = file.substr(lastDot + 1);
        
        printf("File extension: %s\n\n", extension.c_str());
        
        string allowedExtensions[7] = {
            "jpg", "jpeg", "png", "gif",
            "raw", "bmp", "tiff"
        };
        
        if(true == this->in_array <7>(extension, allowedExtensions)) {
            struct curl_httppost *post = NULL;
            struct curl_httppost *last = NULL;
            CURL* curlhash;
            CURLcode response;
            
            curlhash = curl_easy_init();
            curl_easy_setopt(curlhash, CURLOPT_URL, this->serviceUrl);
            
            curl_formadd(&post, &last,
                         CURLFORM_COPYNAME, "photo",
                         CURLFORM_FILE, file.c_str(),
                         CURLFORM_END
                         );
            curl_formadd(&post, &last,
                         CURLFORM_COPYNAME, "path",
                         CURLFORM_COPYCONTENTS, file.c_str(),
                         CURLFORM_END
                         );
            
            curl_easy_setopt(curlhash, CURLOPT_HTTPPOST, post);
            
            response = curl_easy_perform(curlhash);
            curl_formfree(post);
            curl_easy_cleanup(curlhash);
            
            if(response == 0) {
                printf("File successfully uploaded\n\n");
                
                this->showNotification(string("New photo was uploaded"), file + string(" was uploaded successfully."));
            } else {
                if(response != 26 /* READ ERROR, case file missing */) {
                    // push file for later send
                    this->sendQueue.push(file);
                }
                
                printf("An error occured, response code: %d\n\n", response);
            }
        }
    }
}

void Sharer::showNotification(string title, string text)
{
    char* buffer = (char *) malloc(255);
    
    sprintf(buffer,
            "osascript -e 'display notification \"%s\" with title \"%s\"'",
            text.c_str(),
            title.c_str()
            );
    
    system((const char*) buffer);
    delete buffer;
}

void Sharer::flushQueue() {
    int i;
    
    for(i = 0; i < this->sendQueue.size(); i++) {
        const string file = this->sendQueue.front();
        this->sendQueue.pop();
        
        this->sendFileIfImage(file);
    }
}

void Sharer::flushQueueWhileNotEmpty() {
    while (!this->sendQueue.empty()) {
        this->flushQueue();
    }
}

void Sharer::pushAndFlush(string file) {
    this->sendQueue.push(file);
    this->flushQueue();
}

void manageChange(ConstFSEventStreamRef streamRef,
                 void *clientCallBackInfo,
                 size_t numEvents,
                 void *eventPaths,
                 const FSEventStreamEventFlags eventFlags[],
                 const FSEventStreamEventId eventIds[]) {
    
    int i;
    char** paths = (char**) eventPaths;
    
    for (i = 0; i < numEvents; i++) {
        unsigned long flags = (unsigned long) eventFlags[i];
        
        /* flags are unsigned long, IDs are uint64_t */
        printf("Change %llu in %s, flags %lu\n\n", eventIds[i], paths[i], flags);
        
        if(/*(
                flags & kFSEventStreamEventFlagItemCreated
                || flags & kFSEventStreamEventFlagUserDropped
                || flags & kFSEventStreamEventFlagKernelDropped
           )
           && (flags & kFSEventStreamEventFlagItemIsFile)*/ true) { // TODO: find why is not working
            
            string file = string(paths[i]);
            
            printf("Detected file creation: %s\n\n", file.c_str());
           
            Sharer* sharer = (Sharer*) clientCallBackInfo;
               
            sharer->pushAndFlush(file);
        }
    }
}