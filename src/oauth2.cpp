#include <curl/curl.h>
#include <json-c/json.h>
#include <openssl/sha.h>
#include <memory>
#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <queue>
#include <microhttpd.h>
#include <mutex>

#define OAUTH2_URL_AUTH "https://accounts.google.com/o/oauth2/v2/auth"
#define OAUTH2_URL_TOKEN "https://oauth2.googleapis.com/token"
#define OAUTH2_REDIRECT_URI "http://127.0.0.1:8080/oauth2"
#define OAUTH2_CLIENT_ID "247614418461-pbr9ndvh3njg7ld8qj6jaog2emo6681a.apps.googleusercontent.com"
#define OAUTH2_CLIENT_SECRET "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"
#define OAUTH2_SCOPE "https://www.googleapis.com/auth/drive"
#define OAUTH2_REDIR_PAGE "<html><head><title>authorization successful</title></head><body>close this window to continue</body></html>"


std::string random_generate(unsigned int n) {
    std::string verifier_chars("-._~");
    for (char i = 'a'; i <= 'z'; i++) verifier_chars.push_back(i);
    for (char i = 'A'; i <= 'Z'; i++) verifier_chars.push_back(i);
    for (char i = '0'; i <= '9'; i++) verifier_chars.push_back(i);
    auto const verifier_chars_count = verifier_chars.size();
    auto ret = std::string();
    auto buf = new char[n];
    FILE *f = nullptr;
    f = fopen("/dev/urandom", "rb");
    if (f == nullptr) {
        goto error_exit_file;
    }
    if (fread(buf, 1, n, f) != n) {
        goto error_exit_file;
    }
    //todo: probabilities are not equal among characters.
    for (int i = 0; i < n; i++) {
        ret.push_back(verifier_chars[buf[i] % verifier_chars_count]);
    }
    error_exit_file:
    fclose(f);
    delete[] buf;
    return ret;
}

std::string base64_encode(const unsigned char *data, size_t n, bool url = false) {
    std::string base64_chars;
    for (char i = 'A'; i <= 'Z'; i++) base64_chars.push_back(i);
    for (char i = 'a'; i <= 'z'; i++) base64_chars.push_back(i);
    for (char i = '0'; i <= '9'; i++) base64_chars.push_back(i);
    base64_chars.append("+/");
    std::queue<unsigned char> q;
    for (size_t i = 0; i < n; i++) {
        q.push(data[i]);
    }
    int bits_read = 0;
    std::string ret;
    while (!q.empty()) {
        unsigned char c = q.front();
        auto t = (unsigned char) (((c << bits_read) & 0b11111100) >> 2);
        if (bits_read != 0) {
            q.pop();
            if (q.empty()) c = 0;
            else c = q.front();
        }
        bits_read = (bits_read + 6) % 8;
        if (bits_read != 6) {
            t = (unsigned char) (t | (c >> (8 - bits_read)));
        }
        ret.push_back(base64_chars[t]);
    }
    if (url) {
        for (auto &e: ret) {
            if (e == '+') e = '-';
            if (e == '/') e = '_';
        }
    } else {
        for (auto i = (4 - (ret.size() % 4)) % 4; i > 0; i--) {
            ret.push_back('=');
        }
    }
    return ret;
}

std::string base64_encode(const std::vector<unsigned char> &data, bool url = false) {
    size_t n = data.size();
    auto t = new unsigned char[n];
    for (size_t i = 0; i < n; i++) {
        t[i] = data[i];
    }
    auto r = base64_encode(t, n, url);
    delete[] t;
    return r;
}

std::string url_encode(const unsigned char *data, size_t n) {
    std::string ret;
    for (size_t i = 0; i < n; i++) {
        auto t = (unsigned char) data[i];
        if (('a' <= t && t <= 'z') || ('A' <= t && t <= 'Z') || ('0' <= t && t <= '9') || t == '-' || t == '_' ||
            t == '.' || t == '~') {
            ret.push_back((char) t);
        } else {
            char buf[10] = {0};
            sprintf(buf, "%%%x", t);
            ret.append(buf);
        }
    }
    return ret;
}

