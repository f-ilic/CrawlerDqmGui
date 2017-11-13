#include <iostream>
#include <fstream>
#include <vector>
#include <regex>
#include <stdio.h>

#include <curl/curl.h>

#define FLAVOR_INVALID 0
#define FLAVOR_ONLINE  1
#define FLAVOR_OFFLINE 2
#define FLAVOR_RELVAL  3

#define CMD_INVALID 0
#define CMD_CRAWL   1
#define CMD_UPDATE  2


using namespace std;

static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

vector<string> split_string(const string& str, const string& delimiter)
{
    vector<string> strings;
    string::size_type pos = 0;
    string::size_type prev = 0;
    while ((pos = str.find(delimiter, prev)) != string::npos)
    {
        strings.push_back(str.substr(prev, pos - prev));
        prev = pos + 1;
    }

    strings.push_back(str.substr(prev));

    return strings;
}

void crawl_root_files_recursive(CURL* curl, const string& url, vector<string>& root_files) {
    string read_buffer;

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    auto res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        return;
    }

    vector<string>lines = split_string(read_buffer, "<tr>");

    regex rgx_link("<td><a href='(.*)'>");
    regex rgx_root_file("<td><a href='(.*).root'>");
    regex rgx_zip_file("<td><a href='(.*).zip'>");

    smatch link_match;

    for(auto& line : lines) {
        if (regex_search(line, link_match, rgx_link)) { // is it a

            smatch filetype_match;

            if (regex_search(line, filetype_match, rgx_root_file)){         // ... link to a <FILE>.root
                //                cout << ".ROOT: "<<  filetype_match[1] << endl;
                root_files.push_back("https://cmsweb.cern.ch" + string(filetype_match[1]) + ".root");

            } else if(regex_search(line, filetype_match, rgx_zip_file)) {    // ... link to a <FILE>.zip
                // uhmmmm....

            } else {                                                        // ... hyperlink to other website
                cout << "Crawling in " << link_match[1] << endl;
                crawl_root_files_recursive(curl, "https://cmsweb.cern.ch" + string(link_match[1]), root_files);
            }
        }
    }
}


void crawl_links_on_url(CURL* curl,const string& url, vector<string>& out_links_on_page) {
    string read_buffer;

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    auto res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        return;
    }

    vector<string>lines = split_string(read_buffer, "<tr>");

    regex rgx_link("<td><a href='(.*)'>");

    smatch link_match;

    for(auto& line : lines) {
        if (regex_search(line, link_match, rgx_link)) { // is it a hyperlink to other website
            out_links_on_page.push_back("https://cmsweb.cern.ch"  + string(link_match[1]));
        }
    }
}

void vector_to_file(const vector<string>& v, const string& filename) {
    ofstream online_out_file(filename);

    for(auto elem : v) {
        online_out_file << elem << endl;
    }
    online_out_file.close();
}


// The reason there is update_online instead of one update functoin
// that can handle arbitrary urls(online/offline/relval) is that
// we know a lot about the folder structure in onlne and want to
// keep the queries as few as possible so we don't ddos the
// dqmgui.
void update_online(CURL* curl, const string& file_path) {
    // find the last run number in the file_path
    ifstream infile(file_path);
    string line;

    vector<string> current_online;
    while(infile >> line) {
        current_online.push_back(line);
    }

    infile.close();

    string first_line = current_online[0];
    cout << first_line << endl;

    regex rgx_run_number("(.*)\[0-9]+xx/");
    regex rgx_dir_number("(.*)\[0-9]+xxxx/");
    smatch match;
    string run_nr;
    string dir_nr;

    if(regex_search(first_line, match, rgx_run_number)) {
        run_nr = match[0];
    }

    if(regex_search(first_line, match, rgx_dir_number)) {
        dir_nr = match[0];
    }

    string new_file_path = file_path + "_new";

    ofstream new_file(new_file_path);

    // ---------------- files that are in different folder than current file ----------------
    vector<string> higher_folders;
    crawl_links_on_url(curl, dir_nr, higher_folders);

    auto highest_run_in_updated = find(higher_folders.begin(), higher_folders.end(), run_nr); // find the first line in the newly crawled vector

    if (highest_run_in_updated == higher_folders.end()) {
        cout << "This should not happen!! Element NOT found. Aborting update" << endl;
        return;
    }

    vector<string> higher_folder_root_files;
    for(int i = 0; i < distance(higher_folders.begin(), highest_run_in_updated); i++) {
        cout << "[Update] Crawling in " << higher_folders[i] << endl;
        crawl_root_files_recursive(curl, higher_folders[i], higher_folder_root_files);
    }

    // ---------------- same folder as highest run nr before update ----------------
    vector<string> tmp;
    cout << "[Update] Crawling in " << run_nr << endl;
    crawl_root_files_recursive(curl, run_nr, tmp);


    auto it = find(tmp.begin(), tmp.end(), first_line); // find the first line in the newly crawled vector

    if (it == tmp.end()) {
        cout << "This should not happen!! Element NOT found. Aborting update" << endl;
        return;
    }

    // ---------------- Write new file ----------------
    cout << "Writing update file...";

    for(auto e : higher_folder_root_files) {
        new_file << e << endl;
    }

    for(int i = 0; i < distance(tmp.begin(), it); i++) {
        new_file << tmp[i] << endl;
    }

    cout << " Done!" << endl;


    // ---------------- Contatinate Updated list with old file ----------------
    cout << "Writing final file...";

    ifstream old_file(file_path);
    new_file << old_file.rdbuf();
    new_file.close();

    old_file.close();
    remove(file_path.c_str());

    rename(new_file_path.c_str(), file_path.c_str());

    cout << " Done!" << endl;
}

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

