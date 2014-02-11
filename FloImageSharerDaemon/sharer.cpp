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
 
#if FLO_ON_MAC
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
#elif FLO_ON_WIN
    for(auto &pathRaw : this->pathsRaw) {
        TCHAR lpDrive[4];
        TCHAR lpFile[_MAX_FNAME];
        TCHAR lpExt[_MAX_EXT];
        LPTSTR lpDir = const_cast<LPTSTR> pathRaw;
        
        _tsplitpath_s(lpDir, this->lpDrive, 4, NULL, 0, lpFile, _MAX_FNAME, lpExt, _MAX_EXT);
        
        lpDrive[2] = (TCHAR)'\\';
        lpDrive[3] = (TCHAR)'\0';
        
        HANDLE handle = CreateFile(
                                   lpDir,
                                   FILE_LIST_DIRECTORY,
                                   FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE,
                                   NULL,
                                   OPEN_EXISTING,
                                   FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                   NULL
                                );
        
        if(handle == INVALID_HANDLE_VALUE || handle == NULL) {
            throw "Unable to initialize watcher handle";
        }
        
        this->dwChangeHandles[pathRaw] = handle;
    }
#endif
}

Sharer::~Sharer() {
    // flush all remaining items
    this->flushQueueWhileNotEmpty();
    
    this->stop();
    this->unbind();
#if FLO_ON_MAC
    FSEventStreamInvalidate(this->stream);
    FSEventStreamRelease(this->stream);
    delete this->context;
#elif FLO_ON_WIN
    for(auto & it: this->dwChangeHandles) {
        CloseHandle(it->second());
    }
    
    this->dwChangeHandles.clear();
#endif
    this->pathsRaw.clear();
    delete this->serviceUrl;
}

void Sharer::bind() {
    if(false == this->binded) {
#if FLO_ON_MAC
        FSEventStreamScheduleWithRunLoop(this->stream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
#endif
        this->binded = true;
    }
}

void Sharer::unbind() {
    if(true == this->binded) {
#if FLO_ON_MAC
        FSEventStreamUnscheduleFromRunLoop(this->stream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
#endif
        this->binded = false;
    }
}

void Sharer::start() {
    if(false == this->started) {
        // do not allow running loop without to bind
        if(false == this->binded) {
            throw "Normally you have to call Sharer::bind() before calling this.";
        }
#if FLO_ON_MAC
        FSEventStreamStart(this->stream);
        CFRunLoopRun();
#endif
        this->started = true;
    }
}

void Sharer::stop() {
    if(true == this->started) {
#if FLO_ON_MAC
        FSEventStreamStop(this->stream);
#endif
        this->started = false;
    }
}

void Sharer::debugStream() {
#if FLO_ON_MAC
    FSEventStreamShow(this->stream);
#else
    printf("Stream Debugging is implemented only on mac");
#endif
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
#if FLO_ON_MAC
    char* buffer = (char *) malloc(1024); // should be enough...
    
    sprintf(buffer,
            "osascript -e 'display notification \"%s\" with title \"%s\"'",
            text.c_str(),
            title.c_str()
            );
    
    system((const char*) buffer);
    delete buffer;
#else
    printf("User Notifications are implemented only on mac");
#endif
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

#if FLO_ON_MAC
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
        
        if((flags & kFSEventStreamEventFlagItemCreated)
           && (flags & kFSEventStreamEventFlagItemIsFile)) {
            
            string file = string(paths[i]);
            
            printf("Detected file creation: %s\n\n", file.c_str());
           
            Sharer* sharer = (Sharer*) clientCallBackInfo;
               
            sharer->pushAndFlush(file);
        }
    }
}
#elif FLO_ON_WIN
void Sharer::updateListener()
{
    OVERLAPPED o;
    o.hEvent = CreateEventA(0, true, false, 0);
    
    BYTE outBuffer[5120];
    VOID *pBuf = (BYTE*) &outBuffer;
    FILE_NOTIFY_INFORMATION InfoNotify1;
    DWORD InfoNotify;
    BOOL ResultReadChange;
    DWORD outSize = sizeof(outBuffer);
    DWORD timeout = 500;
    
    for(auto & it: this->dwChangeHandles) {
        HANDLE hDir = it->second();
        
        ZeroMemory(outBuffer,sizeof(outBuffer));
        
        ResultReadChange = ReadDirectoryChangesW(
                                                 hDir,
                                                 &outBuffer,
                                                 outSize,
                                                 TRUE,
                                                 FILE_NOTIFY_CHANGE_SIZE|
                                                 FILE_NOTIFY_CHANGE_DIR_NAME|
                                                 FILE_NOTIFY_CHANGE_FILE_NAME,
                                                 &InfoNotify,
                                                 &o,
                                                 NULL
                                            );
        
        if(WaitForSingleObject(o.hEvent, timeout) == WAIT_OBJECT_0) {
            PFILE_NOTIFY_INFORMATION fni = (PFILE_NOTIFY_INFORMATION) pBuf;
            
            AnsiString path = it->first();
            
			do {
				cbOffset = fni->NextEntryOffset;
                
				ZeroMemory(fni, sizeof(fni));
                
				if(fni->Action == FILE_ACTION_ADDED || fni->Action == FILE_ACTION_RENAMED_NEW_NAME) {
                    char ch[fni->FileNameLength];
                    char DefChar = ' ';
                    
                    WideCharToMultiByte(CP_ACP, 0, fni->FileName, -1, ch, fni->FileNameLength, &DefChar, NULL);
                    
                    string file = string(path) + string("\\") + string(ch);
                    
                    this->pushAndFlush(file);
                }
                
				fni = (PFILE_NOTIFY_INFORMATION)((LPBYTE) fni + cbOffset);
			} while(cbOffset);
        }
    }
}
#endif