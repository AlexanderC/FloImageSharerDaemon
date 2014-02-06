//
//  main.c
//  FloImageSharer
//
//  Created by AlexanderC on 1/30/14.
//  Copyright (c) 2014 AlexanderC. All rights reserved.
//

#include <stdio.h>
#include <iostream>
#include <vector>
#include <csignal>
#include "sharer.h"

using namespace std;

Sharer* sharer;

void quitHandler(int signum) {
    printf("Signal intercepted: %d\n\n", signum);
    delete sharer;
}

int main(int argc, char *argv[])
{    
    vector<char*> paths;
    
    if(argc < 3) {
        throw "You may specify 2 arguments: Path and Service Url";
    }
    
    char* path = argv[1];
    char* url = argv[2];
    
    printf("Path: %s\n\n", path);
    printf("Url: %s\n\n", url);
    
    paths.push_back(path);
    
    sharer = new Sharer(paths, url);
    sharer->bind();
    sharer->start();

    // the Long Joe list...
    signal(SIGINT, quitHandler);
    signal(SIGABRT, quitHandler);
    signal(SIGBUS, quitHandler);
    signal(SIGFPE, quitHandler);
    signal(SIGHUP, quitHandler);
    signal(SIGILL, quitHandler);
    signal(SIGKILL, quitHandler);
    signal(SIGPIPE, quitHandler);
    signal(SIGQUIT, quitHandler);
    signal(SIGSEGV, quitHandler);
    signal(SIGSTOP, quitHandler);
    signal(SIGTERM, quitHandler);
    signal(SIGTSTP, quitHandler);
    signal(SIGTTIN, quitHandler);
    signal(SIGTTOU, quitHandler);
    signal(SIGUSR1, quitHandler);
    signal(SIGUSR2, quitHandler);
    signal(SIGPROF, quitHandler);
    signal(SIGSYS, quitHandler);
    signal(SIGTRAP, quitHandler);
    signal(SIGVTALRM, quitHandler);
    signal(SIGXCPU, quitHandler);
    signal(SIGXFSZ, quitHandler);
    
    while(true){
        sleep(1);
    }
    
    return 0;
}