bool isValidCurlConfiguration(CURL* curl, const string& url) {
    string read_buffer;

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &read_buffer);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    auto res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        return false;
    } else {
        return true;
    }
}

// moms spaghetti...
int main(int argc, char *argv[])
{
    if(argc==2 && string(argv[1])=="--help") {
        printHelp();
        return 1;
    }

    if(argc!=6) {
        printUsage();
        return 1;
    }

    // ----- Check if the certificate and key pair are valid -----
    if(argc==6 && (string(argv[4]).substr(0,5) != "cert=" ||string(argv[5]).substr(0,4) != "key=")){
        printUsage();
        return 1;
    }

    string tmp = argv[4];
    string cert_path(tmp.substr(5,tmp.size()));

    tmp = argv[5];
    string key_path(tmp.substr(4,tmp.size()));

    CURL* curl;
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
    curl_easy_setopt(curl, CURLOPT_SSLCERT, cert_path.c_str());
    curl_easy_setopt(curl, CURLOPT_SSLKEY, key_path.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);

    if(!isValidCurlConfiguration(curl, "https://cmsweb.cern.ch/dqm/online/data/browse/Original/")) {
        cout << "\nERROR:" << endl;
        cout << "  Could not GET from server." << endl;
        cout << "  Certificate and/or Key might be invalid.\n" << endl;
        return 1;
    }

    // ----- Flavor & Cmd parsing + error handling -----
    int cmd=CMD_INVALID;
    if(string(argv[1])=="crawl")  cmd=CMD_CRAWL;
    if(string(argv[1])=="update") cmd=CMD_UPDATE;
    if(cmd==CMD_INVALID) {
        cout << "Invalid Command" << endl;
        printUsage();
        return 1;
    }

    int flavor=FLAVOR_INVALID;
    if(string(argv[2])=="online")  flavor = FLAVOR_ONLINE;
    if(string(argv[2])=="offline") flavor = FLAVOR_OFFLINE;
    if(string(argv[2])=="relval")  flavor = FLAVOR_RELVAL;
    if(flavor==FLAVOR_INVALID) {
        cout << "Invalid Flavor" << endl;
        printUsage();
        return 1;
    }
    
    // ------ Check file thats being written to / updated. -----
    string file_name(argv[3]);
    if(!fileExists(file_name) && cmd==CMD_UPDATE) {
        cout << "\nCannot call update on non existing file\n" << endl;
        return 1;
    }

    if(fileExists(file_name) && cmd==CMD_CRAWL) {
        cout << "You are about to override the file: " << file_name << endl;
        cout << "Continue [N/y] ";

        char answer;
        cin >> answer;
        if (!(answer == 'y' || answer == 'Y' )) {
            return 1;
        }
    }

    // ----- set urls to crawl based on flavor -----
    string to_crawl;
    switch(flavor) {
    case FLAVOR_ONLINE:  to_crawl = "https://cmsweb.cern.ch/dqm/online/data/browse/Original/00030xxxx/0003064xx/"; break;
    case FLAVOR_OFFLINE: to_crawl = "https://cmsweb.cern.ch/dqm/offline/data/browse/"; break;
    case FLAVOR_RELVAL:  to_crawl = "https://cmsweb.cern.ch/dqm/relval/data/browse/"; break;
    }

    // ------ Setup done: Call the appropriate method -----

    if(cmd==CMD_UPDATE) {
        switch(flavor) {
        case FLAVOR_ONLINE:  update_online(curl, file_name); break;
        case FLAVOR_OFFLINE: break;
        case FLAVOR_RELVAL:  break;
        }
    }

    if(cmd==CMD_CRAWL) {
        vector<string> out_vec;
        crawl_root_files_recursive(curl, to_crawl, out_vec);
        vector_to_file(out_vec, file_name);

    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 0;
}

