#include <iostream>
#include <fstream>
#include <vector>
#include <regex>
#include <stdio.h>

#include <curl/curl.h>

using namespace std;

struct config {

    config(const char* cert, const char* key) { 
	this->certificate_path = string(cert);
	this->key_path         = string(key);
    }

    string certificate_path;
    string key_path;

    void printDebug() {
        cout << "Certificate: " << certificate_path << endl;
        cout << "Key        : " << key_path << endl;
    }
};

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
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

void crawl_root_files_recursive(CURL* curl, string url, vector<string>& root_files) {
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


void crawl_links_on_url(CURL* curl, string url, vector<string>& out_links_on_page) {
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


void update_online(CURL* curl, const string& url, const string& file_path) {
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


int main(int argc, char *argv[])
{
    config cfg("./data/proxy.cert", "./data/proxy.cert");
    CURL* curl;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
    curl_easy_setopt(curl, CURLOPT_SSLCERT, cfg.certificate_path.c_str());
    curl_easy_setopt(curl, CURLOPT_SSLKEY, cfg.key_path.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

    vector<string> l1;
    crawl_root_files_recursive(curl, "https://cmsweb.cern.ch/dqm/relval/data/browse/", l1);
    vector_to_file(l1, "./data/relval.txt");

//    update_online(curl, "https://cmsweb.cern.ch/dqm/online/data/browse/Original", "/home/fil/projects/CrawlerDqmGui/online.txt");

    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return 0;
}
