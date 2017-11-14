#ifndef DQMGUICRAWLER_H
#define DQMGUICRAWLER_H

#include <iostream>
#include <fstream>
#include <vector>
#include <regex>
#include <stdio.h>

#include <curl/curl.h>

using namespace std;

enum class FLAVOR {
    ONLINE,
    OFFLINE,
    RELVAL
};

class DqmGuiCrawler {
public:
    DqmGuiCrawler(const string& cert_path, const string& key_path);
    ~DqmGuiCrawler();
    bool update(FLAVOR flavor, const string& file);
    bool crawl (FLAVOR flavor, const string& file);
    bool isValidCurlConfiguration(const string& url);

private:
    vector<string> split_string(const string& str, const string& delimiter);
    void crawl_root_files_recursive(const string& url, vector<string>& root_files);
    void crawl_links_on_url(const string& url, vector<string>& out_links_on_page);
    void update_online(const string& file_path);
    void vector_to_file(const vector<string>& v, const string& filename);

    static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp);

    string m_certificate_path;
    string m_key_path;
    CURL* curl;
};

#endif // DQMGUICRAWLER_H