std::string url_encode(std::string data) {
    size_t n = data.size();
    auto t = new unsigned char[n];
    for (size_t i = 0; i < n; i++) {
        t[i] = data[i];
    }
    auto r = url_encode(t, n);
    delete[]t;
    return r;
}

std::string url_decode(std::string data){
    size_t n = data.size();
    std::string ret;
    for(int i = 0;i < n;i++){
        if(data[i] == '%'){
            if(i + 2 < n){
                auto m = strtol(data.substr(i+1,i+2).c_str(),nullptr,16);
                ret.push_back((char)m);
                i += 2;
            } else {
                return "";
            }
        } else {
            ret.push_back(data[i]);
        }
    }
    return ret;
}

std::vector<unsigned char> sha256_encode(const unsigned char *data, size_t n) {
    unsigned char buf[SHA256_DIGEST_LENGTH] = {0};
    SHA256(data, n, buf);
    return std::vector<unsigned char>(buf, buf + SHA256_DIGEST_LENGTH);
}

std::string querystring_build(const std::vector<std::pair<std::string, std::string>> &q) {
    std::string ret;
    for (auto &e: q) {
        ret.append(url_encode(e.first));
        ret.push_back('=');
        ret.append(url_encode(e.second));
        ret.push_back('&');
    }
    ret.pop_back();
    return ret;
}

std::vector<std::pair<std::string,std::string>> querystring_parse(const std::string& q){
    std::vector<std::pair<std::string,std::string>> ret;
    auto proc_pair = [&ret](std::string t){
        auto pos = t.find('=');
        ret.emplace_back(url_decode(t.substr(0,pos)),url_decode(t.substr(pos)));
    };
    int prev = 0;
    for(int i = 0;i < q.size();i++){
        if(q[i] == '&'){
            proc_pair(q.substr(prev,i));
        }
    }
    proc_pair(q.substr(prev));
    return ret;
}

struct parsed_url {
    std::string scheme,domain,path;
    uint16_t port;
    std::vector<std::pair<std::string,std::string>> query;
};
struct parsed_url url_parse(std::string url){
    auto n = url.size();
    struct parsed_url ret;
    auto a = url.find(':');
    if(a + 2 < n && url[a+1] != '/' || url[a+2] != '/'){
        return ret;
    }
    ret.scheme = url.substr(0,a);
    url = url.substr(a+2);
    auto b = url.find('?');
    if(b != std::string::npos) {
        ret.query = querystring_parse(url.substr(b+1));
        url = url.substr(0,b);
    }
    b = url.find('/');
    if(b != std::string::npos){
        ret.path = url.substr(b);
        url = url.substr(0,b);
    }
    b = url.find(':');
    if(b != std::string::npos){
        ret.port = (uint16_t)strtol(url.substr(b+1).c_str(),nullptr,10);
        url = url.substr(0,b);
    }
    ret.domain = url;
    //todo:implement user and password parse
    return ret;
}

std::pair<std::string, std::string>
oauth2_auth(const std::string &endpoint, const std::string &client_id, const std::string &redirect_uri,
            const std::string &scope) {
    std::string verifier = random_generate(128);
    std::string challenge = base64_encode(sha256_encode((const unsigned char *) verifier.c_str(), verifier.size()),
                                          true);
    std::string ret(endpoint);
    ret.push_back('?');
    std::vector<std::pair<std::string, std::string>> q;
    q.emplace_back("client_id", client_id);
    q.emplace_back("redirect_uri", redirect_uri);
    q.emplace_back("response_type", "code");
    q.emplace_back("scope", scope);
    q.emplace_back("code_challenge", challenge);
    q.emplace_back("code_challenge_method", "S256");
    ret.append(querystring_build(q));
    return std::make_pair(ret, verifier);
}

