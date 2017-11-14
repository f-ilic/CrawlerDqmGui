#include <iostream>
#include <fstream>
#include <vector>
#include <regex>
#include <stdio.h>

#include <curl/curl.h>
#include "dqmguicrawler.h"

using namespace std;

void printUsage() {
    cout << "Usage:" << endl;
    cout << "\t* ./CrawlDqmGui crawl  <flavor> <filename> cert=path/to/cert key=path/to/key" << endl;
    cout << "\t* ./CrawlDqmGui update <flavor> <filename> cert=path/to/cert key=path/to/key" << endl;
    cout << "\t* ./CrawlerDqmGui --help" << endl;
    cout << endl;
    cout << "Options for <flavor> are:" << endl;
    cout << "\t*  online"  << endl;
    cout << "\t*  offline" << endl;
    cout << "\t*  relval"  << endl;
    cout << endl;
}

void printHelp() {
    cout << endl;
    cout << "This tool is meant to create an index of all the files available through the DqmGui." << endl;
    cout << "It is also possible to incrementally update the file." << endl;
    cout << "The only prerequisit is that you have a valid GRID  certificate" << endl;
    cout << "     You can obtain such a grid certificate through https://ca.cern.ch/ca/" << endl;
    cout << endl;

    printUsage();
}

bool fileExists (const string& name) {
    ifstream f(name);
    return f.good();
}

int main(int argc, char *argv[])
{
    if(argc==2 && string(argv[1])=="--help") {
        printHelp();
        return 1;
    }

    if(argc!=6) {
        printHelp();
        return 1;
    }

    // ----- Check if the certificate and key pair are valid -----
    if(argc==6 && (string(argv[4]).substr(0,5) != "cert=" ||string(argv[5]).substr(0,4) != "key=")){
        printHelp();
        return 1;
    }

    string tmp = argv[4];
    string cert_path(tmp.substr(5,tmp.size()));

    tmp = argv[5];
    string key_path(tmp.substr(4,tmp.size()));

    DqmGuiCrawler crawler(cert_path, key_path);

    if(!crawler.isValidCurlConfiguration("https://cmsweb.cern.ch/dqm/online/data/browse/Original/")) {
        cout << "\nERROR:" << endl;
        cout << "  Could not GET from server." << endl;
        cout << "  Certificate and/or Key might be invalid.\n" << endl;
        return 1;
    }

    // ----- Flavor & Cmd parsing + error handling -----
    string argv1 = string(argv[1]);
    if(!(argv1=="crawl" || argv1=="update")) {
        cout << "Invalid Command" << endl;
        printUsage();
        return 1;
    }

    bool is_update_mode = (argv1=="update");

    string argv2 = string(argv[2]);
    if(!(argv2=="online" || argv2=="offline" || argv2=="relval")) {
        cout << "Invalid Flavor" << endl;
        printUsage();
        return 1;
    }

    FLAVOR flavor;
    if(argv2=="online")  flavor=FLAVOR::ONLINE;
    if(argv2=="offline") flavor=FLAVOR::OFFLINE;
    if(argv2=="relval")  flavor=FLAVOR::RELVAL;

    // ------ Check file thats being written to / updated. -----
    string file_name = string(argv[3]);
    if(!fileExists(file_name) && is_update_mode) {
        cout << "\nCannot call update on non existing file\n" << endl;
        return 1;
    }

    if(fileExists(file_name) && !is_update_mode) {
        cout << "You are about to override the file: " << file_name << endl;
        cout << "Continue [N/y] ";

        char answer;
        cin >> answer;
        if (!(answer == 'y' || answer == 'Y' )) {
            return 1;
        }
    }

    if(is_update_mode)
        crawler.update(flavor,file_name);
    else
        crawler.crawl(flavor, file_name);

    return 0;
}