static std::string oauth_code = "";
static std::mutex oauth_code_mutex;

static enum MHD_Result
callback_handle(void *cls, struct MHD_Connection *conn, const char *url, const char *method, const char *version,
                const char *upload, size_t* upload_size, void **ptr) {
    struct MHD_Response *response;
    if (std::string("GET") != method) {
        return MHD_NO;
    }
    response = MHD_create_response_from_buffer(strlen(OAUTH2_REDIR_PAGE), (char *) OAUTH2_REDIR_PAGE, MHD_RESPMEM_PERSISTENT);
    auto r = MHD_queue_response(conn, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    const auto code = MHD_lookup_connection_value(conn,MHD_GET_ARGUMENT_KIND,"code");
    if(code != nullptr){
        oauth_code_mutex.lock();
        oauth_code = std::string(code);
        oauth_code_mutex.unlock();
    }
    return r;
}

std::string callback_get() {
    struct MHD_Daemon *d;
    d = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION, 8080, nullptr, nullptr, callback_handle, nullptr, MHD_OPTION_END);
    while(1){
        int l = 0;
        oauth_code_mutex.lock();
        l = oauth_code.size();
        oauth_code_mutex.unlock();
        if(l != 0) {
            break;
        }
    }
    MHD_stop_daemon(d);
    return oauth_code;
}

struct access_token {
    std::string access_token,refresh_token,scope;
    long expires_in;
};

std::string oauth2_token_data;

static size_t oauth2_token_callback(char* data,size_t size,size_t count, void* userdata){
    oauth2_token_data.append(data,size*count);
    return size*count;
}

struct access_token oauth2_token(std::string code,std::string verifier){
    struct access_token ret;
    CURL* curl = curl_easy_init();
    if(!curl){
        return ret;
    }
    std::vector<std::pair<std::string,std::string>> data;
    data.emplace_back("code",code);
    data.emplace_back("code_verifier",verifier);
    data.emplace_back("client_id",OAUTH2_CLIENT_ID);
    data.emplace_back("client_secret",OAUTH2_CLIENT_SECRET);
    data.emplace_back("redirect_uri",OAUTH2_REDIRECT_URI);
    data.emplace_back("grant_type","authorization_code");
    auto data_query = querystring_build(data);
    curl_easy_setopt(curl,CURLOPT_URL,OAUTH2_URL_TOKEN);
    curl_easy_setopt(curl,CURLOPT_POST,1);
    curl_easy_setopt(curl,CURLOPT_POSTFIELDS,data_query.c_str());
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,oauth2_token_callback);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    auto* result = json_tokener_parse(oauth2_token_data.c_str());
    const char* t = nullptr;
    t = json_object_get_string(json_object_object_get(result,"access_token"));
    if(t != nullptr){
        ret.access_token = std::string(t);
    }
    t = json_object_get_string(json_object_object_get(result,"refresh_token"));
    if(t != nullptr){
        ret.refresh_token = std::string(t);
    }
    t = json_object_get_string(json_object_object_get(result,"scope"));
    if(t != nullptr){
        ret.scope = std::string(t);
    }
    auto expire = json_object_get_int(json_object_object_get(result,"expires_in"));
    if(expire != 0){
        ret.expires_in = expire;
    }
    return ret;
}

int main() {
    auto auth = oauth2_auth(OAUTH2_URL_AUTH, OAUTH2_CLIENT_ID, OAUTH2_REDIRECT_URI, OAUTH2_SCOPE);
    std::cout << "Authorization URL: " << auth.first << "\nCode Verifier: " << auth.second <<  std::endl;
    auto code = callback_get();
    std::cout << "Authorization Code: " << code << std::endl;
    auto result= oauth2_token(code,auth.second);
    std::cout << "Access Token: " << result.access_token << std::endl;
    std::cout << "Refresh Token: " << result.refresh_token << std::endl;
    std::cout << "Scope: " << result.scope << std::endl;
    std::cout << "Expires in: " << result.expires_in << std::endl;
}

